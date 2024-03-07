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
int add_to_buffer(struct packet *pkt, struct packet *buffer, short last_acked_num)
{
    int idx = pkt->seqnum - last_acked_num;
    if (idx < 0)
    {
        printf("Already received ACK up to packet %d, which occurs after packet %d\n", last_acked_num, pkt->seqnum);
        return -1;
    }
    if (idx >= BUFFER_SIZE)
    {
        printf("Exceeded maximum window size with packet %d while waiting for ack for %d\n", pkt->seqnum, last_acked_num);
        return -1;
    }
    buffer[idx] = *pkt;
    return idx;
}

// Send all packets within the window (cwnd) starting from first unacked packet
void send_window_packets(short *seq_num,
                        short ack_num,
                        int cwnd,
                        FILE *fp,
                        struct packet *pkt, 
                        int send_sock,
                        struct sockaddr_in *addr, 
                        socklen_t addr_size){
    int unsent_num = *seq_num - ack_num;
    int num_to_send = cwnd - unsent_num;
    printf("cwnd=%d, num_to_send=%d, ", cwnd, num_to_send);
    char payload[PAYLOAD_SIZE];
    int data_len;
    for (int i = 0; i < num_to_send; i++)
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
            return -1;
        }
        else
        {
            perror("recv_ack() failed");
            exit(1);
        }
    }
    return ack_num;
}

void update_buffer(struct packet *buffer, int recv_count){
    for (int i = recv_count; i < BUFFER_SIZE; i++)
    {
        buffer[i - recv_count] = buffer[i];
    }
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
    short newly_acked;
    short ack_num = 0;
    int cwnd = 0;
    int ssthresh = 5;  // TBD
    int duplicate_ack_count;

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
        recv_SYN = recv_ack(listen_sockfd, &server_addr_from, addr_size);
    }
    printf("Connection established\n");

    // TODO: select random seq_num
    // srand(time(NULL));   // initialization
    // seq_num = rand();

    // send packets to the server, and receive ACK
    while (ack_num < packet_num-1){
        if (cwnd <= ssthresh){
            cwnd++;
        }
        send_window_packets(&seq_num, ack_num, cwnd, fp, &pkt, send_sockfd, &server_addr_to, addr_size);
        

        // buffer packet
        add_to_buffer(&pkt, unacked_buffer, ack_num);

        // receive ack
        newly_acked =  recv_ack(listen_sockfd, &server_addr_from, addr_size);
        printf("Received ACK=%d\n", newly_acked);
        
        // handle ack -> packet lost OR timeout
        if (newly_acked == -1){
            // Timeout
            // TODO: adjust cwnd
            // TODO: resend packet
            ssthresh = fmax((int)cwnd / 2, 2);
            cwnd = INITIAL_WINDOW_SIZE;
        }
        else if (newly_acked == ack_num){
            // Fast transmit
            duplicate_ack_count++;
            if (duplicate_ack_count == 3){
                // TODO: adjust cwnd
                // TODO: resend packet
            }
        }
        else if (newly_acked < ack_num){
            // ignore late ack
            continue;
        }
        else{
            // Normal case
            update_buffer(unacked_buffer, newly_acked-ack_num);
            ack_num = newly_acked;
            duplicate_ack_count = 0;
        }
    }

    // Close the socket
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
