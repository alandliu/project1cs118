#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include "io.h"
#include "consts.h"

// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in* addr, int type,
                 ssize_t (*input_p)(uint8_t*, size_t),
                 void (*output_p)(uint8_t*, size_t)) {

    // initiate connection
    three_way_hs(sockfd, addr, type, input_p, output_p);

    // unblock the socket
    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);


    // loop
    while (true) {
        uint8_t buffer[sizeof(packet) + MAX_PAYLOAD] = {0};
        socklen_t addr_size = sizeof(struct sockaddr_in);

        int bytes_recvd = recvfrom(sockfd, buffer, 1024,
                                    0, (struct sockaddr*) addr, 
                                    &addr_size);
        packet* rcv_packet = (packet*) buffer;
        if (bytes_recvd > 0) {
            print_diag(rcv_packet, RECV);
            // ensure valid SEQ
            output_p(rcv_packet->payload, ntohs(rcv_packet->length));
            // if received ack > expected store in buffer
            // else if received ack < expected ack, retransmit
        }

        memset(buffer, 0, sizeof(packet) + MAX_PAYLOAD);
        packet* send_pkt = (packet*) buffer;
        int n = input_p(buffer, 1024);
        if (n > 0) {
            sendto(sockfd, buffer, n, 0, 
                (struct sockaddr*) addr, addr_size);
        }
    }
}
