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

#define BUFLEN 512 // Max length of buffer
#define PORT 8888  // The port on which to listen for incoming data

void die(char *s)
{
    perror(s);
}

void req_handler(char *buf, int len){
    ns_msg msg;
    ns_sect section;
    ns_rr rr;
    int rrmax;
    int i;
    int result;

    result = ns_initparse(buf, len, &msg);

    if(result == -1)
    {
        die("ns_initparse");
    }

    rrmax = ns_msg_count(msg, ns_s_qd);

    for(i = 0; i < rrmax; i++)
    {
        result =  ns_parserr(&msg, ns_s_qd, i, &rr);
        printf("%s\n", rr.name);
    }

}

int init_server(void *t)
{
    struct sockaddr_in s_in_addr;
    struct sockaddr_in s_out_addr;
    int out_addr_len;

    int s;
    int result;
    int recv_len;
    char *json_dump;
    PkgContext pkg;
    redisContext *srg;

    struct storage strg;
    strg.host = "localhost";
    strg.port = 6379;

    char buf[BUFLEN] = {0};

    // create UDP socket
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (s == -1)
    {
        die("socket");
        thrd_exit(1);
    }

    memset((char *)&s_in_addr, 0, sizeof(s_in_addr));

    s_in_addr.sin_family = AF_INET;
    s_in_addr.sin_port = htons(PORT);
    s_in_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    result = bind(s, (struct sockaddr *)&s_in_addr, sizeof(s_in_addr));

    if (result == -1)
    {
        die("bind");
        thrd_exit(1);
    }
    
    init_srorage(&srg, &strg);
    
    while (1)
    {
        memset(buf, 0, BUFLEN);

        recv_len = recvfrom(
            s, buf, BUFLEN, 0, (struct sockaddr *)&s_out_addr, &out_addr_len);


        if (recv_len == -1)
        {
            die("recvfrom");
            close(s);
            thrd_exit(0);
        }

        init_package(&pkg);

        datagram_to_json(&pkg, &s_out_addr, buf, recv_len);

        write_buffer(srg, CLIENT_COMM_BUFFER_IN, pkg.json_dump);

        delete_package(&pkg);
    }

    close(s);

    thrd_exit(0);
}
