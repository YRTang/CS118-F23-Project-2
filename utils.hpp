#ifndef UTILS_H
#define UTILS_H
#include <cstdio>
#include <cstdlib>
#include <cstring>

// MACROS
#define SERVER_IP "127.0.0.1"
#define LOCAL_HOST "127.0.0.1"
#define SERVER_PORT_TO 5002
#define CLIENT_PORT 6001
#define SERVER_PORT 6002
#define CLIENT_PORT_TO 5001
#define PAYLOAD_SIZE 1180
#define INITIAL_WINDOW_SIZE 1
#define TIMEOUT 2
#define PACKET_SIZE 1200
#define BUFFER_SIZE 50
#define SYN_NUM 1


// Packet Layout
// You may change this if you want to
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    //char ack;
    //char last;
    unsigned int length;
    char payload[PAYLOAD_SIZE];
    int is_handshake;
    int total_pck_num;
};

// Utility function to build a packet
void build_packet(struct packet* pkt, unsigned short seqnum, unsigned short acknum,
                unsigned int length, const char* payload, int is_handshake, int total_pck_num) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    //pkt->ack = ack;
    //pkt->last = last;
    pkt->length = length;
    pkt->is_handshake = is_handshake;
    pkt->total_pck_num = total_pck_num;
    memcpy(pkt->payload, payload, length);
}

struct buffer_unit{
    struct packet pkt;
    int is_received;
};

// Utility function to print a packet
void printRecv(struct packet* pkt) {
    //printf("RECV %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", (pkt->ack) ? " ACK": "");
    printf("RECV %d %d\n", pkt->seqnum, pkt->acknum);
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d\n", pkt->seqnum, pkt->acknum);
        //printf("RESEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d\n", pkt->seqnum, pkt->acknum);
        //printf("SEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
}

#endif