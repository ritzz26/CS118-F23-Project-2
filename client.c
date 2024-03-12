#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_PORT 12345
#define SERVER_IP "127.0.0.1"
#define CLIENT_PORT 54321
#define TIMEOUT_SEC 2
#define MAX_PACKET_SIZE 1024
#define MAX_SEQ_NUM 256
#define WINDOW_SIZE 4

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

void set_socket_timeout(int sockfd, long sec) {
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        error("Error setting socket timeout");
    }
}

void send_packet(struct packet *pkt) {
    if (sendto(sockfd, pkt, sizeof(*pkt), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        error("sendto");
    }
}

int wait_for_ack(int expected_seq) {
    struct packet ack_pkt;
    unsigned int len = sizeof(cliaddr);
    while (1) {
        if (recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&cliaddr, &len) < 0) {
            // Timeout occurred
            return 0;
        }
        if (ack_pkt.ack == 1 && ack_pkt.seq_num == expected_seq) {
            return 1;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) error("Socket creation failed");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(CLIENT_PORT);
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (const struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) error("Bind failed");

    set_socket_timeout(sockfd, TIMEOUT_SEC);

    // Remove the redundant file opening code here

    // Correct place for the sleep, just before starting to send the file
    sleep(1); // Sleep for 1 second to ensure readiness

    // Open the file a single time before the sending loop
    FILE *file = fopen(argv[1], "rb");
    if (!file) error("Failed to open file");

    int seq_num = 0;
    ssize_t read_bytes;
    char file_buffer[MAX_PACKET_SIZE];
    struct packet pkt;

    while ((read_bytes = fread(file_buffer, 1, MAX_PACKET_SIZE, file)) > 0) {
        pkt.seq_num = seq_num;
        memcpy(pkt.data, file_buffer, read_bytes);
        pkt.len = read_bytes;
        pkt.ack = 0;

        send_packet(&pkt);
        if (!wait_for_ack(seq_num)) {
            printf("Timeout, resending packet %d\n", seq_num);
            send_packet(&pkt); // Resend the same packet
        } else {
            printf("ACK received for packet %d\n", seq_num);
        }
        seq_num = (seq_num + 1) % MAX_SEQ_NUM; // Simple sequence number increment
    }

    fclose(file);
    close(sockfd);
    return 0;
}