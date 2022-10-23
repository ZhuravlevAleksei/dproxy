#ifndef CONF_H
#define CONF_H

#include <stdio.h>
#include <stdbool.h>
#include <threads.h>
#include "storage.h"

#define NS_COLLECTION_KEY "ns"

typedef struct
{
    char *upstream_dns_host;
    int upstream_dns_port;

    char *storage_host;
    int storage_port;

    char *answ_addr;

    char *listen_host;
    int listen_port;

    int clients_number;
    int client_timeout;
}MainConf;

struct MainOpt
{
    const char *config_file_name;
    const char *blacklist_file_name;
};

void get_main_options(struct MainOpt *options);
bool open_conf(MainConf *cnf, const char *fname);
bool open_blacklist(redisContext *srg, const char *fname);
void free_conf(MainConf *cnf);


#endif /* #ifndef CONF_H */
