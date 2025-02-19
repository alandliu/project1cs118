#include "linked_list.h"
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "consts.h"

void insert_packet(struct Node** head, packet* pkt) {
    struct Node* new_node = (struct Node*) malloc(sizeof(struct Node) + ntohs(pkt->length));
    // because we are converting, we must restore endianness when extracting
    new_node->seq_num = ntohs(pkt->seq);
    new_node->ack_num = ntohs(pkt->ack);
    new_node->length = ntohs(pkt->length);
    new_node->win = ntohs(pkt->win);
    new_node->flags = pkt->flags;
    memcpy(new_node->payload, pkt->payload, new_node->length);

    struct Node* cur_node = *head;
    
    if (cur_node == NULL || cur_node->seq_num > new_node->seq_num) {
        new_node->next = cur_node;
        *head = new_node;
        return;
    }

    while(cur_node->next != NULL && cur_node->next->seq_num < new_node->seq_num) {
        cur_node = cur_node->next;
    }
    struct Node* temp = cur_node->next;
    cur_node->next = new_node;
    new_node->next = temp;
}

void delete_packet(struct Node** head, int16_t target_seq) {
    struct Node* cur_node = *head;
    
    // check head
    if (cur_node->seq_num == target_seq) {
        *head = cur_node -> next;
        free(cur_node);
        return;
    }

    while (cur_node->next != NULL && cur_node->next->seq_num != target_seq) {
        cur_node = cur_node->next;
    }

    if (cur_node->next == NULL) {
        return;
    }
    struct Node* tb_free = cur_node->next;
    cur_node->next = cur_node->next->next;
    free(tb_free);
}

void count_unacked_bytes(struct Node** head, int* accumulator) {
    int total_bytes = 0;
    struct Node* cur_node = *head;
    while (cur_node != NULL) {
        total_bytes += cur_node->length;
        cur_node = cur_node->next;
    }
    *accumulator = total_bytes;
}

void free_up_to(struct Node** head, int16_t target_seq) {
    struct Node* cur_node = *head;
    // remove head elements
    while (cur_node != NULL && cur_node->seq_num <= target_seq) {
        struct Node* tb_free = cur_node;
        cur_node = cur_node->next;
        free(tb_free);
    }
    *head = cur_node;
}

int retrieve_head_packet(struct Node** head, packet* t_pkt) {
    struct Node* cur_node = *head;
    if (cur_node == NULL) return 0;
    
    int16_t t_seq = cur_node->seq_num;
    return retrieve_packet(head, t_pkt, t_seq);
}

int retrieve_packet(struct Node** head, packet* t_pkt, int16_t target_seq) {
    struct Node* cur_node = *head;
    while (cur_node != NULL && cur_node->seq_num != target_seq) {
        cur_node = cur_node->next;
    }
    if (cur_node == NULL) {
        return 0;
    }
    // return endianness
    t_pkt->seq = htons(cur_node->seq_num);
    t_pkt->ack = htons(cur_node->ack_num);
    t_pkt->length = htons(cur_node->length);
    t_pkt->win = htons(cur_node->win);
    t_pkt->flags = cur_node->flags;
    t_pkt->unused = 0;
    memcpy(t_pkt->payload, cur_node->payload, cur_node->length);
    return 1;
}

void print_list(struct Node** head) {
    struct Node* cur_node = *head;
    while (cur_node != NULL) {
        printf("%d -> ", cur_node->seq_num);
        cur_node = cur_node->next;
    }
    printf("NULL\n");
    return;
}

void free_packets(struct Node** head) {
    struct Node* cur_node = *head;
    while (cur_node != NULL) {
        struct Node* tb_free = cur_node;
        cur_node = cur_node->next;
        free(tb_free);
    }
    printf("All free\n");
    *head = NULL;
    return;
}