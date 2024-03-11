#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"
#include <sys/select.h>

// Check for incoming acknowledgments
int check_for_ack(int listen_sockfd, struct sockaddr_in server_addr_to, socklen_t addr_size, unsigned short *acknumber) {
    struct packet temp;
    int bytes_read = recvfrom(listen_sockfd, &temp, sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, &addr_size);
    if (bytes_read<0){
        return 0;
    }
    printRecv(&temp);
    // Check for the sequence number/acknum
    if((&temp)->ack == 1) {
        *acknumber = (&temp)->acknum;
        return 1;
    }
    else {
        return 0;
    }
}

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    // struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        perror("Error setting receive timeout");
        return 1;
    }

    int bytes_read;
    int chunk = PAYLOAD_SIZE;
    // int packets_sent_in_window = 0;
    int start_times[WINDOW_SIZE];
    char acked_packets[WINDOW_SIZE];
    char sent_packets[WINDOW_SIZE];
    struct packet packets[WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; i++) {
        acked_packets[i] = 0;
        sent_packets[i] = 0;
        //packets[i] = build_packet();
    }
    // while((bytes_read = fread(buffer, 1, chunk, fp))>0) { //is payload_size the best number
    //int index = 0;
    for (;;) {     
    for (int index = 0; index < WINDOW_SIZE+1; index++) {
            // for (int i = 0; i < WINDOW_SIZE; i++) {
            //     printf("%d", packets[i].seqnum);
            // } 
            // TIMEOUT LOGIC: If the packet has been sent but has not been acked yet
            if (sent_packets[index] == 1 && acked_packets[index] == 0) {
                int time_now = time(NULL);
                if (time_now - start_times[index] >= TIMEOUT) {
                    if (sendto(send_sockfd, &packets[index], sizeof(struct packet), 0,(struct sockaddr *) &server_addr_to, addr_size) < 0) {
                        perror("Error sending data");
                        close(listen_sockfd);
                        close(send_sockfd);
                        return 1;
                    }
                    printSend(&packets[index], 1);
                    start_times[index] = time(NULL);
                }

            }
            // SENDING PACKETS LOGIC: If the packet has not been sent yet
            if (sent_packets[index] == 0) {
                bytes_read = fread(buffer, 1, chunk, fp);
                if (feof(fp)) {
                    // End of file is reached, modify the packet type in the build_packet call
                    build_packet(&packets[index], seq_num, seq_num+bytes_read, 1, 0, bytes_read, buffer);
                }
                else{
                    printf("%d\n", seq_num);
                    build_packet(&packets[index], seq_num, seq_num+chunk, 0, 0, bytes_read, buffer);
                    // printf("%d", packets[index].seqnum);
                    //packets[index] = pkt;
                }
                // printf("%s", pkt.payload);
                // Send data to server
                if (sendto(send_sockfd, &packets[index], sizeof(struct packet), 0,(struct sockaddr *) &server_addr_to, addr_size) < 0) {
                    perror("Error sending data");
                    close(listen_sockfd);
                    close(send_sockfd);
                    return 1;
                }
                printSend(&packets[index], 0);
                sent_packets[index] = 1;
                start_times[index] = time(NULL);
                // sent_packets[packets_sent_in_window] = 1;
                // packets_sent_in_window+=1;

                // Update sequence number for the next packet
                seq_num+=chunk;
            }
            //index++;
            if (index < WINDOW_SIZE-1) {
                continue;
            }
            for (int i = 0; i < WINDOW_SIZE; i++) {
                printf("%d\n", packets[i].seqnum);
                printf("%d\n", acked_packets[i]);
            } 
            // ACK LOGIC: Checking for ACKs for unacknowledged packets
            unsigned short acknumber;
            int returnval = check_for_ack(listen_sockfd, server_addr_from, addr_size, &acknumber);
            for (int i = 0; i < WINDOW_SIZE; i++) {
                if (acked_packets[i] == 0) {
                    unsigned short correct_acknum = packets[i].seqnum;
                    if (returnval == 1 && acknumber == correct_acknum) {
                        acked_packets[i] = 1;
                    }
                }
            }

            // SLIDING WINDOW LOGIC: Slide the window appropriately based on already acked packets
            for (int i = 0; i < WINDOW_SIZE; i++) {
                if (acked_packets[i] == 1) {
                    shift_left_packet(packets, WINDOW_SIZE);
                    shift_left(sent_packets, WINDOW_SIZE);
                    shift_left(acked_packets, WINDOW_SIZE);
                    sent_packets[WINDOW_SIZE-1] = 0;
                    acked_packets[WINDOW_SIZE-1] = 0;
                    i--;
                }
                else {
                    printf("broke out of loop");
                    break;
                }
            }
    }
    }
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}


