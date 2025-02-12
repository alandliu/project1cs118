#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "io.h"
#include "consts.h"

// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in* addr, int type,
                 ssize_t (*input_p)(uint8_t*, size_t),
                 void (*output_p)(uint8_t*, size_t)) {

    while (true) {
        uint8_t buffer[1024];
        socklen_t addr_size = sizeof(struct sockaddr_in);

        char buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet* p = (packet*) buf;
        int bytes_recvd = recvfrom(sockfd, p, sizeof(packet) + MAX_PAYLOAD,
                                    0, (struct sockaddr*) addr, 
                                    &addr_size);
        if (bytes_recvd > 0) {
            printf("SYN: %d\n", p->flags & 1);
            printf("ACK: %d\n", (p->flags >> 1) & 1);
            printf("PAR: %d\n", (p->flags >> 2) & 1);
            printf("PACKET SIZE OF: %d\n", p->length);
            print_diag(p, RECV);
            output_p(p->payload, p->length);
        }

        int n = input_p(buffer, 1024);
        if (n > 0) {
            char buf[sizeof(packet) + MAX_PAYLOAD] = {0};
            packet* pkt = (packet*) buf;
            pkt->flags = SYN;
            const char* msg = "Hello Mr. Server";
            pkt->length = 17;
            memcpy(pkt->payload, msg, pkt->length);
            ssize_t did_send = sendto(sockfd, pkt, sizeof(packet) + MAX_PAYLOAD,
                                        0, (struct sockaddr*) addr,
                                        addr_size);
        }
    }
}
