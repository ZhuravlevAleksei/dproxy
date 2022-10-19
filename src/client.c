#include "client.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include "package.h"
#include "storage.h"
#include <threads.h>

#define BUFLEN 512 // Max length of buffer
#define SERVER "8.8.8.8"
#define PORT 53 // The port on which to listen for incoming data


int init_client(void *t)
{
    struct sockaddr_in s_in_addr;
    struct sockaddr_in s_out_addr;
    int slen = sizeof(s_out_addr);
    int out_addr_len;

    int s;
    int i;
    int result;
    int recv_len;
    PkgContext pkg;
    redisContext *srg;

    unsigned long addr;
    unsigned short port;
    unsigned short transaction;

    struct storage strg;
    strg.host = "localhost";
    strg.port = 6379;

    unsigned char buf[RESPONSE_PACKET_BUF_SIZE];
    char message[RESPONSE_PACKET_BUF_SIZE];

    // create UDP socket
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (s == -1)
    {
        perror("socket");
        thrd_exit(1);
    }

    memset((char *)&s_out_addr, 0, sizeof(s_out_addr));

    s_out_addr.sin_family = AF_INET;
    s_out_addr.sin_port = htons(PORT);

    if (inet_aton(SERVER, &s_out_addr.sin_addr) == 0)
    {
        printf("inet_aton() failed\n");
        close(s);
        thrd_exit(1);
    }

    s_in_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    result = bind(s, (struct sockaddr *)&s_in_addr, sizeof(s_in_addr));

    init_srorage(&srg, &strg);

    while (1)
    {
        read_buffer(srg, SERVER_COMM_BUFFER_OUT, message);

        memset(buf, 0, BUFLEN);

        if (!json_to_datagram(message, buf, &recv_len, BUFLEN))
        {
            continue;
        }

        if (sendto(s, buf, recv_len, 0, (struct sockaddr *)&s_out_addr, slen) == -1)
        {
            perror("sendto()");
            close(s);
            thrd_exit(1);
        }

        memset(buf, 0, BUFLEN);

        recv_len = recvfrom(
            s, buf, BUFLEN, 0, (struct sockaddr *)&s_out_addr, &out_addr_len);

        if (recv_len == -1)
        {
            perror("recvfrom");
            close(s);
            thrd_exit(1);
        }

        if (!json_get_addr(message, &addr, &port, &transaction))
        {
            continue;
        }

        for(i = 0; i < recv_len; i++)
        {
            sprintf((message + (i * 2)), "%02x", buf[i]);
        }

        message[(recv_len * 2)] = 0;

        memset(buf, 0, BUFLEN);

        response_to_json(buf, addr, port, transaction, message);

        write_buffer(srg, SERVER_COMM_BUFFER_IN, buf);
    }

    close(s);

    thrd_exit(0);
}