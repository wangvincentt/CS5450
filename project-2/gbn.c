#include "gbn.h"
state_t s;

uint16_t checksum(uint16_t *buf, int nwords) {
    uint32_t sum;

    for (sum = 0; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

uint16_t gbnhdr_checksum(gbnhdr* packet)
{
    int buff_size = sizeof(uint8_t) + sizeof (uint32_t) * 2 + sizeof(packet->data);  
    uint16_t *buff = malloc (sizeof(uint16_t) * buff_size);
    buff[0] = (uint16_t)packet->type;
    buff[1] = (uint16_t)packet->seqnum;
    buff[2] = (uint16_t)packet->bodylen;
    memcpy(buff + 3, packet->data, sizeof(packet->data));

    return checksum(buff, (buff_size / sizeof(uint16_t)));
}


ssize_t maybe_sendto(int  sockfd, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    char *buffer = malloc(len);
    memcpy(buffer, buf, len);
    /*----- Packet not lost -----*/
    if (rand() > LOSS_PROB*RAND_MAX) {
        /*----- Packet corrupted -----*/
        if (rand() < CORR_PROB*RAND_MAX) {
            /*----- Selecting a random byte inside the packet -----*/
            int index = (int)((len-1)*rand()/(RAND_MAX + 1.0));
            /*----- Inverting a bit -----*/
            char c = buffer[index];
            if (c & 0x01) c &= 0xFE;
            else c |= 0x01;
            buffer[index] = c;
        }
        /*----- Sending the packet -----*/
        int retval = sendto(sockfd, buffer, len, flags, to, tolen);
        free(buffer);
        return retval;
    }
    /*----- Packet lost -----*/
    else
        return(len);  /* Simulate a success */
}

void allhandler(int sig){
    printf("Get in interrupt allhandler!\n");
    s.attempts ++;
    s.fast_mod = 0;
    if(s.attempts >= ATTEMPT_LIMIT){
        printf("attempts times reach threshold >= %d\n", ATTEMPT_LIMIT );
        exit(-1);
    }
    if(s.prevsenttype == 0){
        printf("Prev sent header ONLY, resending SYN/FIN, allhandler sending type is %d\n", s.prevheader.type);
        maybe_sendto(s.fd, &s.prevheader, sizeof(s.prevheader), 0, s.serveraddr, s.serveraddrlen);
    }else{  
        printf("Prev sent DATA, resending pack no %d\n", s.prevdata0.seqnum);
        maybe_sendto(s.fd, &s.prevdata0, sizeof(s.prevdata0), 0, s.serveraddr, s.serveraddrlen);
    }
    signal(SIGALRM, allhandler);
    alarm(TIMEOUT);
}

void gbn_init(){
    s.fast_mod = 0;
    s.closing = 0;
    s.last_data_len = 0;
    srand((unsigned)time(0));
    s.init_seq = 0;
    s.curr_seq = 0;
}


int gbn_socket(int domain, int type, int protocol){
        
    /*----- Randomizing the seed. This is used by the rand() function -----*/
    gbn_init();
    return socket(domain,type,protocol);
}

int gbn_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return bind(sockfd, addr, addrlen);
}

int gbn_listen(int sockfd, int backlog) {
    /* do some server-side initialization if necessary */
    return 1;
}

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){

    /* TODO: Your code here. */
    signal(SIGALRM, allhandler);
    s.fd = sockfd;
    s.serveraddr = server;
    s.serveraddrlen = socklen;
    s.prevsenttype = 0;
    s.prevheader.type = SYN;
    s.prevheader.seqnum = 0;
    s.prevheader.checksum = gbnhdr_checksum(&s.prevheader);
    s.init_seq = s.prevheader.seqnum;
    s.curr_seq = s.prevheader.seqnum;

    maybe_sendto(sockfd, &s.prevheader, sizeof(s.prevheader), 0, server, socklen);
    alarm(TIMEOUT);

    gbnhdr synack;
    struct sockaddr_in socket_ack;
    int ack_len;
    while(1){
        int code = recvfrom(sockfd, &synack, sizeof(synack), 0, &socket_ack, &ack_len);
        printf("received type is %d within gbn_connect\n",synack.type);
        if(code < 0){
            printf("Error in recvfrom within gbn_connect, YOU SUCK!\n");
            exit(-1);
        }
        if(gbnhdr_checksum(&synack) != synack.checksum){
            printf("hronly buf checksum is %d, computed checksum is %d\n",synack.checksum, gbnhdr_checksum(&synack));
            printf("checksum is unmatched within gbn_connect, wait for next send\n");
            continue;
        }

        if(synack.type == SYNACK){
            alarm(0);
            s.attempts = 0;
            break;
        }
    }

    printf("Successful connect\n");
    return 0; 
}


int gbn_close(int sockfd){
    printf("prepared to close socket\n");
    /* TODO: Your code here. */


    if(sockfd != s.fd){
        printf("State FD is %d, but query FD is %d\n", s.fd, sockfd);
        return -1;
    }

    if(s.closing){
        printf("Is closing \n");
        return 0;
    }

    s.prevsenttype = 0;
    s.prevheader.type = FIN;
    s.prevheader.seqnum = 0;
    s.prevheader.checksum = gbnhdr_checksum(&s.prevheader);

    s.curr_seq = s.prevheader.seqnum;
    printf("maybe_sendto within close type should be FIN, and true is %d\n", s.prevheader.type);
    maybe_sendto(sockfd, &s.prevheader, sizeof(s.prevheader), 0, s.serveraddr, s.serveraddrlen);
    alarm(TIMEOUT);
    gbnhdr finack;
    struct sockaddr_in socket_ack;
    socklen_t ack_len;

    while(1){
        printf("in gbn_close loop\n");
        int code = recvfrom(sockfd, &finack, sizeof(finack), 0, &socket_ack, &ack_len);
        if(code < 0){
            printf("Error in recvfrom within gbn_close, YOU SUCK!\n");
            exit(-1);
        }

        printf("in loop2, type is %d\n", finack.type);
        if(gbnhdr_checksum(&finack) != finack.checksum){
            printf("hronly buf checksum is %d, computed checksum is %d\n",finack.checksum, gbnhdr_checksum(&finack));
            printf(" checksum is unmatched with gbn_close, wait for next send\n");
            continue;
        }

        printf("finack type within gbn_close is %d\n", finack.type);
        if(finack.type == FINACK){
            alarm(0);
            s.attempts = 0;
            break;
        }
    }

    printf("Successful close\n");
    return 0;
}

int gbn_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
    gbnhdr buf;
    while(1){
        int code = recvfrom(sockfd, &buf, sizeof(buf), 0, addr, addrlen);
        if(code < 0){
            printf("Error in recvfrom within gbn_accept, YOU SUCK!\n");
            exit(-1);
        }
        if(gbnhdr_checksum(&buf) != buf.checksum){
            printf("hronly buf checksum is %d, computed checksum is %d\n",buf.checksum, gbnhdr_checksum(&buf));
            printf(" checksum is unmatched within gbn_accept, wait for next send\n");
            continue;
        }
        s.fd = sockfd;
        s.clientaddr = addr;
        s.clientaddrlen = *addrlen;

        if(buf.type == SYN){
            s.curr_seq = 1;
            s.init_seq = buf.seqnum;
            gbnhdr synack;
            synack.type = SYNACK;
            synack.seqnum = s.init_seq;
            synack.checksum = gbnhdr_checksum(&synack);
            
            maybe_sendto(sockfd, &synack, sizeof(synack), 0, addr, s.clientaddrlen);
            break;
        }
    }
    printf("Successful accept\n");
    return sockfd;
}


int gbn_send(int sockfd, char *buf, size_t len, int flags) {
    if(sockfd != s.fd){
        printf("State FD is %d, but query FD is %d\n", s.fd, sockfd);
        return -1;
    }

    gbnhdr buff;
    struct sockaddr_in sock_ack;
    int tmp_slen;

    int i = 0, j = 0, k = 0;
    while(i < len) {
        s.prevdata0.type = DATA;
        s.prevdata0.seqnum = i + 1;
        j = 0;
        s.prevdata0.bodylen = 0;
        while(j < DATALEN && ( i + j) < len) {
            s.prevdata0.data[j] = buf[ i + j];
            j++;
            s.prevdata0.bodylen++;
        }
        s.prevdata0.checksum = gbnhdr_checksum(&s.prevdata0);
        s.prevsenttype = 1;
        s.curr_seq = s.prevdata0.seqnum;
        
        printf("Send data file type is %d\n", s.prevdata0.type);
        printf("Data length is %d, and Data file content is %d\n", s.prevdata0.bodylen, buf[0]);
        maybe_sendto(sockfd, &s.prevdata0, sizeof(s.prevdata0), 0, s.serveraddr, s.serveraddrlen);
        alarm(TIMEOUT);
        
        if(s.fast_mod && i + DATALEN < len) {
            s.prevdata1.type = DATA;
            s.prevdata1.seqnum = i + DATALEN + 1;
            // s.prevdata1.checksum = 0;
            k = 0;
            s.prevdata1.bodylen = 0;
            while(k < DATALEN && (i + DATALEN + k) < len) {
                s.prevdata1.data[k] = buf[i + DATALEN + k];
                k++;
                s.prevdata1.bodylen++;
            }
            s.prevdata1.checksum = gbnhdr_checksum(&s.prevdata1);
            maybe_sendto(sockfd, &s.prevdata1, sizeof(s.prevdata1), 0, s.serveraddr, s.serveraddrlen);
        } else if(s.fast_mod) {
            s.fast_mod = 0;
        }
        
        int flag = 1;
        while(flag){
	        int code = recvfrom(sockfd, &buff, sizeof(buff), 0, &sock_ack, &tmp_slen);
	        if(code < 0){
            printf("Error in recvfrom within gbn_connect, YOU SUCK!\n");
            exit(-1);
        	}
	        
	        if(gbnhdr_checksum(&buff) != buff.checksum){
	            printf("hronly buf checksum is %d, computed checksum is %d\n",buff.checksum, gbnhdr_checksum(&buff));
                printf(" checksum is unmatched within gbn_send, wait for next send\n");
	            continue;
	        }

	        if(buff.type == DATA) {
	            maybe_sendto(sockfd, &s.prevdata0, sizeof(s.prevdata0), 0, s.serveraddr, s.serveraddrlen);
	            continue;
	        }
	        else if(buff.type == DATAACK) {
	            if(buff.seqnum == s.curr_seq) {
	                alarm(0);
	                s.attempts = 0;
	                i = i + j;
	                s.fast_mod = 1;
	                flag = 0;
	                break;
	            } else {
	                continue;
	            }
	        } else {
	            continue;
	        }
	      }

    }
    return 0;
}


ssize_t gbn_recv(int sockfd, uint8_t *buf, size_t len, int flags) {
    if(sockfd != s.fd){
        printf("unmatched FD within gbn_recv, sockfd is %d, current_fd is%d\n", sockfd, s.fd);
        return -1;
    }

    gbnhdr buff;
    struct sockaddr_in sock_ack;
    int sock_len;

    while(1){
    	int code = recvfrom(sockfd, &buff, sizeof(buff), 0, &sock_ack, &sock_len);
	    if(code < 0 ){
	        printf("Error in recvfrom within gbn_recv, YOU SUCK!\n");
	        exit(-1);
	    }

	    if(buff.type == SYN && buff.seqnum == 0) {
	    	int flag = 1;
            if(gbnhdr_checksum(&buff) != buff.checksum) {
                printf("gbnhdr buf checksum is %d, computed checksum is %d\n",buff.checksum, gbnhdr_checksum(&buff));
                printf(" checksum is unmatched within gbn_recv 1, wait for next send\n");
                flag = 0;
            }
            if(flag == 0)	continue;

            gbnhdr synack; 
            synack.type = SYNACK;
            synack.seqnum = 0;
            synack.checksum = 0;
            synack.checksum = gbnhdr_checksum(&synack);
            maybe_sendto(sockfd, &synack, sizeof(synack), 0, s.clientaddr, s.clientaddrlen);
            continue;
    	}	
    	if(buff.type == FIN && buff.seqnum == 0) {
    		int flag = 1;
            if(gbnhdr_checksum(&buff) != buff.checksum) {
                printf("gbnhdr buf checksum is %d, computed checksum is %d\n",buff.checksum, gbnhdr_checksum(&buff));
                printf(" checksum is unmatched within gbn_recv 2, wait for next send\n");
                flag = 0;
            }
            if(flag == 0)	continue;
            
            gbnhdr finackpack;
            finackpack.type = FINACK;
            finackpack.seqnum = 0;
            finackpack.checksum = gbnhdr_checksum(&finackpack);
            maybe_sendto(sockfd, &finackpack, sizeof(finackpack), 0, s.clientaddr, s.clientaddrlen);
            s.closing = 1;
            gbn_close(sockfd);
            return 0;
    	}

    	if(gbnhdr_checksum(&buff) != buff.checksum) {
            printf("gbnhdr buf checksum is %d, computed checksum is %d\n",buff.checksum, gbnhdr_checksum(&buff));
            printf(" checksum is unmatched within gbn_recv 3, wait for next send\n");
            continue;
    	}

    	if(buff.seqnum == 1 & s.curr_seq >1000000) {
            s.curr_seq = 1;
            s.last_data_len = 0;
    	}

    	if(buff.type == DATA && buff.seqnum < s.curr_seq+s.last_data_len) {
            gbnhdr ulackpack;
            ulackpack.type = DATAACK;
            ulackpack.seqnum = buff.seqnum;
            ulackpack.checksum = gbnhdr_checksum(&ulackpack);

            maybe_sendto(sockfd, &ulackpack, sizeof(ulackpack), 0, s.clientaddr, s.clientaddrlen);
            continue;
    	}else if(buff.type == DATA && buff.seqnum == s.curr_seq + s.last_data_len) {
            gbnhdr dataackpack;
            dataackpack.type = DATAACK;
            dataackpack.seqnum = buff.seqnum;
            dataackpack.checksum = gbnhdr_checksum(&dataackpack);

            maybe_sendto(sockfd, &dataackpack, sizeof(dataackpack), 0, s.clientaddr, s.clientaddrlen);
            printf("Receive data file type is %d\n", buff.type);
            printf("Body length is %d, and Content is %d\n", buff.bodylen, buff.data[0]);
            printf("The last_data_len is %d\n", s.last_data_len);
            for(int ii = 0; ii < buff.bodylen; ii++) {
                buf[ii] = buff.data[ii];
            }
            s.curr_seq = buff.seqnum;
            printf("The Body length now is %d\n",buff.bodylen);
            s.last_data_len = buff.bodylen;
            return s.last_data_len;
    	} 
    }
}
