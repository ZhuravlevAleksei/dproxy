#ifndef CONF_H
#define CONF_H

#include <stdio.h>
#include "storage.h"

#define NS_COLLECTION_KEY "ns"

struct conf
{
    const char *fname;         
    FILE *fh;
};

struct MainOpt
{
    const char *fname;
};

void open_conf(struct conf *cnf, const char *fname, redisContext *storage);
void get_main_options(struct MainOpt *options);

#endif /* #ifndef CONF_H */
