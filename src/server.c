#include "server.h"
#include "conf.h"
#include "logger.h"
#include "package.h"
#include "storage.h"
#include <arpa/inet.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <threads.h>
#include <unistd.h>


#define BUFLEN 512

int init_server(void* cnf) {
    MainConf* serv_conf = (MainConf*)cnf;
    struct sockaddr_in s_in_addr;
    struct sockaddr_in s_out_addr;
    int slen = sizeof(s_out_addr);
    int out_addr_len = sizeof(s_out_addr);

    int sd;
    int on = 1;
    struct pollfd fds[1];
    int nfds = 1;
    int timeout;
    int result;
    int recv_len;
    char* json_dump;
    PkgContext pkg;
    redisContext* srg;
    struct timespec lock_time;

    char buf[BUFLEN] = {0};
    char message[RESPONSE_PACKET_BUF_SIZE];

    unsigned long out_host;
    unsigned short out_port;

    lock_time_init(&lock_time, SERVER_LOG_TIMEOUT);

    if(!init_srorage(&srg, serv_conf->storage_host, serv_conf->storage_port, &lock_time)) {
        error_log(&lock_time, "Server init_srorage");
        thrd_exit(1);
    }

    init_package(&lock_time);

    // create UDP socket
    sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(sd == -1) {
        error_log(&lock_time, "Server cteate socket");
        thrd_exit(1);
    }

    result = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char*)&on, sizeof(on));

    if(result == -1) {
        error_log(&lock_time, "Server set socket options");
        thrd_exit(1);
    }

    result = ioctl(sd, FIONBIO, (char*)&on);

    if(result == -1) {
        error_log(&lock_time, "Server ioctl");
        thrd_exit(1);
    }

    memset((char*)&s_out_addr, 0, sizeof(s_out_addr));

    memset((char*)&s_in_addr, 0, sizeof(s_in_addr));

    s_in_addr.sin_family = AF_INET;
    s_in_addr.sin_port = htons(serv_conf->listen_port);

    if(inet_aton(serv_conf->listen_host, &s_in_addr.sin_addr) == 0) {
        error_log(&lock_time, "Server inet_aton() failed");
        return 0;
    }

    result = bind(sd, (struct sockaddr*)&s_in_addr, sizeof(s_in_addr));

    if(result == -1) {
        error_log(&lock_time, "Server bind");
        thrd_exit(1);
    }

    memset(fds, 0, sizeof(fds));
    fds[0].fd = sd;
    fds[0].events = POLLIN;

    timeout = (10);

    while(1) {
        memset(buf, 0, BUFLEN);

        result = poll(fds, nfds, timeout);

        if(result == -1) {
            error_log(&lock_time, "Server poll");
            close(sd);
            thrd_exit(1);
        } else if(result > 0) {
            recv_len = recvfrom(fds[0].fd, buf, BUFLEN, 0, (struct sockaddr*)&s_out_addr, &out_addr_len);

            if(recv_len == -1) {
                error_log(&lock_time, "Server recvfrom");
                close(sd);
                thrd_exit(1);
            }

            create_package(&pkg);

            datagram_to_json(&pkg, &s_out_addr, buf, recv_len);

            if(!write_buffer(srg, CLIENT_COMM_BUFFER_IN, pkg.json_dump)) {
                close(sd);
                thrd_exit(0);
            }

            delete_package(&pkg);
        } else {
            if(!read_buffer_non_blocking(srg, SERVER_COMM_BUFFER_IN, message)) {
                continue;
            }

            memset((char*)&s_out_addr, 0, sizeof(s_out_addr));

            if(!json_to_response(&out_host, &out_port, buf, &recv_len, message)) {
                continue;
            }

            if(out_host == 0 || out_port == 0) {
                continue;
            }

            s_out_addr.sin_family = AF_INET;
            s_out_addr.sin_port = htons(out_port);
            s_out_addr.sin_addr.s_addr = out_host;

            if(sendto(sd, buf, recv_len, 0, (struct sockaddr*)&s_out_addr, slen) == -1) {
                error_log(&lock_time, "Server sendto");
            }
        }
    }

    close(sd);

    thrd_exit(0);
}
