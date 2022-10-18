#include "dp.h"
#include "conf.h"
#include "storage.h"
#include "server.h"
#include "client.h"
#include <threads.h>
#include <stdbool.h>
#include "filter.h"
#include "package.h"


int main(int argc, char **argv)
{
    struct MainOpt opt;
    struct conf cnf;
    struct storage strg;
    unsigned short i;
    thrd_t thread_servr_ID;
    thrd_t thread_client_ID;
    redisContext *srg;
    char value[1024];
    unsigned char buf[65535];
    int recv_len;
    unsigned char names_len;
    char *name_list;
    unsigned char n;

    strg.host = "localhost";
    strg.port = 6379;

    get_main_options(&opt);

    init_srorage(&srg, &strg);

    open_conf(&cnf, opt.fname, srg);

    if(thrd_success != thrd_create(&thread_servr_ID, init_server, NULL))
    {
        printf("Thread init_server start Error\n");
        return 1;
    }

    if(thrd_success != thrd_create(&thread_client_ID, init_client, NULL))
    {
        printf("Thread init_client start Error\n");
        return 1;
    }
    // init_client((void*)buf);

    while(true)
    {
        read_buffer(srg, CLIENT_COMM_BUFFER_IN, value);

        names_len = get_names_list(&name_list, value);

        // json_to_datagram(value, buf, &recv_len, 65535);

        for(n = 0; n < names_len; n++)
        {
            if(check_in_set(srg, NS_COLLECTION_KEY, (name_list + n)))
            {
                continue;
            }

            write_buffer(srg, SERVER_COMM_BUFFER_OUT, value);
        }

        clear_filter();
    }

    thrd_join(thread_servr_ID, NULL);
    thrd_join(thread_client_ID, NULL);
    return 0;
}
