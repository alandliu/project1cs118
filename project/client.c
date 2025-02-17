#include "consts.h"
#include "io.h"
#include "transport.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: client <hostname> <port> \n");
        exit(1);
    }

    /* Create sockets */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    // use IPv4  use UDP

    /* Construct server address */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET; // use IPv4
    // Only supports localhost as a hostname, but that's all we'll test on
    char* addr = strcmp(argv[1], "localhost") == 0 ? "127.0.0.1" : argv[1];
    server_addr.sin_addr.s_addr = inet_addr(addr);
    // Set sending port
    int PORT = atoi(argv[2]);
    server_addr.sin_port = htons(PORT); // Big endian

    // Client initiates three-way handshake
    init_io();
    // print("Initiating handshake.");
    srand(time(NULL));
    char send_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
    packet* syn_pkt = (packet*) send_buf;
    syn_pkt->flags = SYN;
    syn_pkt->seq = htons(rand() % (1 << 10));
    char message[MAX_PAYLOAD] = {0};
    int n = input_io((uint8_t*)message, MAX_PAYLOAD);
    if (n > 0) {
        memcpy(syn_pkt->payload, message, n);
        syn_pkt->length = htons(n);
    }
    print_diag(syn_pkt, SEND);
    sendto(sockfd, syn_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
            (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    // print("Syn sent");

    // wait for syn-ack
    // print("Awaiting SYN-ACK");
    char recv_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
    socklen_t s = sizeof(struct sockaddr_in);
    int bytes_recvd = recvfrom(sockfd, &recv_buf, sizeof(recv_buf), 0, (struct sockaddr*) &server_addr, &s);
    packet* recv_sa_pkt = (packet*) recv_buf;
    // print("Received SYN-ACK");
    print_diag(recv_sa_pkt, RECV);
    output_io(recv_sa_pkt->payload, ntohs(recv_sa_pkt->length));

    // send first ACK packet, with or without data
    memset(send_buf, 0, sizeof(packet) + MAX_PAYLOAD);
    packet* ack_pkt = (packet*) send_buf;
    ack_pkt->flags = ACK;
    ack_pkt->seq = htons(ntohs(recv_sa_pkt->ack));
    ack_pkt->ack = htons(ntohs(recv_sa_pkt->seq) + 1);
    n = input_io((uint8_t*)message, MAX_PAYLOAD);
    if (n > 0) {
        memcpy(ack_pkt->payload, message, n);
        ack_pkt->length = htons(n);
    } else if (n == 0) {
        ack_pkt->seq = htons(0);
    }
    print_diag(syn_pkt, SEND);
    sendto(sockfd, ack_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
            (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));

    // nonblock socket
    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);
    listen_loop(sockfd, &server_addr, CLIENT, input_io, output_io);

    return 0;
}
