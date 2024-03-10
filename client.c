#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"
#include <sys/select.h>

// Check for incoming acknowledgments
int check_for_ack(struct packet *ack_pkt, int seq_num, int listen_sockfd, struct sockaddr_in server_addr_to, socklen_t addr_size) {
    struct packet temp;
    int bytes_read = recvfrom(listen_sockfd, &temp, sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, &addr_size);
    if (bytes_read<0){
        return 0;
    }
    // Check for the sequence number/acknum
    if((&temp)->ack == 1)
        return 1;
    else
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

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        perror("Error setting receive timeout");
        return 1;
    }

    int bytes_read;
    int chunk = PAYLOAD_SIZE;
    while((bytes_read = fread(buffer, 1, chunk, fp))>0) { //is payload_size the best number        
            if (feof(fp)) {
                // End of file is reached, modify the packet type in the build_packet call
                build_packet(&pkt, seq_num, seq_num+bytes_read, 1, 0, bytes_read, buffer);
            }
            else{
                build_packet(&pkt, seq_num, seq_num+chunk, 0, 0, bytes_read, buffer);
            }
            // printf("%s", pkt.payload);
            // Send data to server
            // printSend(&pkt, 0);
            if (sendto(send_sockfd, &pkt, sizeof(struct packet), 0,(struct sockaddr *) &server_addr_to, addr_size) < 0) {
                perror("Error sending data");
                close(listen_sockfd);
                close(send_sockfd);
                return 1;
            }
            // TODO: Implement acknowledgment handling and timeout logic here
            while (!check_for_ack(&ack_pkt, seq_num, listen_sockfd, server_addr_from, addr_size)) {
      //          
                                    // printSend(&pkt, 1);
                                    if (sendto(send_sockfd, &pkt, sizeof(struct packet), 0,(struct sockaddr *) &server_addr_to, addr_size) < 0) {
                                        perror("Error sending data");
                                        close(listen_sockfd);
                                        close(send_sockfd);
                                        return 1;
                                    }       
                                }
            // Update sequence number for the next packet
            seq_num+=chunk;
    }
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}


