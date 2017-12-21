#ifndef _gbn_h
#define _gbn_h

#include<sys/types.h>
#include<sys/socket.h>
#include<sys/ioctl.h>
#include<signal.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<netinet/in.h>
#include<errno.h>
#include<netdb.h>
#include<time.h>

/*----- Error variables -----*/
extern int h_errno;
extern int errno;

/*----- Protocol parameters -----*/
#define LOSS_PROB 1e-2    /* loss probability                            */
#define CORR_PROB 1e-3    /* corruption probability                      */

// #define LOSS_PROB 0       /* loss probability                            */
// #define CORR_PROB 0       /* corruption probability                      */
#define DATALEN   1024    /* length of the payload                       */
#define N         1024    /* Max number of packets a single call to gbn_send can process */
#define TIMEOUT      1    /* timeout to resend packets (1 second)        */
#define ATTEMPT_LIMIT     5

/*----- Packet types -----*/
#define SYN      0        /* Opens a connection                          */
#define SYNACK   1        /* Acknowledgement of the SYN packet           */
#define DATA     2        /* Data packets                                */
#define DATAACK  3        /* Acknowledgement of the DATA packet          */
#define FIN      4        /* Ends a connection                           */
#define FINACK   5        /* Acknowledgement of the FIN packet           */
#define RST      6        /* Reset packet used to reject new connections */

#define h_addr h_addr_list[0]

/*----- Go-Back-n packet format -----*/
typedef struct {
    uint16_t checksum;        /* header and payload checksum                */
    uint8_t  type;            /* packet type (e.g. SYN, DATA, ACK, FIN)     */
    uint8_t  padding;
    uint32_t  seqnum;         /* sequence number of the packet              */
    uint32_t  bodylen;
    uint8_t data[DATALEN];    /* pointer to the payload                     */
} gbnhdr;

typedef struct state_t{
    int init_seq;
    int curr_seq;
    int fd;
    int fast_mod;
    int attempts;
    int curr_type;
    int closing;


    const struct  sockaddr *serveraddr;
    socklen_t serveraddrlen;
    int prevsenttype;
    gbnhdr prevheader; 
    gbnhdr prevdata0;
    gbnhdr prevdata1;
    
    const struct  sockaddr *clientaddr;
    socklen_t clientaddrlen;
    size_t last_data_len;
} state_t;

enum {
	CLOSED=0,
	SYN_SENT,
	SYN_RCVD,
	ESTABLISHED,
	FIN_SENT,
	FIN_RCVD
};


#endif
