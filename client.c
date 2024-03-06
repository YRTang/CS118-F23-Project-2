#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>

#include "utils.h"


void send_packet(struct packet *pkt, int sockfd, struct sockaddr_in *addr, socklen_t addr_size){
    int bytes_sent = sendto(sockfd, pkt, PACKET_SIZE, 0, (struct sockaddr *)addr, addr_size);
    if (bytes_sent < 0)
    {
        perror("Error sending packet");
        exit(1);
    }
    
}

// Function that will buffer sent packets for us
int buffer_packet(struct packet *pkt, struct packet *buffer, short ack_num)
{
    int ind = pkt->seqnum - ack_num;
    if (ind < 0)
    {
        printf("Already received ACK up to packet %d, which occurs after packet %d\n", ack_num, pkt->seqnum);
        return -1;
    }
    if (ind >= BUFFER_SIZE)
    {
        printf("Exceeded maximum window size with packet %d while waiting for ack for %d\n", pkt->seqnum, ack_num);
        return -1;
    }
    buffer[ind] = *pkt;
    return ind;
}

// Send all packets that are not acked within a window
void send_ready_packets(short *seq_num,
                        short ack_num,
                        FILE *fp,
                        struct packet *pkt, 
                        int send_sock,
                        struct sockaddr_in *addr, 
                        socklen_t addr_size){
    int unsent_num = *seq_num - ack_num;
    // TODO: Adjust due to cwnd
    // int num_to_send = cwnd - num_unacked;
    char payload[PAYLOAD_SIZE];
    int data_len;
    for (int i = 0; i < unsent_num; i++)
    {
        // Read in the file
        data_len = fread(payload, 1, PAYLOAD_SIZE, fp);
        if (data_len < 0)
        {
            perror("Error reading file");
            exit(1);
        }
        build_packet(pkt, *seq_num, ack_num, data_len, payload, 0, pkt->total_pck_num);
        send_packet(pkt, send_sock, addr, addr_size);
        (*seq_num)++;      
    }
}

void set_socket_timeout(int sockfd, struct timeval timeout)
{
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("Error setting socket timeout");
        exit(1);
    }
}

// This function will only receive ACK from the server
int recv_ack(int sockfd, struct sockaddr_in *addr, socklen_t addr_size)
{
    int ack_num;
    if (recvfrom(sockfd, &ack_num, sizeof(ack_num), 0, (struct sockaddr *)addr, &addr_size) < 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            // Timeout reached, return -2 to deal with it in the main
            return -2;
        }
        else
        {
            perror("Recvfrom failed");
            return -1;
        }
    }
    return ack_num;
}

int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    // struct packet ack_pkt;
    // char buffer[PAYLOAD_SIZE];
    short seq_num = 0;
    short ack_num = (-1) * INITIAL_WINDOW_SIZE;
    // char last = 0;
    // char ack = 0;

    struct packet unacked_buffer[BUFFER_SIZE];

    // set timer for packet timeout
    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT * 100000;

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

    // Calculate required packet size
    fseek(fp, 0, SEEK_END);
    double file_size = (double)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    int packet_num = (int)ceil(file_size / (double)PAYLOAD_SIZE);
    printf("packet_num is %d\n", packet_num);

    // Handshake to establish connection
    // Do not include data in the packet (empty payload)
    build_packet(&pkt, seq_num, ack_num, 0, "", 1, packet_num);
    send_packet(&pkt, send_sockfd, &server_addr_to, addr_size);

    set_socket_timeout(listen_sockfd, tv);

    // the server will send SYN (ack_num) = 1 if the handshake is established
    int recv_SYN = recv_ack(listen_sockfd, &server_addr_from, addr_size);

    while (recv_SYN != SYN_NUM){
        send_packet(&pkt, send_sockfd, &server_addr_to, addr_size);
        connected = recv_ack(listen_sockfd, &server_addr_from, addr_size);
    }
    printf("Connection established\n");

    // TODO: select random seq_num
    // srand(time(NULL));   // initialization
    // seq_num = rand();

    // Consistently send packets to the server, and receive ACK
    // Send: Initially N packets (window_size = N); Later, 1 packet at a time
    // Receive: One ACK_packet at a time
    // seq_num = [1:packet_num], ack_num = [0:packet_num]
    while (ack_num < packet_num){
        send_ready_packets(&seq_num, ack_num, fp, &pkt, send_sockfd, &server_addr_to, addr_size);

        // buffer packet
        buffer_packet(&pkt, unacked_buffer, ack_num);

        // receive ack
        ack_num =  recv_ack(listen_sockfd, &server_addr_from, addr_size);
        printf("Received ACK=%d\n", ack_num);
        
        // handle ack -> packet lost OR timeout
    }

    // Close the socket
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

