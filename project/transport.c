#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include "io.h"
#include "consts.h"

void three_way_hs(int sockfd, struct sockaddr_in* addr, int type,
    ssize_t (*input_p)(uint8_t*, size_t),
    void (*output_p)(uint8_t*, size_t)) {
        srand(time(NULL));
        if (type == CLIENT) {
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
                    (struct sockaddr*) addr, sizeof(struct sockaddr_in));

            // wait for syn-ack
            char recv_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
            socklen_t s = sizeof(struct sockaddr_in);
            int bytes_recvd = recvfrom(sockfd, &recv_buf, sizeof(recv_buf), 0, (struct sockaddr*) addr, &s);
            packet* recv_sa_pkt = (packet*) recv_buf;
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
                    (struct sockaddr*) addr, sizeof(struct sockaddr_in));
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
            sendto(sockfd, syn_ack_pkt, sizeof(packet) + MAX_PAYLOAD, 0, 
                    (struct sockaddr*) addr, sizeof(struct sockaddr_in));
            // print("SYN-ACK sent");
        }
}

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
