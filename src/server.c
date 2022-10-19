#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "server.h"

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
#define PORT 8888  // The port on which to listen for incoming data

void die(char *s)
{
    perror(s);
}

void req_handler(char *buf, int len)
{
    ns_msg msg;
    ns_sect section;
    ns_rr rr;
    int rrmax;
    int i;
    int result;

    result = ns_initparse(buf, len, &msg);

    if (result == -1)
    {
        die("ns_initparse");
    }

    rrmax = ns_msg_count(msg, ns_s_qd);

    for (i = 0; i < rrmax; i++)
    {
        result = ns_parserr(&msg, ns_s_qd, i, &rr);
        printf("%s\n", rr.name);
    }
}

int init_server(void *t)
{
    struct sockaddr_in s_in_addr;
    struct sockaddr_in s_out_addr;
    int slen = sizeof(s_out_addr);
    int out_addr_len;

    int sd;
    int on = 1;
    struct pollfd fds[1];
    int nfds = 1;
    int timeout;
    int result;
    int recv_len;
    char *json_dump;
    PkgContext pkg;
    redisContext *srg;

    struct storage strg;
    strg.host = "localhost";
    strg.port = 6379;

    char buf[BUFLEN] = {0};
    char message[RESPONSE_PACKET_BUF_SIZE];

    unsigned long out_host;
    unsigned short out_port;

    // create UDP socket
    sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sd == -1)
    {
        die("cteate socket error");
        thrd_exit(1);
    }

    result = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char *)&on, sizeof(on));

    if (result == -1)
    {
        die("setsockopt error");
        thrd_exit(1);
    }

    result = ioctl(sd, FIONBIO, (char *)&on);

    if (result == -1)
    {
        die("ioctl error");
        thrd_exit(1);
    }

    memset((char *)&s_in_addr, 0, sizeof(s_in_addr));

    s_in_addr.sin_family = AF_INET;
    s_in_addr.sin_port = htons(PORT);

    s_in_addr.sin_addr.s_addr = htonl(0xC0A86408); //htonl(INADDR_ANY);

    result = bind(sd, (struct sockaddr *)&s_in_addr, sizeof(s_in_addr));

    if (result == -1)
    {
        die("bind error");
        thrd_exit(1);
    }

    memset(fds, 0, sizeof(fds));
    fds[0].fd = sd;
    fds[0].events = POLLIN;

    timeout = (10);

    init_srorage(&srg, &strg);

    while (1)
    {
        memset(buf, 0, BUFLEN);

        result = poll(fds, nfds, timeout);

        if (result == -1)
        {
            die("poll error");
            close(sd);
            thrd_exit(1);
        }
        else if (result > 0)
        {
            recv_len = recvfrom(
                fds[0].fd, buf, BUFLEN, 0, (struct sockaddr *)&s_out_addr, &out_addr_len);

            if (recv_len == -1)
            {
                die("recv error");
                close(sd);
                thrd_exit(1);
            }

            init_package(&pkg);

            datagram_to_json(&pkg, &s_out_addr, buf, recv_len);

            write_buffer(srg, CLIENT_COMM_BUFFER_IN, pkg.json_dump);

            delete_package(&pkg);
        }
        else
        {
            if (!read_buffer_non_blocking(srg, SERVER_COMM_BUFFER_IN, message))
            {
                continue;
            }

            memset((char *)&s_out_addr, 0, sizeof(s_out_addr));

            if (!json_to_response(&out_host, &out_port, buf, &recv_len, message))
            {
                continue;
            }

            if(out_host == 0 || out_port == 0)
            {
                continue;
            }

            s_out_addr.sin_family = AF_INET;
            s_out_addr.sin_port = htons(out_port);
            s_out_addr.sin_addr.s_addr = out_host;


            if (sendto(sd, buf, recv_len, 0, (struct sockaddr *)&s_out_addr, slen) == -1)
            {
                perror("sendto error");
            }
        }
    }

    close(sd);

    thrd_exit(0);
}
