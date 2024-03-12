#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"
#include <sys/select.h>

double base_rtt = 0.1;
double resend_timeout;
int cwnd = 1;
int thresh = 1;
unsigned short last_sent_seq = 0;
char last = 0;

int check_for_ack(unsigned short seq_num, int listen_sockfd, struct sockaddr_in server_addr_to, socklen_t addr_size, int chunk, int N) {
    struct packet temp;
    unsigned short temp_seq = seq_num + PAYLOAD_SIZE;
    int bytes_read = recvfrom(listen_sockfd, &temp, sizeof(struct packet), 0, (struct sockaddr *)&server_addr_to, &addr_size);
    if (bytes_read<0){
        return 0;
    }
    if(temp.last==1){
            last = 1;
            return 1;
        }

    // if((temp.ack == 1) && ((temp.acknum==temp_seq)||((temp.acknum==0)&&(temp_seq==64512))))
    if((temp.ack == 1) && ((temp.acknum==temp_seq)))
    {
        int temp_val = temp.acknum;
        // if(((temp.acknum==0)&&(temp.seqnum==64512))){
        if (temp.acknum==0) {
            temp_val = 1;
        }
        last_sent_seq = temp.acknum;
        cwnd=cwnd+(1/cwnd);
        return temp_val;
    }
    else
    {
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
    timeout.tv_usec = 500000; //CHANGE TO lower??

    if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        perror("Error setting receive timeout");
        return 1;
    }

    int bytes_read;
    int chunk = PAYLOAD_SIZE;
    int N;
    int ack_rec = 0;
    int piped_pckts=0;
    unsigned short sent_seq_num=0;
    resend_timeout = base_rtt * (1 + base_rtt);
    // struct packet* pkts_sent = malloc(N * sizeof(struct packet));
    size_t bufferSize = sizeof(struct packet);
    struct packet* pkts_sent = (struct packet*)malloc(bufferSize*10);
    while(!last|| (piped_pckts!=0)){
        N=cwnd;
        while(piped_pckts<N){
            bytes_read = fread(buffer, 1, chunk, fp);
            if (feof(fp)) {
                build_packet(&pkts_sent[piped_pckts], sent_seq_num, sent_seq_num+chunk,1, 0, bytes_read, buffer);
                piped_pckts++;
            }
            else{
                build_packet(&pkts_sent[piped_pckts], sent_seq_num, sent_seq_num+chunk,0, 0, bytes_read, buffer);
            }
            // usleep(100000);
            // printSend(&pkts_sent[piped_pckts], 0);
            if (sendto(send_sockfd, &pkts_sent[piped_pckts], sizeof(pkts_sent[piped_pckts]), 0,(struct sockaddr *) &server_addr_to, addr_size) < 0) {
                perror("Error sending data");
                close(listen_sockfd);
                close(send_sockfd);
                return 1;
            }
            last_sent_seq = sent_seq_num;

            piped_pckts+=1;
            sent_seq_num+=chunk;
        }
        int ack_rec = 0;
        while (!ack_rec && piped_pckts>0 && !last){
            // cwnd/=2;
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(listen_sockfd, &readfds);
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 20000;
            int ret = select(listen_sockfd + 1, &readfds, NULL, NULL, &timeout);
            if (ret > 0 && FD_ISSET(listen_sockfd, &readfds)) {
                ack_rec = check_for_ack(last_sent_seq, listen_sockfd, server_addr_from, addr_size, chunk, N);
                // last = ack_rec;
            } else if (ret == 0) {
                // Retransmit all unacknowledged packets
                for (int k=0; k<piped_pckts; k+=1) {
                    // printSend(&pkts_sent[k], 1);
                    usleep(10000);
                    if (sendto(send_sockfd, &pkts_sent[k], sizeof(struct packet), 0,(struct sockaddr *) &server_addr_to, addr_size) < 0) {
                        perror("Error sending data");
                        close(listen_sockfd);
                        close(send_sockfd);
                        return 1;
                    } 
                }
                cwnd /=2;
                if(cwnd<1){
                    cwnd=1;
                }
            }     
        }
        for (int i = 0; i < N - 1; i+=1) {
            pkts_sent[i] = pkts_sent[i + 1];
        }
        piped_pckts--;
    }
    free(pkts_sent);
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}


