#include "dp.h"
#include "conf.h"
#include "storage.h"
#include "server.h"
#include "client.h"
#include <threads.h>
#include <stdbool.h>
#include "filter.h"
#include "package.h"
#include <stdlib.h>
#include <arpa/nameser.h>

#include <string.h>
#include <arpa/inet.h>


int main(int argc, char **argv)
{
    struct MainOpt opt;
    struct conf cnf;
    struct storage strg;
    unsigned short i;
    thrd_t thread_servr_ID;
    thrd_t thread_client_ID_0;
    thrd_t thread_client_ID_1;
    thrd_t thread_client_ID_2;    
    redisContext *srg;
    char value[RESPONSE_PACKET_BUF_SIZE];
    int recv_len;
    unsigned char names_len;
    char *name_list;
    unsigned char n;
    unsigned long answ_addr;
    unsigned char *datagram_resp_buf;
    int datagram_len;
    char *json_str_buf;

    struct sockaddr_in s_out_addr;

    strg.host = "localhost";
    strg.port = 6379;

    get_main_options(&opt);

    init_srorage(&srg, &strg);

    open_conf(&cnf, opt.fname, srg);

    memset((char *)&s_out_addr, 0, sizeof(s_out_addr));

    if (inet_aton("68.183.241.41", &s_out_addr.sin_addr) == 0)
    {
        perror("inet_aton() failed\n");
        answ_addr = 0;
    }
    else
    {
        answ_addr = s_out_addr.sin_addr.s_addr;
    }
    
    if(thrd_success != thrd_create(&thread_servr_ID, init_server, NULL))
    {
        printf("Thread init_server start Error\n");
        return 1;
    }

    if(thrd_success != thrd_create(&thread_client_ID_0, init_client, NULL))
    {
        printf("Thread init_client 0 start Error\n");
        return 1;
    }

    // if(thrd_success != thrd_create(&thread_client_ID_1, init_client, NULL))
    // {
    //     printf("Thread init_client 1 start Error\n");
    //     return 1;
    // }

    // if(thrd_success != thrd_create(&thread_client_ID_2, init_client, NULL))
    // {
    //     printf("Thread init_client 2 start Error\n");
    //     return 1;
    // }

    while(true)
    {
        read_buffer(srg, CLIENT_COMM_BUFFER_IN, value);

        names_len = get_names_list(&name_list, value);

        for(n = 0; n < names_len; n++)
        {
            if(!check_in_set(srg, NS_COLLECTION_KEY, (name_list + n)))
            {
                write_buffer(srg, SERVER_COMM_BUFFER_OUT, value);
                continue;
            }

            datagram_resp_buf = calloc(NS_PACKETSZ, sizeof(char));

            datagram_len = build_response_datagram(datagram_resp_buf, value, answ_addr);

            json_str_buf = calloc(RESPONSE_PACKET_BUF_SIZE, sizeof(char));

            if(build_response_packet(json_str_buf, datagram_resp_buf, datagram_len, value))
            {
                write_buffer(srg, SERVER_COMM_BUFFER_IN, json_str_buf);
            }

            free(datagram_resp_buf);
            free(json_str_buf);
        }

        clear_filter();
    }

    thrd_join(thread_servr_ID, NULL);
    thrd_join(thread_client_ID_0, NULL);
    thrd_join(thread_client_ID_1, NULL);
    thrd_join(thread_client_ID_2, NULL);
    return 0;
}
