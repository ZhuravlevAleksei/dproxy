#include "storage.h"
#include "conf.h"
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
#include "logger.h"

static struct timespec lock_time;

unsigned long answer_addr_init(char *addr_str)
{
    struct sockaddr_in s_out_addr;
    unsigned long answ_addr = 0;

    memset((char *)&s_out_addr, 0, sizeof(s_out_addr));

    if (inet_aton(addr_str, &s_out_addr.sin_addr) == 0)
    {
        error_log(&lock_time, "Main answ_addr inet_aton()");
        return 0;
    }
    else
    {
        answ_addr = s_out_addr.sin_addr.s_addr;
    }

    return answ_addr;
}


int main(int argc, char **argv)
{
    struct MainOpt opt;
    static MainConf cnf;
    thrd_t thread_servr_ID;
    thrd_t *clients_threads_arr;   
    redisContext *srg;
    char value[RESPONSE_PACKET_BUF_SIZE];
    unsigned char names_len;
    char *name_list;
    unsigned char n;
    unsigned long answ_addr;
    unsigned char *datagram_resp_buf;
    int datagram_len;
    char *json_str_buf;

    logger_init();
    lock_time_init(&lock_time, MAIN_LOG_TIMEOUT);

    get_main_options(&opt);

    if(!open_conf(&cnf, opt.config_file_name))
    {
        free_conf(&cnf);
        return 1;
    }

    answ_addr = answer_addr_init(cnf.answ_addr);

    if(!init_srorage(&srg, cnf.storage_host, cnf.storage_port, &lock_time))
    {
        free_conf(&cnf);
        return 1;
    }

    if(!open_blacklist(srg, opt.blacklist_file_name))
    {
        goto panic;
    }

    if(thrd_success != thrd_create(&thread_servr_ID, init_server, &cnf))
    {
        error_log(&lock_time, "Thread init_server start");
        return 1;
    }

    clients_threads_arr = malloc(cnf.clients_number * sizeof(thrd_t));

    for(n = 0; n < cnf.clients_number; n++)
    {
        if(thrd_create((clients_threads_arr + n), init_client, &cnf) == thrd_success)
        {
            continue;
        }

        error_log(&lock_time, "Thread init_client %d start\n", n);
    }

    while(true)
    {
        if(!read_buffer(srg, CLIENT_COMM_BUFFER_IN, value))
        {
            goto panic;
        }

        names_len = get_names_list(&name_list, value);

        for(n = 0; n < names_len; n++)
        {
            if(!check_in_set(srg, NS_COLLECTION_KEY, (name_list + n)))
            {
                if(!write_buffer(srg, SERVER_COMM_BUFFER_OUT, value))
                {
                    goto panic;
                }
                continue;
            }

            datagram_resp_buf = calloc(NS_PACKETSZ, sizeof(char));

            datagram_len = build_response_datagram(datagram_resp_buf, value, answ_addr);

            json_str_buf = calloc(RESPONSE_PACKET_BUF_SIZE, sizeof(char));

            if(build_response_packet(json_str_buf, datagram_resp_buf, datagram_len, value))
            {
                if(!write_buffer(srg, SERVER_COMM_BUFFER_IN, json_str_buf))
                {
                    goto panic;
                }
            }

            free(datagram_resp_buf);
            free(json_str_buf);
        }

        clear_filter();
    }

    panic:
    for(n = 0; n < cnf.clients_number; n++)
    {
        thrd_detach(*(clients_threads_arr + n));
    }

    thrd_detach(thread_servr_ID);

    free(clients_threads_arr);

    free_conf(&cnf);
    free(name_list);
    return 1;
}
