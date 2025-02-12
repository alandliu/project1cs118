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
    print("Initiating handshake.");
    srand(time(NULL));
    char buf[sizeof(packet) + MAX_PAYLOAD] = {0};
    packet* syn_pkt = (packet*) buf;
    syn_pkt->flags = SYN;
    syn_pkt->seq = (int16_t) rand();
    char message[MAX_PAYLOAD] = {0};
    int n = input_io((uint8_t*)message, MAX_PAYLOAD);
    if (n > 0) {
        memcpy(syn_pkt->payload, message, n);
        memcpy(syn_pkt->payload, message, strlen(message));
        syn_pkt->length = strlen(message);
    }
    sendto(sockfd, syn_pkt, sizeof(packet) + MAX_PAYLOAD, 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    print("Syn sent");
    
    // nonblock socket
    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);
    listen_loop(sockfd, &server_addr, CLIENT, input_io, output_io);

    return 0;
}
