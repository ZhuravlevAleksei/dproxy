#include "storage.h"
#include <stdio.h>

bool init_srorage(
    redisContext **context, char *storage_host,  int storage_port)
{
    redisReply *reply;
    redisContext *cntxt;

    printf("Storage config %s:%u\n", storage_host, storage_port);

    cntxt = redisConnect(storage_host, storage_port);

    if (cntxt == NULL || cntxt->err)
    {
        if (cntxt)
        {
            printf("Storage Error: %s\n", cntxt->errstr);
        }
        else
        {
            printf("Can't allocate redis context\n");
        }

        return false;
    }

    reply = redisCommand(cntxt, "PING");

    if(reply == NULL)
    {
        printf("Storage Error: %s\n", cntxt->errstr);
    }

    if(reply->type != REDIS_REPLY_STATUS)
    {
        printf("Storage Error: %s\n", cntxt->errstr);
        return false;
    }else
    {
        printf("Storage PING was %s\n", reply->str);
    }

    freeReplyObject(reply);

    *context = cntxt;

    return true;
}

void write_buffer(redisContext *context, const char *key, char *value)
{
    redisReply *reply;

    reply = redisCommand(context, "LPUSH %s %s", key, value);
    freeReplyObject(reply);
}

void write_set(redisContext *context, const char *collection, char *value)
{
    redisReply *reply;

    reply = redisCommand(context, "SADD %s %s", collection, value);
    freeReplyObject(reply);
}

void read_buffer(redisContext *context, const char *key, char *value)
{
    redisReply *reply;

    reply = redisCommand(context, "BLPOP %s 0", key);

    if (reply->type == REDIS_REPLY_ARRAY) {
        sprintf(value, "%s", reply->element[1]->str);
    }

    freeReplyObject(reply);
}

bool read_buffer_non_blocking(redisContext *context, const char *key, char *value)
{
    redisReply *reply;

    reply = redisCommand(context, "LLEN %s", key);

    if(reply->integer == 0)
    {
        freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);

    reply = redisCommand(context, "LPOP %s", key);

    if (reply->type == REDIS_REPLY_STRING) {
        sprintf(value, "%s", reply->str);
    }

    freeReplyObject(reply);
    return true;
}

bool check_in_set(redisContext *context, const char *collection, char *value)
{
    redisReply *reply;
    bool res;

    reply = redisCommand(context, "SISMEMBER %s %s", collection, value);

    if(reply->integer == 1)
    {
        res = true;
    }else
    {
        res = false;
    }

    freeReplyObject(reply);

    return res;
}
