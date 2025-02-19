#include "linked_list.h"
#include "consts.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define M_P_S sizeof(packet) + MAX_PAYLOAD

int main() {
    struct Node* head = NULL;

    int8_t b1[M_P_S] = {0};
    packet* p1 = (packet*) b1;
    p1->seq = htons(31);

    int8_t b2[M_P_S] = {0};
    packet* p2 = (packet*) b2;
    p2->seq = htons(34);

    int8_t b3[M_P_S] = {0};
    packet* p3 = (packet*) b3;
    p3->seq = htons(32);

    int8_t b4[M_P_S] = {0};
    packet* p4 = (packet*) b4;
    p4->seq = htons(35);

    int8_t b5[M_P_S] = {0};
    packet* p5 = (packet*) b5;
    p5->seq = htons(36);

    int8_t b6[M_P_S] = {0};
    packet* p6 = (packet*) b6;
    p6->seq = htons(33);
    
    insert_packet(&head, p1);
    insert_packet(&head, p2);
    insert_packet(&head, p3);
    insert_packet(&head, p4);
    insert_packet(&head, p5);
    insert_packet(&head, p6);

    int8_t t_buf[M_P_S] = {0};
    packet* t_packet = (packet*) t_buf;
    int16_t ack = 31;
    int output_io_packets_in_buf = retrieve_packet(&head, t_packet, ack);
    while (output_io_packets_in_buf) {
        printf("Retrieved %d\n", htons(t_packet->seq));
        ack++;
        output_io_packets_in_buf = retrieve_packet(&head, t_packet, ack);
    }
    free_up_to(&head, ack);
    print_list(&head);

    // printf("Linked List: ");
    // print_list(&head);

    // printf("Deleting 33...\n");
    // delete_packet(&head, 33);
    // print_list(&head);

    // printf("Deleting up to 33\n");
    // free_up_to(&head, 33);
    // print_list(&head);

    free_packets(&head); // Free memory
    print_list(&head);
    return 0;
}