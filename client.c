#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"
#include <sys/select.h>

#define TIMEOUT_SEC 2
fd_set master_fds, read_fds;
struct timeval timeout;


// Initialize the file descriptor set
void init_fd_set(int send_sockfd) {
    FD_ZERO(&master_fds);
    FD_ZERO(&read_fds);
    FD_SET(send_sockfd, &master_fds);
}

struct packet {
    unsigned short sequence_number;
    unsigned short acknowledgment_number;
    char payload[PAYLOAD_SIZE];
    };


// Check for incoming acknowledgments
int check_for_ack(struct packet *ack_pkt, int seq_num, int send_sockfd, struct sockaddr_in server_addr_from) {
    read_fds = master_fds;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;

    if (select(send_sockfd + 1, &read_fds, NULL, NULL, &timeout) > 0) {
        for (int i = 0; i <= send_sockfd; ++i) {
            if (FD_ISSET(i, &read_fds)) {
                char buffer[PAYLOAD_SIZE];
                socklen_t addr_size = sizeof(server_addr_from);
                int recv_len = recvfrom(i, buffer, PAYLOAD_SIZE, 0, (struct sockaddr *)&server_addr_from, &addr_size);
                if (recv_len > 0) {
                    struct packet ack_pkt;
                    memcpy(&ack_pkt, buffer, recv_len);
                    if (ack_pkt.acknowledgment_number == seq_num - 1) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
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

    int bytes_read;
    while  ((bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp))>0) {
        // create segmented data
        int segment;
        //TODO: HOW DO WE WANT TO SELECT CHUNK AND SEGMENT SIZE
        for (segment = 0; segment * PAYLOAD_SIZE < bytes_read; ++segment) {
            // copy data
            int offset = segment * PAYLOAD_SIZE;
            int chunk = bytes_read - offset;
            if (chunk > PAYLOAD_SIZE) {
                chunk = PAYLOAD_SIZE;
            }
            struct packet pkt;
            pkt.sequence_number = segment;
            pkt.acknowledgment_number = 0; 
            memcpy(pkt.payload, buffer + offset, chunk);

            // Send data to server
            if (sendto(send_sockfd, &pkt, chunk, 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_from)) < 0) {
                perror("Error sending data");
                fclose(fp);
                close(listen_sockfd);
                close(send_sockfd);
                return 1;
            }

            // TODO: Implement acknowledgment handling and timeout logic here
            if (!check_for_ack(&ack_pkt, seq_num, send_sockfd, server_addr_from)) {
                            int timeout_counter = 0;
                            while (!check_for_ack(&ack_pkt, seq_num, send_sockfd, server_addr_from) && timeout_counter < 3) {
                                timeout_counter++;
                                sleep(TIMEOUT_SEC);
                            }
                        }
            // Update sequence number for the next packet
            seq_num++;
        }
    }
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}


