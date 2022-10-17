#include "storage.h"
#include <stdio.h>

void init_srorage(redisContext **context, struct storage *conf)
{
    redisReply *reply;
    redisContext *cntxt;

    printf("Storage config %s:%u\n", conf->host, conf->port);

    cntxt = redisConnect(conf->host, conf->port);

    if (cntxt == NULL || cntxt->err)
    {
        if (cntxt)
        {
            printf("Storage Error: %s\n", cntxt->errstr);
            // handle error
        }
        else
        {
            printf("Can't allocate redis context\n");
        }

        return;
    }

    reply = redisCommand(cntxt, "PING");

    if(reply == NULL)
    {
        printf("Storage Error: %s\n", cntxt->errstr);
    }

    if(reply->type != REDIS_REPLY_STATUS)
    {
        printf("Storage Error: %s\n", cntxt->errstr);
    }else
    {
        printf("Storage PING was %s\n", reply->str);
    }

    freeReplyObject(reply);

    *context = cntxt;
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
