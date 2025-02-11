#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include "io.h"
#include "consts.h"

// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in* addr, int type,
                 ssize_t (*input_p)(uint8_t*, size_t),
                 void (*output_p)(uint8_t*, size_t)) {

    while (true) {
        uint8_t buffer[1024];
        socklen_t addr_size = sizeof(struct sockaddr_in);

        int bytes_recvd = recvfrom(sockfd, buffer, 1024,
                                    0, (struct sockaddr*) addr, 
                                    &addr_size);

        if (bytes_recvd > 0) {
            output_p(buffer, bytes_recvd);
        }

        int n = input_p(buffer, 1024);
        if (n > 0) {
            ssize_t did_send = sendto(sockfd, buffer, n,
                                        0, (struct sockaddr*) addr,
                                        addr_size);
        }
    }
}
