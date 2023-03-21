#ifndef STORAGE_H
#define STORAGE_H

#include <hiredis.h>
#include <stdbool.h>

#define SERVER_COMM_BUFFER_OUT "server:req"
#define SERVER_COMM_BUFFER_IN "server:resp"
#define CLIENT_COMM_BUFFER_IN "client:req"

bool init_srorage(redisContext** context, char* storage_host, int storage_port, struct timespec* lock_time);

bool write_buffer(redisContext* context, const char* key, char* value);
bool read_buffer(redisContext* context, const char* key, char* value);
bool write_set(redisContext* context, const char* collection, char* value);
bool check_in_set(redisContext* context, const char* collection, char* value);
bool read_buffer_non_blocking(redisContext* context, const char* key, char* value);

#endif /* #ifndef STORAGE_H */
