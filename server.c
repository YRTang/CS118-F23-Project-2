#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"





int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet cur_pkt;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 1;
    // int recv_len;
    // struct packet ack_pkt;
    struct buffer_unit recv_buffer[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        recv_buffer[i].is_received = 0;
    }

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

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt

    // handle tcp handshake

    //receive tcp handshake msg and get # packet need to get
    recvfrom(listen_sockfd, &cur_pkt, PACKET_SIZE, 0, (struct sockaddr *)&client_addr_from, &addr_size);
    int num_packets = cur_pkt.total_pck_num;
    printf("Received handshake\n");
    printf("num_packets=%d", num_packets);

    //write tcp handshake msg to file
    fwrite(cur_pkt.payload, 1, cur_pkt.length, fp);

    //send ack back to client
    sendto(send_sockfd, &expected_seq_num, sizeof(expected_seq_num), 0, (struct sockaddr *)&client_addr_to, addr_size);
    

    //if receive duplicate handshake msg
    while (cur_pkt.is_handshake){
        // receive handshake msg but not store it, send ack back
        recvfrom(listen_sockfd, &cur_pkt, PACKET_SIZE, 0, (struct sockaddr *)&client_addr_from, &addr_size);
        sendto(send_sockfd, &expected_seq_num, sizeof(expected_seq_num), 0, (struct sockaddr *)&client_addr_to, addr_size);
    }

    // receive all packets
    for (;;) {
        if (expected_seq_num >= num_packets){
            break;
        }
        // receive packets
        recvfrom(listen_sockfd, &cur_pkt, PACKET_SIZE, 0, (struct sockaddr *)&client_addr_from, &addr_size);
        int buffered_index = cur_pkt.seqnum - expected_seq_num;
        if ((buffered_index >= 0) && (buffered_index < BUFFER_SIZE)){
            // buffer packets
            if (recv_buffer[buffered_index].is_received != 0){
                // 0 means not receive this packet before, buffer it
                recv_buffer[buffered_index].pkt = cur_pkt;
                recv_buffer[buffered_index].is_received = 1;
            }
            //write sequenced packets in buffer from head to file
            int un_written_idx = 0;
            for (int i = 0; i < BUFFER_SIZE; ++i) {
                if (!recv_buffer[i].is_received){
                    un_written_idx = i;
                    break;
                }
                fwrite(recv_buffer[i].pkt.payload, 1, recv_buffer[i].pkt.length, fp);
                expected_seq_num ++;
            }

            // move un-written file forward
            for (int i = un_written_idx; i < BUFFER_SIZE; ++i) {
                recv_buffer[i - un_written_idx] = recv_buffer[i];
            }
            for (int i = BUFFER_SIZE - un_written_idx; i < BUFFER_SIZE; ++i) {
                recv_buffer[i].is_received = 0;
            }
        }

        // send ack back to client
        sendto(send_sockfd, &expected_seq_num, sizeof(expected_seq_num), 0, (struct sockaddr *)&client_addr_to, addr_size);
    }

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
