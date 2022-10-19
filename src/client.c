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
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#define BUFLEN 512 // Max length of buffer
#define SERVER "8.8.8.8"
#define PORT 53 // The port on which to listen for incoming data
#define RECVFROM_TIMEOUT_MS (60 * 1000)


int init_client(void *t)
{
    struct sockaddr_in s_in_addr;
    struct sockaddr_in s_out_addr;
    int slen = sizeof(s_out_addr);
    int out_addr_len;

    int i;
    int sd;
    int on = 1;
    struct pollfd fds[1];
    int nfds = 1;
    int timeout;
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
    sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sd == -1)
    {
        perror("socket");
        thrd_exit(1);
    }

    result = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char *)&on, sizeof(on));

    if (result == -1)
    {
        perror("setsockopt error");
        thrd_exit(1);
    }

    result = ioctl(sd, FIONBIO, (char *)&on);

    if (result == -1)
    {
        perror("ioctl error");
        thrd_exit(1);
    }

    memset((char *)&s_out_addr, 0, sizeof(s_out_addr));

    s_out_addr.sin_family = AF_INET;
    s_out_addr.sin_port = htons(PORT);

    if (inet_aton(SERVER, &s_out_addr.sin_addr) == 0)
    {
        perror("inet_aton() failed\n");
        close(sd);
        thrd_exit(1);
    }

    s_in_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    result = bind(sd, (struct sockaddr *)&s_in_addr, sizeof(s_in_addr));

    if (result == -1)
    {
        perror("bind error");
        thrd_exit(1);
    }

    memset(fds, 0, sizeof(fds));
    fds[0].fd = sd;
    fds[0].events = POLLIN;

    timeout = RECVFROM_TIMEOUT_MS;

    init_srorage(&srg, &strg);

    while (1)
    {
        read_buffer(srg, SERVER_COMM_BUFFER_OUT, message);

        memset(buf, 0, BUFLEN);

        if (!json_to_datagram(message, buf, &recv_len, BUFLEN))
        {
            continue;
        }

        if (sendto(sd, buf, recv_len, 0, (struct sockaddr *)&s_out_addr, slen) == -1)
        {
            perror("sendto()");
            close(sd);
            thrd_exit(1);
        }

        memset(buf, 0, BUFLEN);

        result = poll(fds, nfds, timeout);

        if (result == -1)
        {
            perror("poll error");
            close(sd);
            thrd_exit(1);
        }
        else if (result > 0)
        {
            recv_len = recvfrom(
                sd, buf, BUFLEN, 0, (struct sockaddr *)&s_out_addr, &out_addr_len);

            if (recv_len == -1)
            {
                perror("recvfrom");
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
        else
        {
            printf("Packet was lost. Timeout:%u\n", RECVFROM_TIMEOUT_MS);
            continue;
        }
    }

    close(sd);

    thrd_exit(0);
}