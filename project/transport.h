#pragma once

#include <stdint.h>
#include <unistd.h>

// commences and facilitates the three way handshake
void three_way_hs(int sockfd, struct sockaddr_in* addr, int type,
                    ssize_t (*input_p)(uint8_t*, size_t),
                    void (*output_p)(uint8_t*, size_t),
                    int16_t* next_seq, int16_t* cum_ack);

// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in* addr, int type,
                 ssize_t (*input_p)(uint8_t*, size_t),
                 void (*output_p)(uint8_t*, size_t));
