#pragma once

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "consts.h"


struct Node {
    int16_t seq_num;
    int16_t ack_num;
    int16_t flags;
    int16_t length;
    int16_t win;
    int8_t payload[0];

    struct Node *next;
};

// insert packet where seq num is appropriate
void insert_packet(struct Node** head, packet* pkt);

// delete packet specified by sequence number
void delete_packet(struct Node** head, int16_t target_seq);

// sets t_pkt equal to the packet
// returns falsy integer (0) if not found
int retrieve_packet(struct Node** head, packet* t_pkt, int16_t target_seq);

// count payload bytes in buffer
void count_unacked_bytes(struct Node** head, int* accumulator);

// free bytes of packets with seq_num <= target
void free_up_to(struct Node** head, int16_t target_seq);

// print out all packet sequence numbers
void print_list(struct Node** head);

// free all packet memory
void free_packets(struct Node** head);