#ifndef STORAGE_H
#define STORAGE_H

#include <hiredis.h>

struct storage
{
    const char *host;
    unsigned short port;
};

void init_srorage(redisContext **context, struct storage *conf);
void write_buffer(redisContext *context, const char *key, char *value);
void read_buffer(redisContext *context, const char *key, char *value);


#endif /* #ifndef STORAGE_H */
