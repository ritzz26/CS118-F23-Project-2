#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MACROS
#define SERVER_IP "127.0.0.1"
#define LOCAL_HOST "127.0.0.1"
#define SERVER_PORT_TO 5002
#define CLIENT_PORT 6001
#define SERVER_PORT 6002
#define CLIENT_PORT_TO 5001
#define PAYLOAD_SIZE 1024
#define WINDOW_SIZE 5
#define TIMEOUT 2
#define MAX_SEQUENCE 1024



// Packet Layout
// You may change this if you want to
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    char ack;
    char last; //might be a flag for the last packet in the socket
    unsigned int length;
    char payload[PAYLOAD_SIZE];
};

// Utility function to build a packet
void build_packet(struct packet* pkt, unsigned short seqnum, unsigned short acknum, char last, char ack,unsigned int length, const char* payload) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->ack = ack;
    pkt->last = last;
    pkt->length = length;
    memcpy(pkt->payload, payload, length);
}

// Utility function to print a packet
void printRecv(struct packet* pkt) {
    printf("RECV %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", (pkt->ack) ? " ACK": "");
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
}

void shift_left(char arr[], int size) {
    // Shift each element one position to the left
    for (int i = 0; i < size - 1; i++) {
        arr[i] = arr[i + 1];
    }
}

void shift_left_short(unsigned short arr[], int size) {
    // Shift each element one position to the left
    for (int i = 0; i < size - 1; i++) {
        arr[i] = arr[i + 1];
    }
}

void shift_left_packet(struct packet arr[], int size) {
    // Shift each element one position to the left
    for (int i = 0; i < size - 1; i++) {
        arr[i] = arr[i + 1];
    }
}

void shift_left_cstring(char *arr[], int size) {
    for (int i = 0; i < size - 1; i++) {
        strcpy(arr[i], arr[i+1]);
    }
}

void shift_left_int(unsigned int arr[], int size) {
    // Shift each element one position to the left
    for (int i = 0; i < size - 1; i++) {
        arr[i] = arr[i + 1];
    }
}

#endif