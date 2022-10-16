#include "dp.h"
#include "conf.h"
#include "storage.h"
#include "server.h"


int main(int argc, char **argv)
{
    struct MainOpt opt;
    struct conf cnf;
    struct storage strg;
    unsigned short i;
    char *v_str;
    long long val;

    strg.host = "localhost";
    strg.port = 6379;

    get_main_options(&opt);

    // open_conf(&cnf, opt.fname);

    init_srorage(&strg);

    val = 1278446744;


    // for(i = 0; i < 10; i++)
    // {
    //     write(i);
    // }

    // for(i = 0; i < 10; i++)
    // {
    //     read();
    // }

    init_server();
}
