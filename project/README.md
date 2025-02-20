# Description of Work (10 points)

This is new for Project 1. We ask you to include a file called `README.md` that contains a quick description of:

1. the design choices you made,
2. any problems you encountered while creating your solution, and
3. your solutions to those problems.

This is not meant to be comprehensive; less than 300 words will suffice.

Design Choices

Instead of splitting up the client and server functionalities, we thought that these two had enough similaries to share a common loop, instead of being split up. To implement this, we mostly followed our implementation from project zero to set up client and server, and created a transport.c file with a listen loop called by both. Outside of this listen loop, we define a three way handshake function to set up our connection. Once this is done, we continue the loop, listening for incoming packets and processing them by setting and checking parity, increasing our current ack numbers, modifying our send and read buffers, and setting timers to run our retransmission logic. For our buffers, we chose a linked list structure that provides efficient insertion or deletion of packet entries as they are sent, received, or need to be retransmitted. We implemented this linked list as a class and defined many useful functions for inserting, deleting, and freeing packets from the buffer.

Challenges + Solutions

Creating buffers and effectively tracking the flow control windows was a significant challenge. Initially, we tried implementing circular buffers but this was difficult to keep track of, especially if the buffer got large enough to wrap around. Switching our data structure to a linked list helped simplify this process, and our helper functions in the listed list class were efficient in this task. Dealing with dropped and corrupted packets also challenging. Implementing a robust retransmission mechanism, along with parity checking, helped mitigate these issues by ensuring that lost or corrupted packets were detected and appropriately retransmitted. However, we still ended up with minor percent differences, meaning we weren't completely successful in this.
