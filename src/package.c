#include "package.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <resolv.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static const char *query_json_keys[] = {
    "flags",
    "questions",
    "answer",
    "authority",
    "additional"
};

typedef enum
{
    ns_transaction = 0,
    ns_flags = 2,
    ns_questions = 4,
    ns_answer = 6,
    ns_authority = 8,
    ns_additional = 10
} NsSectDisp;

bool str_to_register(unsigned short *reg, const cJSON *json, const char *key);

void init_package(PkgContext *context)
{
    context->datagram = cJSON_CreateObject();
}

void delete_package(PkgContext *context)
{
    cJSON_Delete(context->datagram);
}

void arithmetic_to_json(cJSON *j_obj, const char *key, unsigned long long value)
{
    cJSON *j_number = NULL;

    j_number = cJSON_CreateNumber(value);

    if (j_number == NULL)
    {
        printf("cJSON_CreateNumber\n");
        return;
    }

    cJSON_AddItemToObject(j_obj, key, j_number);
}

cJSON *create_queries_json(PkgContext *context)
{
    cJSON *j_arr = NULL;

    j_arr = cJSON_CreateArray();

    if (j_arr == NULL)
    {
        printf("cJSON_CreateArray\n");
        return j_arr;
    }

    cJSON_AddItemToObject(context->datagram, LIST_QUERIES_KEY, j_arr);

    return j_arr;
}

void create_string_json(cJSON *j_obj, const char *key, const char *value)
{
    cJSON *str = NULL;

    str = cJSON_CreateString(value);

    if (str == NULL)
    {
        printf("cJSON_CreateString\n");
        return;
    }

    cJSON_AddItemToObject(j_obj, key, str);
}

unsigned short _msg_parse(const unsigned char *p_msg, NsSectDisp disp)
{
    return ntohs(*((u_int16_t *)(p_msg + disp)));
}

unsigned short questions_to_json(cJSON *j_arr, unsigned short q_number, ns_rr *rr)
{
    unsigned short byte_counter = 0;
    cJSON *j_question = cJSON_CreateObject();

    if (j_question == NULL)
    {
        printf("questions_to_json error\n");
        return byte_counter;
    }

    cJSON_AddItemToArray(j_arr, j_question);

    create_string_json(j_question, "name", ns_rr_name(*rr));
    byte_counter += strlen(ns_rr_name(*rr)) + 2;

    arithmetic_to_json(j_question, "type", ns_rr_type(*rr));
    arithmetic_to_json(j_question, "class", ns_rr_class(*rr));
    byte_counter += 4;
    return byte_counter;
}

void datagram_to_json(PkgContext *context, struct sockaddr_in *addr, char *buf, int buf_len)
{
    ns_msg msg;
    ns_rr rr;
    int result;
    unsigned short msg_count;
    unsigned short i;
    cJSON *j_queries;
    unsigned short byte_counter = 0;
    char *add_section;
    unsigned char *msg_ptr;

    if (context->datagram == NULL)
    {
        printf("cJSON_CreateObject error\n");
        return;
    }

    arithmetic_to_json(context->datagram, "addr", addr->sin_addr.s_addr);
    arithmetic_to_json(context->datagram, "port", ntohs(addr->sin_port));

    result = ns_initparse(buf, buf_len, &msg);

    if (result == -1)
    {
        printf("ns_initparse");
        return;
    }

    msg_count = ns_msg_count(msg, ns_s_qd);

    arithmetic_to_json(context->datagram, "transaction", msg._id);
    arithmetic_to_json(context->datagram, "flags", msg._flags);
    arithmetic_to_json(context->datagram, "questions", msg_count);
    arithmetic_to_json(context->datagram, "answer", _msg_parse(msg._msg, ns_answer));
    arithmetic_to_json(context->datagram, "authority", _msg_parse(msg._msg, ns_authority));
    arithmetic_to_json(context->datagram, "additional", _msg_parse(msg._msg, ns_additional));
    byte_counter += 12;

    j_queries = create_queries_json(context);
    
    for (i = 0; i < msg_count; i++)
    {
        result = ns_parserr(&msg, ns_s_qd, i, &rr);

        if (result < 0)
        {
            printf("ns_parserr error");
            return;
        }

        byte_counter += questions_to_json(j_queries, i, &rr);

        result = ns_skiprr(msg._msg_ptr, msg._eom, ns_s_qd, 1);

        if (result < 0)
        {
            break;
        }

        msg._msg_ptr += result;
    }

    if(byte_counter < buf_len)
    {
        msg_ptr = (unsigned char*)(msg._msg + 4 + (buf_len - byte_counter));

        byte_counter = buf_len - byte_counter;

        add_section = calloc((byte_counter * 2) + 1, sizeof(char));

        for(i = 0; i < byte_counter; i++)
        {
            sprintf((add_section + (i * 2)), "%02x", *(msg_ptr++));
        }

        if(cJSON_AddStringToObject(context->datagram, "add_section", add_section) == NULL)
        {
            printf("Additioanal section error");
            free(add_section);
            return;
        }

        free(add_section);
    }

    context->json_dump = cJSON_PrintUnformatted(context->datagram);
}

struct DNS_HEADER
{
    unsigned short id; // identification number

    unsigned short flags;

    unsigned short q_count;
    unsigned short ans_count;
    unsigned short auth_count;
    unsigned short add_count;
};

struct QUESTION
{
    unsigned short qtype;
    unsigned short qclass;
};

void ChangetoDnsNameFormat(unsigned char *dns, unsigned char *host)
{
    int lock = 0, i;
    strcat((char *)host, ".");

    for (i = 0; i < strlen((char *)host); i++)
    {
        if (host[i] == '.')
        {
            *dns++ = i - lock;
            for (; lock < i; lock++)
            {
                *dns++ = host[lock];
            }
            lock++;
        }
    }
    *dns++ = '\0';
}

unsigned short json_to_question(unsigned char *qinfo, const cJSON *j_info_array)
{
    unsigned short len = 0;
    unsigned short tmp;
    const cJSON *j_info;
    const cJSON *j_item;

    j_item = cJSON_GetArrayItem(j_info_array, 0);

    j_info = cJSON_GetObjectItemCaseSensitive(j_item, "name");

    if ((j_info == NULL) || (!cJSON_IsString(j_info)))
    {
        fprintf(stderr, "json_to_question:cJSON_GetArrayItem error: 'name'\n");
        return 0;
    }

    ChangetoDnsNameFormat(qinfo, j_info->valuestring);

    len += strlen((const char *)qinfo) + 1;
    qinfo += len;

    j_info = cJSON_GetObjectItemCaseSensitive(j_item, "type");

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        fprintf(stderr, "str_to_register error: 'type'\n");
        return 0;
    }
    *((unsigned short *)qinfo) = htons(j_info->valueint);

    qinfo += sizeof(unsigned short);

    j_info = cJSON_GetObjectItemCaseSensitive(j_item, "class");

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        fprintf(stderr, "str_to_register error: 'class'\n");
        return 0;
    }
    *((unsigned short *)qinfo) = htons(j_info->valueint);

    return len += 4;
}

bool str_to_register(unsigned short *reg, const cJSON *json, const char *key)
{
    const cJSON *j_int = cJSON_GetObjectItemCaseSensitive(json, key);

    if (cJSON_IsNumber(j_int) && (j_int != NULL))
    {
        *reg = (unsigned short)j_int->valueint;
        return true;
    }

    return false;
}

bool json_to_datagram(char *packet, unsigned char *resolv, int *res_len, int buf_len)
{
    cJSON *datagram_json = cJSON_Parse(packet);
    const cJSON *j_arr = NULL;
    cJSON *j_string;
    struct DNS_HEADER *dns = NULL;
    struct QUESTION *qinfo = NULL;
    unsigned char *qname;
    unsigned short tmp;
    int i;
    int len = 0;
    char test_str[2];

    if (datagram_json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return false;
    }

    dns = (struct DNS_HEADER *)resolv;

    dns->id = (unsigned short)htons(getpid());

    unsigned short *reg_ptr = &(dns->flags);

    for (i = 0; i < (sizeof(query_json_keys)/sizeof(const char *)); i++)
    {
        if (!str_to_register(&tmp, datagram_json, query_json_keys[i]))
        {
            fprintf(stderr, "str_to_register error: '%s'\n", query_json_keys[i]);
            *res_len = 0;
            cJSON_Delete(datagram_json);
            return false;
        }
        *(reg_ptr + i) = htons(tmp);
    }

    // pointer of the query
    *res_len = sizeof(struct DNS_HEADER);
    qname = resolv + *res_len;

    j_arr = cJSON_GetObjectItemCaseSensitive(datagram_json, LIST_QUERIES_KEY);

    tmp = json_to_question(qname, j_arr);

    if(tmp == 0)
    {
        *res_len = 0;
        cJSON_Delete(datagram_json);
        return false;
    }

    *res_len += tmp;
    qname = qname + tmp;
    
    j_string = cJSON_GetObjectItemCaseSensitive(datagram_json, "add_section");

    if (cJSON_IsString(j_string) && (j_string->valuestring != NULL))
    {
        for(i = 0; i < strlen(j_string->valuestring); i+=2)
        {
            test_str[0] = *(j_string->valuestring + i);
            test_str[1] = *(j_string->valuestring + (i + 1));

            if(sscanf(test_str, "%hhx", qname+i/2) != 1)
            {
                cJSON_Delete(datagram_json);
                *res_len = 0;
                return false;
            }
        }
    }

    *res_len += strlen(j_string->valuestring) / 2;

    cJSON_Delete(datagram_json);
    return true;
}

bool json_to_response(
    unsigned long *out_host, unsigned short *out_port,
    unsigned char *buf, int *buf_len, char *packet)
{
    cJSON *datagram_json = cJSON_Parse(packet);
    cJSON *j_string;
    cJSON *j_int;
    int i;
    char test_str[2];
    unsigned short out_transaction;

    j_int = cJSON_GetObjectItemCaseSensitive(datagram_json, "addr");

    if (!cJSON_IsNumber(j_int) || (j_int == NULL))
    {
        cJSON_Delete(datagram_json);
        return false;
    }

    *out_host = (unsigned long)j_int->valuedouble;

    if(!str_to_register(out_port, datagram_json, "port"))
    {
        cJSON_Delete(datagram_json);
        return false;
    }

    if(!str_to_register(&out_transaction, datagram_json, "transaction"))
    {
        cJSON_Delete(datagram_json);
        return false;
    }

    j_string = cJSON_GetObjectItemCaseSensitive(datagram_json, "datagram");
    if(!cJSON_IsString(j_string) || (j_string->valuestring == NULL))
    {
        cJSON_Delete(datagram_json);
        return false;
    }

    for(i = 0; i < strlen(j_string->valuestring); i+=2)
    {
        test_str[0] = *(j_string->valuestring + i);
        test_str[1] = *(j_string->valuestring + (i + 1));

        if(sscanf(test_str, "%hhx", buf+i/2) != 1)
        {
            cJSON_Delete(datagram_json);
            return false;
        }
    }

    *buf_len = strlen(j_string->valuestring) / 2;

    *((unsigned short*)buf) = htons(out_transaction);

    cJSON_Delete(datagram_json);
    return true;
}

bool response_to_json(
    char *buf, unsigned long out_host, unsigned short out_port,
    unsigned short out_transaction, char *message)
{
    char *string = NULL;
    cJSON *response_json = cJSON_CreateObject();

    if(cJSON_AddNumberToObject(response_json, "addr", out_host) == NULL)
    {
        cJSON_Delete(response_json);
        return false;
    }

    if(cJSON_AddNumberToObject(response_json, "port", out_port) == NULL)
    {
        cJSON_Delete(response_json);
        return false;
    }

    if(cJSON_AddNumberToObject(response_json, "transaction", out_transaction) == NULL)
    {
        cJSON_Delete(response_json);
        return false;
    }

    if(cJSON_AddStringToObject(response_json, "datagram", message) == NULL)
    {
        cJSON_Delete(response_json);
        return false;
    }

    string = cJSON_PrintUnformatted(response_json);
    strcpy(buf, string);
    cJSON_Delete(response_json);
    return true;
}

bool json_get_addr(
    char *packet, unsigned long *addr, unsigned short *port, unsigned short *transaction)
{
    cJSON *datagram_json = cJSON_Parse(packet);
    cJSON *j_info;

    j_info = cJSON_GetObjectItemCaseSensitive(datagram_json, "addr");

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        fprintf(stderr, "json_get_addr error: 'addr'\n");
        cJSON_Delete(datagram_json);
        return 0;
    }
    *addr = j_info->valuedouble;

    j_info = cJSON_GetObjectItemCaseSensitive(datagram_json, "port");

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        fprintf(stderr, "json_get_addr error: 'port'\n");
        cJSON_Delete(datagram_json);
        return 0;
    }
    *port = j_info->valueint;

    j_info = cJSON_GetObjectItemCaseSensitive(datagram_json, "transaction");

    if ((j_info == NULL) || (!cJSON_IsNumber(j_info)))
    {
        fprintf(stderr, "json_get_addr error: 'transaction'\n");
        cJSON_Delete(datagram_json);
        return 0;
    }
    *transaction = j_info->valueint;

    cJSON_Delete(datagram_json);
    return true;
}
