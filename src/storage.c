#include "storage.h"
#include "logger.h"
#include <stdio.h>

static struct timespec* lock_time;

bool init_srorage(redisContext** context, char* storage_host, int storage_port, struct timespec* l_time) {
    redisReply* reply;
    redisContext* cntxt;
    lock_time = l_time;

    info_log(lock_time, "Storage config %s:%u\n", storage_host, storage_port);

    cntxt = redisConnect(storage_host, storage_port);

    if(cntxt == NULL || cntxt->err) {
        if(cntxt) {
            error_log(lock_time, "Storage Error: %s\n", cntxt->errstr);
        } else {
            error_log(lock_time, "Can't allocate redis context\n");
        }

        return false;
    }

    reply = redisCommand(cntxt, "PING");

    if(reply == NULL) {
        error_log(lock_time, "Storage Error: %s\n", cntxt->errstr);
        return false;
    }

    if(reply->type != REDIS_REPLY_STATUS) {
        error_log(lock_time, "Storage Error: %s\n", cntxt->errstr);
        freeReplyObject(reply);

        return false;
    } else {
        info_log(lock_time, "Storage PING gets %s\n", reply->str);
    }

    freeReplyObject(reply);

    *context = cntxt;

    return true;
}

bool write_buffer(redisContext* context, const char* key, char* value) {
    redisReply* reply;

    reply = redisCommand(context, "LPUSH %s %s", key, value);

    if(reply == NULL) {
        error_log(lock_time, "Storage redisCommand Error: reply == NULL\n");
        return false;
    }

    freeReplyObject(reply);
    return true;
}

bool write_set(redisContext* context, const char* collection, char* value) {
    redisReply* reply;

    reply = redisCommand(context, "SADD %s %s", collection, value);

    if(reply == NULL) {
        error_log(lock_time, "Storage redisCommand Error: reply == NULL\n");
        return false;
    }

    freeReplyObject(reply);
    return true;
}

bool read_buffer(redisContext* context, const char* key, char* value) {
    redisReply* reply;

    reply = redisCommand(context, "BLPOP %s 0", key);

    if(reply == NULL) {
        error_log(lock_time, "Storage redisCommand Error: reply == NULL\n");
        return false;
    }

    if(reply->type == REDIS_REPLY_ARRAY) {
        sprintf(value, "%s", reply->element[1]->str);
    }

    freeReplyObject(reply);
    return true;
}

bool read_buffer_non_blocking(redisContext* context, const char* key, char* value) {
    redisReply* reply;

    reply = redisCommand(context, "LLEN %s", key);

    if(reply == NULL) {
        error_log(lock_time, "Storage redisCommand Error: reply == NULL\n");
        return false;
    }

    if(reply->integer == 0) {
        freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);

    reply = redisCommand(context, "LPOP %s", key);

    if(reply == NULL) {
        error_log(lock_time, "Storage redisCommand Error: reply == NULL\n");
        return false;
    }

    if(reply->type == REDIS_REPLY_STRING) {
        sprintf(value, "%s", reply->str);
    }

    freeReplyObject(reply);
    return true;
}

bool check_in_set(redisContext* context, const char* collection, char* value) {
    redisReply* reply;
    bool res;

    reply = redisCommand(context, "SISMEMBER %s %s", collection, value);

    if(reply == NULL) {
        error_log(lock_time, "Storage redisCommand Error: reply == NULL\n");
        return false;
    }

    if(reply->integer == 1) {
        res = true;
    } else {
        res = false;
    }

    freeReplyObject(reply);

    return res;
}
