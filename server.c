    #include <arpa/inet.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>

    #include "utils.h"

    int main() {
        int listen_sockfd, send_sockfd;
        struct sockaddr_in server_addr, client_addr_from, client_addr_to;
        struct packet buffer;
        socklen_t addr_size = sizeof(client_addr_from);
        unsigned short expected_seq_num = 0;
        int recv_len;
        struct packet ack_pkt;

        // Create a UDP socket for sending
        send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (send_sockfd < 0) {
            perror("Could not create send socket");
            return 1;
        }

        // Create a UDP socket for listening
        listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (listen_sockfd < 0) {
            perror("Could not create listen socket");
            return 1;
        }

        // Configure the server address structure
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        // Bind the listen socket to the server address
        if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Bind failed");
            close(listen_sockfd);
            return 1;
        }

        // Configure the client address structure to which we will send ACKs
        memset(&client_addr_to, 0, sizeof(client_addr_to));
        client_addr_to.sin_family = AF_INET;
        client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
        client_addr_to.sin_port = htons(CLIENT_PORT_TO);

        memset(&client_addr_from, 0, sizeof(client_addr_from));
        client_addr_from.sin_family = AF_INET;
        client_addr_from.sin_addr.s_addr = inet_addr(LOCAL_HOST);
        client_addr_from.sin_port = htons(CLIENT_PORT);

        // if (connect(listen_sockfd, (struct sockaddr *)&client_addr_from, addr_size) == -1) {
        //     perror("connect failed");
        //     return 1;
        // }//WE DONT KNOW IF THIS SHOULD BE HERE

        // Open the target file for writing (always write to output.txt)
        FILE *fp = fopen("output.txt", "wb");
        // TODO: Receive file from the client and save it as output.txt
        char last = 0;
        int chunk = PAYLOAD_SIZE;
        // Initializing buffer with all the expected sequence numbers for the given window
        unsigned short expected_seq[WINDOW_SIZE];
        struct packet packets[WINDOW_SIZE];
        int alreadySeen[WINDOW_SIZE];
        for (int i = 0; i < WINDOW_SIZE; i++) {
            expected_seq[i] = i * chunk;
            alreadySeen[i] = 0;
        }
        while (!last) {
            struct packet rec_pkt;
            
            int bytes_read = recvfrom(listen_sockfd, &rec_pkt, sizeof(struct packet), 0,  (struct sockaddr *)&server_addr, &addr_size);
            if (bytes_read <= 0) {
                perror("failed to receive");
                return 1;
            }
            printRecv(&rec_pkt);
            // printf("%s%d\n", "Sequence Num: ",(&rec_pkt)->seqnum);
            // printf("%s", rec_pkt.payload);

            // HANDLE ACKS FOR SEQUENCE NUMBERS IN WINDOW NOT SEEN YET
            for (int i = 0; i < WINDOW_SIZE; i++) {
                printf("%d\n", expected_seq[i]);
                fflush(stdout);
                if (!alreadySeen[i]) {
                    if (rec_pkt.seqnum == expected_seq[i]) {
                        alreadySeen[i] = 1;
                        //strcpy(payloads[i], rec_pkt.payload);
                        build_packet(&packets[i], rec_pkt.seqnum, rec_pkt.seqnum, 0, 1, (&rec_pkt)->length, (&rec_pkt)->payload);
                        packets[i].payload[(&rec_pkt)->length] = '\0';
                        build_packet(&ack_pkt, (&rec_pkt)->seqnum, (&rec_pkt)->seqnum, 0, 1, (&rec_pkt)->length, (&rec_pkt)->payload);
                        if((&rec_pkt)->last==1){
                            last = 1;
                        }
                        if (sendto(send_sockfd, &ack_pkt, sizeof(struct packet), 0,(struct sockaddr *) &client_addr_to, addr_size) < 0) {
                            perror("Error sending ack");
                            close(listen_sockfd);
                            close(send_sockfd);
                            return 1;
                        }
                        printSend(&ack_pkt, 0);
                    }
                } 
            }
            // HANDLE SLIDING WINDOW
            for (int i = 0; i < WINDOW_SIZE; i++) {
                printf("%d", alreadySeen[i]);
                if (alreadySeen[i] == 1) {
                    shift_left_int(alreadySeen, WINDOW_SIZE);
                    fprintf(fp, "%s", packets[i].payload); 
                    shift_left_packet(packets, WINDOW_SIZE);
                    shift_left_short(expected_seq, WINDOW_SIZE);
                    expected_seq[WINDOW_SIZE-1] = expected_seq[WINDOW_SIZE-2] + chunk;
                    alreadySeen[WINDOW_SIZE-1] = 0;
                    i--;
                }
                else {
                    break;
                }
            }
            // // ADDITIONAL LOGIC TO FIX LAST
            // if (last == 1) {
            //     for (int i = 0; i < WINDOW_SIZE; i++) {
                    
            //     }
            // }
            // if(rec_pkt.seqnum == expected_seq_num){
            //     expected_seq_num = (&rec_pkt)->seqnum + (&rec_pkt)->length;
            //     rec_pkt.payload[(&rec_pkt)->length] = '\0';
            //     fprintf(fp, "%s", rec_pkt.payload);
            // }
            // build_packet(&ack_pkt, (&rec_pkt)->seqnum, expected_seq_num, 0, 1, (&rec_pkt)->length, (&rec_pkt)->payload);
            // if((&rec_pkt)->last==1){
            //     last = 1;
            // }
            // if (sendto(send_sockfd, &ack_pkt, sizeof(struct packet), 0,(struct sockaddr *) &client_addr_to, addr_size) < 0) {
            //         perror("Error sending ack");
            //         close(listen_sockfd);
            //         close(send_sockfd);
            //         return 1;
            //     }
                // printSend(&ack_pkt, 0);
        }

        fclose(fp);
        close(listen_sockfd);
        close(send_sockfd);
        return 0;
    }
