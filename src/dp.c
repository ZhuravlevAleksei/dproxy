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
    redisContext *srg;
    unsigned short i;
    thrd_t threadID;
    char value[1024];

    strg.host = "localhost";
    strg.port = 6379;

    get_main_options(&opt);

    // open_conf(&cnf, opt.fname);

    filter_test();

    init_srorage(&srg, &strg);

    if(thrd_success != thrd_create(&threadID, init_server, NULL))
    {
        printf("Thread start Error\n");
        return 1;
    }

    while(true)
    {
        read_buffer(srg, "client:rq", value);

        printf("%s\n", value);
    }

    thrd_join(threadID, NULL);
    return 0;
}
