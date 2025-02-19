#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include "io.h"
#include "consts.h"
#include "linked_list.h"

#define MAX_PACKET_SIZE MAX_PAYLOAD + sizeof(packet)

void set_parity(packet* pkt) {
    uint8_t hold = 0;
   
    uint8_t* bytes = (uint8_t*) pkt;
    int len = sizeof(packet) + MIN(MAX_PAYLOAD, ntohs(pkt->length));

    for (int i = 0; i < len; i++) {
        uint8_t val = bytes[i];
        for (int j = 0; j < 8; j++) {
            hold ^= (val >> j) & 1;
         }
        
    }
    if (hold == 1) {
        pkt->flags |= PARITY;
    }
}

int check_parity(packet* pkt) {
    uint8_t hold = 0;
   
    uint8_t* bytes = (uint8_t*) pkt;
    int len = sizeof(packet) + MIN(MAX_PAYLOAD, ntohs(pkt->length));

    for (int i = 0; i < len; i++) {
        uint8_t val = bytes[i];
        for (int j = 0; j < 8; j++) {
            hold ^= (val >> j) & 1;
         }
        
    }
    return hold == 0;
}

void three_way_hs(int sockfd, struct sockaddr_in* addr, int type,
    ssize_t (*input_p)(uint8_t*, size_t), void (*output_p)(uint8_t*, size_t), 
    int16_t* next_seq, int16_t* cum_ack) {
        srand(time(NULL));
        int16_t window = MAX_WINDOW;
        if (type == CLIENT) {
            char send_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
            packet* syn_pkt = (packet*) send_buf;
            syn_pkt->flags = SYN;
            syn_pkt->seq = htons(rand() % (1 << 10));
            syn_pkt->win = htons(window);
            char message[MAX_PAYLOAD] = {0};
            int n = input_p((uint8_t*)message, MAX_PAYLOAD);
            if (n > 0) {
                memcpy(syn_pkt->payload, message, n);
                syn_pkt->length = htons(n);
            }
            set_parity(syn_pkt);
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
            ack_pkt->win = htons(window);
            n = input_p((uint8_t*)message, MAX_PAYLOAD);
            if (n > 0) {
                memcpy(ack_pkt->payload, message, n);
                ack_pkt->length = htons(n);
            } else if (n == 0) {
                ack_pkt->seq = htons(0);
            }
            set_parity(ack_pkt);
            print_diag(ack_pkt, SEND);
            sendto(sockfd, ack_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
                    (struct sockaddr*) addr, sizeof(struct sockaddr_in));
            *next_seq = ntohs(ack_pkt->seq) + 1; // next seq number to send
            *cum_ack = ntohs(recv_sa_pkt->seq) + 1; // last received ack
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
            syn_ack_pkt->win = htons(window);
            char message[MAX_PAYLOAD] = {0};
            int n = input_p((uint8_t*)message, MAX_PAYLOAD);
            if (n > 0) {
                memcpy(syn_ack_pkt->payload, message, n);
                syn_ack_pkt->length = htons(n);
            }
            set_parity(syn_ack_pkt);
            print_diag(syn_ack_pkt, SEND);
            sendto(sockfd, syn_ack_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
                    (struct sockaddr*) addr, sizeof(struct sockaddr_in));
            
            // receive client ACK
            memset(buf, 0, sizeof(packet) + MAX_PAYLOAD);
            packet* rcv_ack = (packet*) buf;
            int bytes_recvd = recvfrom(sockfd, &buf, sizeof(packet) + MAX_PAYLOAD,
                                        0, (struct sockaddr*) addr, &s);
            if (ntohs(rcv_ack->length)) {
                output_p(rcv_ack->payload, ntohs(rcv_ack->length));
            }
            print_diag(rcv_ack, RECV);
            *next_seq = ntohs(syn_ack_pkt->seq) + 1;
            *cum_ack = ntohs(rcv_ack->seq) + 1;
        }
}


// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in* addr, int type,
                 ssize_t (*input_p)(uint8_t*, size_t),
                 void (*output_p)(uint8_t*, size_t)) {

    // initiate connection
    // next seq is the next sequence number that we should send to other end host
    // next ack is the next byte expected from other end host
    int16_t init_seq, next_seq, next_ack;
    three_way_hs(sockfd, addr, type, input_p, output_p, &init_seq, &next_ack);
    next_seq = init_seq;

    // unblock the socket
    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);

    printf("Next seq will be %d\n", next_seq);
    printf("Acked up to %d\n", next_ack - 1);

    // init buffers
    // note:
    // 1. buffers account for headers, and must be stored
    // 2. flow control integer does not count header size
    struct Node* r_buffer = NULL;
    struct Node* s_buffer = NULL;
    int max_win_size = MAX_WINDOW;
    int max_buffer_size = MAX_WINDOW;
    int cur_buffer_size = 0;

    // int i_test = 1;
    // loop
    while (true) {
        uint8_t rcv_buffer[MAX_PACKET_SIZE] = {0};
        uint8_t snd_buffer[MAX_PACKET_SIZE] = {0};
        uint8_t msg[MAX_PAYLOAD] = {0};
        uint8_t ack_data = 0;
        socklen_t addr_size = sizeof(struct sockaddr_in);

        int bytes_recvd = recvfrom(sockfd, rcv_buffer, sizeof(packet) + MAX_PAYLOAD,
                                    0, (struct sockaddr*) addr, 
                                    &addr_size);
        packet* rcv_pkt = (packet*) rcv_buffer;
        if (bytes_recvd > 0) {
            if (check_parity(rcv_pkt)) {
                print_diag(rcv_pkt, RECV);
                int16_t s_num = ntohs(rcv_pkt->seq);
                int16_t a_num = ntohs(rcv_pkt->ack); // ack our sending buffer
                int16_t len = ntohs(rcv_pkt->length);
                max_buffer_size = ntohs(rcv_pkt->win); // set win size

                // if the sequence is next to ACK, we ACK up to that 
                if (s_num == next_ack) {
                    next_ack++;
                    // if packet is an ACK packet with data (length > 0), we must ACK it
                    if (len > 0) {
                        ack_data = 1;
                        output_p(rcv_pkt->payload, len);
                    }
                    // start checking for other packets with seq_num + 1 and print if there
                    int8_t t_buf[MAX_PACKET_SIZE] = {0};
                    packet* t_packet = (packet*) t_buf;
                    int output_io_packets_in_buf = retrieve_packet(&r_buffer, t_packet, next_ack);
                    while (output_io_packets_in_buf) {
                        output_p(t_packet->payload, ntohs(t_packet->length));
                        next_ack++;
                        output_io_packets_in_buf = retrieve_packet(&r_buffer, t_packet, next_ack);
                    }
                    free_up_to(&r_buffer, next_ack);
                } else if (s_num < next_ack) {
                    // if received s_num less than cum ack, ack with current expected ack
                    ack_data = 1;
                } else if (s_num > next_ack) {
                    // TODO: test. manually plant out of order packet.
                    insert_packet(&r_buffer, rcv_pkt);
                }

                // we must remove all packets up to the ACKed packet
                free_up_to(&s_buffer, a_num);
            } else {
                print("CORRUPT");
            }
        }

        // read only as much as there is flow control
        count_unacked_bytes(&s_buffer, &cur_buffer_size);
        int n = input_p(msg, MIN(MAX_PAYLOAD, max_buffer_size - cur_buffer_size));
        if (n > 0 || ack_data) {
            memset(snd_buffer, 0, sizeof(packet) + MAX_PAYLOAD);
            packet* snd_pkt = (packet*) snd_buffer;
            snd_pkt->seq = htons(next_seq);
            snd_pkt->ack = htons(next_ack);
            snd_pkt->win = htons(max_win_size);
            snd_pkt->flags = ACK;
            memcpy(snd_pkt->payload, msg, n);
            snd_pkt->length = htons(n);

            // this code purposefully misplaces packets
            // printf("iter is: %d\n", i_test);
            // if (i_test % 5 == 0) {
            //     snd_pkt->seq = htons(next_seq+1);
            //     next_seq--;
            // }

            set_parity(snd_pkt); // must be last

            print_diag(snd_pkt, SEND);
            sendto(sockfd, snd_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
                (struct sockaddr*) addr, addr_size);

            // add to buffer
            // if there is data, buffer it in send buffer
            if (n > 0) {
                insert_packet(&s_buffer, snd_pkt);
            }
            // print("OBSERVE THE SEND BUFFER:");
            // print_list(&s_buffer);

            next_seq++;
            ack_data = 0;
            // i_test++;
        }
    }
}
