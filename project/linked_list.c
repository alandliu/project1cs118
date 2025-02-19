#include "linked_list.h"
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "consts.h"

void insert_packet(struct Node** head, packet* pkt) {
    struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));
    // we are not converting network so we can just send directly when restoring
    new_node->seq_num = pkt->seq;
    new_node->ack_num = pkt->ack;
    new_node->length = pkt->length;
    new_node->win = pkt->win;
    memcpy(new_node->payload, pkt->payload, ntohs(pkt->length));
    
    if (*head == NULL) {
        *head = new_node;
        new_node->next = NULL;
        return;
    }

    struct Node* cur_node = *head;
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

    struct Node* tb_free = cur_node->next;
    cur_node->next = cur_node->next->next;
    free(tb_free);
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
    return;
}