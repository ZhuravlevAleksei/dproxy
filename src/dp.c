#include "dp.h"
#include "conf.h"
#include "storage.h"
#include "server.h"
#include <threads.h>
#include <stdbool.h>
#include "filter.h"


int main(int argc, char **argv)
{
    struct MainOpt opt;
    struct conf cnf;
    struct storage strg;
    unsigned short i;
    thrd_t threadID;
    redisContext *srg;
    char value[1024];
    unsigned char names_len;
    char *name_list;
    unsigned char n;

    strg.host = "localhost";
    strg.port = 6379;

    get_main_options(&opt);

    init_srorage(&srg, &strg);

    open_conf(&cnf, opt.fname, srg);

    if(thrd_success != thrd_create(&threadID, init_server, NULL))
    {
        printf("Thread start Error\n");
        return 1;
    }

    while(true)
    {
        read_buffer(srg, CLIENT_COMM_BUFFER_IN, value);

        names_len = get_names_list(&name_list, value);

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

    thrd_join(threadID, NULL);
    return 0;
}
