#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_PORT 12345
#define MAX_PACKET_SIZE 1024
#define MAX_SEQ_NUM 256

struct packet {
    int seq_num;
    char data[MAX_PACKET_SIZE];
    int len; // Actual data length
    int ack;
};

int sockfd;
struct sockaddr_in servaddr, cliaddr;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void send_ack(int seq) {
    struct packet ack_pkt;
    ack_pkt.seq_num = seq;
    ack_pkt.ack = 1; // Indicate it's an ACK packet
    if (sendto(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
        error("sendto");
    }
}

int main() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) error("Socket creation failed");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) error("Bind failed");

    FILE *output_file = fopen("output.txt", "wb");
    if (!output_file) error("Failed to open file for writing");

    struct packet recv_pkt;
    unsigned int len = sizeof(cliaddr);
    while (1) {
        if (recvfrom(sockfd, &recv_pkt, sizeof(recv_pkt), 0, (struct sockaddr *)&cliaddr, &len) < 0) {
            printf("No more data. Closing.\n");
            break;
        }
        fwrite(recv_pkt.data, 1, recv_pkt.len, output_file);
        send_ack(recv_pkt.seq_num);
    }

    fclose(output_file);
    close(sockfd);
    return 0;
}
