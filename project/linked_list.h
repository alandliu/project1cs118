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

    struct Node *prev;
    struct Node *next;
};

// insert packet where seq num is appropriate
void insert_packet(struct Node** head, packet* pkt);

void delete_packet(struct Node** head, int16_t target_seq);

void print_list(struct Node** head);

void free_packets(struct Node** head);