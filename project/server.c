#include "consts.h"
#include "io.h"
#include "transport.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: server <port>\n");
        exit(1);
    }

    /* Create sockets */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // use IPv4  use UDP

    /* Construct our address */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET; // use IPv4
    server_addr.sin_addr.s_addr =
        INADDR_ANY; // accept all connections
                    // same as inet_addr("0.0.0.0")
                    // "Address string to network bytes"
    // Set receiving port
    int PORT = atoi(argv[1]);
    server_addr.sin_port = htons(PORT); // Big endian

    /* Let operating system know about our config */
    int did_bind =
        bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));

    struct sockaddr_in client_addr; // Same information, but about client
    socklen_t s = sizeof(struct sockaddr_in);
    char buffer;

    // Wait for client connection
    // the cumulative ACK before entering the listen loop is the SEQ of the last received packet
    init_io();
    srand(time(NULL));
    // print("Waiting for client...");
    char buf[sizeof(packet) + MAX_PAYLOAD] = {0};
    int syn_rcvd = 0;
    packet* recv_syn = (packet*) buf;
    while (!syn_rcvd) {
        int bytes_recvd = recvfrom(sockfd, &buf, sizeof(packet) + MAX_PAYLOAD, 0,
                               (struct sockaddr*) &client_addr, &s);
        if (bytes_recvd < 0) exit(1);
        if (recv_syn->flags & 1) syn_rcvd = 1;
    }
    char* client_ip = inet_ntoa(client_addr.sin_addr);
    int client_port = ntohs(client_addr.sin_port);
    // print("Client found!");
    print_diag(recv_syn, RECV);
    output_io(recv_syn->payload, ntohs(recv_syn->length));

    // send syn-ack
    char send_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
    packet* syn_ack_pkt = (packet*) send_buf;
    syn_ack_pkt->flags = SYN + ACK;
    syn_ack_pkt->seq = htons(rand() % (1 << 10));
    syn_ack_pkt->ack = htons(ntohs(recv_syn->seq) + 1);
    char message[MAX_PAYLOAD] = {0};
    int n = input_io((uint8_t*)message, MAX_PAYLOAD);
    if (n > 0) {
        memcpy(syn_ack_pkt->payload, message, n);
        syn_ack_pkt->length = htons(n);
    }
    print_diag(syn_ack_pkt, SEND);
    int16_t initial_seq = ntohs(syn_ack_pkt->seq) + 1;
    int16_t cum_ack = ntohs(recv_syn->seq);
    sendto(sockfd, syn_ack_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
            (struct sockaddr*) &client_addr, sizeof(struct sockaddr_in));
    // print("SYN-ACK sent");

    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);
    listen_loop(sockfd, &client_addr, SERVER, input_io, output_io);

    return 0;
}
