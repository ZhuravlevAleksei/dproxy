#ifndef STORAGE_H
#define STORAGE_H

#include <hiredis.h>
#include <stdbool.h>

#define SERVER_COMM_BUFFER_OUT "server:req"
#define SERVER_COMM_BUFFER_IN "server:resp"
#define CLIENT_COMM_BUFFER_IN "client:req"

struct storage
{
    const char *host;
    unsigned short port;
};

void init_srorage(redisContext **context, struct storage *conf);
void write_buffer(redisContext *context, const char *key, char *value);
void read_buffer(redisContext *context, const char *key, char *value);
void write_set(redisContext *context, const char *collection, char *value);
bool check_in_set(redisContext *context, const char *collection, char *value);
bool read_buffer_non_blocking(redisContext *context, const char *key, char *value);


#endif /* #ifndef STORAGE_H */
