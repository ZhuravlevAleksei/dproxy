#include "storage.h"
#include <stdio.h>

static redisContext *context;

void init_srorage(struct storage *conf)
{
    redisReply *reply;

    printf("Storage config %s:%u\n", conf->host, conf->port);

    context = redisConnect(conf->host, conf->port);

    if (context == NULL || context->err)
    {
        if (context)
        {
            printf("Storage Error: %s\n", context->errstr);
            // handle error
        }
        else
        {
            printf("Can't allocate redis context\n");
        }

        return;
    }

    reply = redisCommand(context, "PING");

    if(reply == NULL)
    {
        printf("Storage Error: %s\n", context->errstr);
    }

    if(reply->type != REDIS_REPLY_STATUS)
    {
        printf("Storage Error: %s\n", context->errstr);
    }else
    {
        printf("Storage PING was %s\n", reply->str);
    }

    freeReplyObject(reply);
}

void write_json(char *value)
{
    redisReply *reply;

    reply = redisCommand(context, "LPUSH clientrec:queue:ns %s", value);
    freeReplyObject(reply);
}

// void write(unsigned short value)
// {
//     redisReply *reply;

//     reply = redisCommand(context, "LPUSH clientrec:queue:ns %u", value);
//     freeReplyObject(reply);
// }

// void read()
// {
//     redisReply *reply;
//     unsigned short out;

//     reply = redisCommand(context, "RPOP clientrec:queue:ns");

//     printf("%s\n", reply->str);

//     freeReplyObject(reply);
// }

