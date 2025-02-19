#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include "io.h"
#include "consts.h"

void three_way_hs(int sockfd, struct sockaddr_in* addr, int type,
    ssize_t (*input_p)(uint8_t*, size_t), void (*output_p)(uint8_t*, size_t), 
    int16_t* next_seq, int16_t* cum_ack) {
        srand(time(NULL));
        if (type == CLIENT) {
            char send_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
            packet* syn_pkt = (packet*) send_buf;
            syn_pkt->flags = SYN;
            syn_pkt->seq = htons(rand() % (1 << 10));
            char message[MAX_PAYLOAD] = {0};
            int n = input_p((uint8_t*)message, MAX_PAYLOAD);
            if (n > 0) {
                memcpy(syn_pkt->payload, message, n);
                syn_pkt->length = htons(n);
            }
            print_diag(syn_pkt, SEND);
            sendto(sockfd, syn_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
                    (struct sockaddr*) addr, sizeof(struct sockaddr_in));

            // wait for syn-ack
            char recv_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
            socklen_t s = sizeof(struct sockaddr_in);
            int bytes_recvd = recvfrom(sockfd, &recv_buf, sizeof(recv_buf), 0, (struct sockaddr*) addr, &s);
            packet* recv_sa_pkt = (packet*) recv_buf;
            print_diag(recv_sa_pkt, RECV);
            output_p(recv_sa_pkt->payload, ntohs(recv_sa_pkt->length));

            // send first ACK packet, with or without data
            memset(send_buf, 0, sizeof(packet) + MAX_PAYLOAD);
            packet* ack_pkt = (packet*) send_buf;
            ack_pkt->flags = ACK;
            ack_pkt->seq = htons(ntohs(recv_sa_pkt->ack));
            ack_pkt->ack = htons(ntohs(recv_sa_pkt->seq) + 1);
            n = input_p((uint8_t*)message, MAX_PAYLOAD);
            if (n > 0) {
                memcpy(ack_pkt->payload, message, n);
                ack_pkt->length = htons(n);
            } else if (n == 0) {
                ack_pkt->seq = htons(0);
            }
            print_diag(ack_pkt, SEND);
            sendto(sockfd, ack_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
                    (struct sockaddr*) addr, sizeof(struct sockaddr_in));
            *next_seq = ntohs(ack_pkt->seq) + 1; // next seq number to send
            *cum_ack = ntohs(recv_sa_pkt->seq); // last received ack
        } else if (type == SERVER) {
            char buf[sizeof(packet) + MAX_PAYLOAD] = {0};
            int syn_rcvd = 0;
            socklen_t s = sizeof(struct sockaddr_in);
            packet* recv_syn = (packet*) buf;
            while (!syn_rcvd) {
                int bytes_recvd = recvfrom(sockfd, &buf, sizeof(packet) + MAX_PAYLOAD, 0,
                                       (struct sockaddr*) addr, &s);
                if (bytes_recvd < 0) exit(1);
                syn_rcvd = recv_syn->flags & 1;
            }
            char* client_ip = inet_ntoa(addr->sin_addr);
            int client_port = ntohs(addr->sin_port);
            print_diag(recv_syn, RECV);
            output_p(recv_syn->payload, ntohs(recv_syn->length));
        
            // send syn-ack
            char send_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
            packet* syn_ack_pkt = (packet*) send_buf;
            syn_ack_pkt->flags = SYN + ACK;
            syn_ack_pkt->seq = htons(rand() % (1 << 10));
            syn_ack_pkt->ack = htons(ntohs(recv_syn->seq) + 1);
            char message[MAX_PAYLOAD] = {0};
            int n = input_p((uint8_t*)message, MAX_PAYLOAD);
            if (n > 0) {
                memcpy(syn_ack_pkt->payload, message, n);
                syn_ack_pkt->length = htons(n);
            }
            print_diag(syn_ack_pkt, SEND);
            sendto(sockfd, syn_ack_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
                    (struct sockaddr*) addr, sizeof(struct sockaddr_in));
            
            // receive client ACK
            memset(buf, 0, sizeof(packet) + MAX_PAYLOAD);
            packet* rcv_ack = (packet*) buf;
            int bytes_recvd = recvfrom(sockfd, &buf, sizeof(packet) + MAX_PAYLOAD,
                                        0, (struct sockaddr*) addr, &s);
            print_diag(rcv_ack, RECV);
            *next_seq = ntohs(syn_ack_pkt->seq) + 1;
            *cum_ack = ntohs(rcv_ack->seq);
        }
}

// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in* addr, int type,
                 ssize_t (*input_p)(uint8_t*, size_t),
                 void (*output_p)(uint8_t*, size_t)) {

    // initiate connection
    // next seq is the next sequence number that should be reecived
    // cum ack is the last byte that has been acked of the sender
    int16_t next_seq, cum_ack;
    three_way_hs(sockfd, addr, type, input_p, output_p, &next_seq, &cum_ack);

    // unblock the socket
    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);

    printf("Next seq will be %d\n", next_seq);
    printf("Acked up to %d\n", cum_ack);

    // init buffers
    // note:
    // 1. buffers account for headers, and must be stored
    // 2. flow control integer does not count header sie
    int8_t sending_buf[(sizeof(packet) + MAX_PAYLOAD) * 40] = {0};
    int8_t receiving_buf[(sizeof(packet) + MAX_PAYLOAD) * 40] = {0};
    int max_buffer_size = MIN_WINDOW;
    int cur_buffer_size = 0;
    // loop
    while (true) {
        uint8_t rcv_buffer[sizeof(packet) + MAX_PAYLOAD] = {0};
        uint8_t snd_buffer[sizeof(packet) + MAX_PAYLOAD] = {0};
        uint8_t msg[MAX_PAYLOAD] = {0};
        uint8_t ack_data = 0;
        socklen_t addr_size = sizeof(struct sockaddr_in);

        int bytes_recvd = recvfrom(sockfd, rcv_buffer, 1024,
                                    0, (struct sockaddr*) addr, 
                                    &addr_size);
        packet* rcv_pkt = (packet*) rcv_buffer;
        if (bytes_recvd > 0) {
            print_diag(rcv_pkt, RECV);
            // if packet is an ACK packet with data, we must ACK it
            if (ntohs(rcv_pkt->length)) {
                ack_data = 1;
                // TODO: check if received sequence <= cum ack. if so we don't print
                output_p(rcv_pkt->payload, ntohs(rcv_pkt->length));
            }
            // if the sequence is next to ACK, we ACK up to that
            if (ntohs(rcv_pkt->seq) == cum_ack+1) {
                cum_ack++;
            }
            // TODO: if received sequence > cum ack, buffer it
            // otherwise if received sequence < cum ack, resend same ack
        }

        int n = input_p(msg, MAX_PAYLOAD);
        if (n > 0 || ack_data) {
            memset(snd_buffer, 0, sizeof(packet) + MAX_PAYLOAD);
            packet* snd_pkt = (packet*) snd_buffer;
            snd_pkt->seq = htons(next_seq);
            snd_pkt->ack = htons(cum_ack);
            snd_pkt->flags = ACK;
            memcpy(snd_pkt->payload, msg, n);
            snd_pkt->length = htons(n);
            print_diag(snd_pkt, SEND);
            sendto(sockfd, snd_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
                (struct sockaddr*) addr, addr_size);
            next_seq++;
            ack_data = 0;
        }
    }
}
