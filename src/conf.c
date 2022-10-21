#include "conf.h"
#include <stdio.h>
#include <yaml.h>

#define MAX_LEN_VALUE 256

typedef struct
{
    const char *sec;
    const char *key;
    void *val;
    bool(*converter)(void **val,  char *input);
}ConfItems;

static yaml_parser_t parser;

void get_main_options(struct MainOpt *options)
{
    options->config_file_name = "server.yml";
    options->blacklist_file_name = "blacklist.yml";
}

bool block_search(yaml_token_t *token, const char *name)
{
    do
    {
        yaml_token_delete(token);
        yaml_parser_scan(&parser, token);

        switch ((*token).type)
        {
            case YAML_BLOCK_MAPPING_START_TOKEN:
                break;
            case YAML_KEY_TOKEN:
                break;
            case YAML_SCALAR_TOKEN:
                if(strcmp((*token).data.scalar.value, name) == 0)
                {
                    return true;
                }
                break;

            default:
                

        }
    } while (((*token).type != YAML_STREAM_END_TOKEN) && ((*token).type != YAML_NO_TOKEN));

    return false;
}

bool key_search(yaml_token_t *token, const char *name)
{
    do
    {
        yaml_token_delete(token);
        yaml_parser_scan(&parser, token);

        switch ((*token).type)
        {
            case YAML_KEY_TOKEN:
                break;
            case YAML_SCALAR_TOKEN:
                if(name == NULL)
                {
                    return true;
                }

                if(strcmp((*token).data.scalar.value, name) == 0)
                {
                    return true;
                }
                break;
            case YAML_BLOCK_MAPPING_START_TOKEN:
                break;

            default:
                
        }
    } while (((*token).type != YAML_STREAM_END_TOKEN) && ((*token).type != YAML_NO_TOKEN));

    return false;
}

bool get_value(yaml_token_t *token, char *value)
{
    do
    {
        yaml_token_delete(token);
        yaml_parser_scan(&parser, token);

        switch ((*token).type)
        {
            case YAML_VALUE_TOKEN:
                break;
            case YAML_SCALAR_TOKEN:
                strcpy(value, (*token).data.scalar.value);
                return true;

            default:
                

        }
    } while (((*token).type != YAML_STREAM_END_TOKEN) && ((*token).type != YAML_NO_TOKEN));

    return false;
}

bool get_scalar(char *result, const char *section, const char *key, yaml_token_t *token)
{
    static char current_section[MAX_LEN_VALUE];

    if(strcmp(current_section, section) != 0)
    {
        // strncpy(current_section, section, MAX_LEN_VALUE);
        strcpy(current_section, section);

        if(!block_search(token, section))
        {
            fprintf(stderr, "Section '%s' not found\n", section);
            return false;
        }
    }

    if(!key_search(token, key))
    {
        fprintf(stderr, "Key '%s' not found\n", key);
        return false;
    }

    if(!get_value(token, result))
    {
        fprintf(stderr, "There is not value with '%s'\n", key);
        return false;
    }

    return true;
}

bool scalar_to_int(void **val,  char *input)
{
    if(sscanf(input,"%d", ((int*)(val))) != 1)
    {
        fprintf(stderr, "The value is wrong '%s'\n", input);
        return false;
    }

    return true;
}

bool scalar_to_char(void **val,  char *input)
{
    int len = strlen(input);

    if(len == 0)
    {
        return false;
    }

    *val = calloc(len + 1, sizeof(char));

    if(*val == NULL)
    {
        return false;
    }

    strcpy(*val, input);

    return true;
}

bool open_conf(MainConf *cnf, const char *fname)
{
    ConfItems items[] = {
        {"Upstream DNS",    "host",     &(cnf->upstream_dns_host), scalar_to_char},
        {"Upstream DNS",    "port",     &(cnf->upstream_dns_port), scalar_to_int},
        {"Storage",         "host",     &(cnf->storage_host),   scalar_to_char},
        {"Storage",         "port",     &(cnf->storage_port),   scalar_to_int},
        {"Filter",          "addr",     &(cnf->answ_addr),      scalar_to_char},
        {"Server",          "listen",   &(cnf->listen_host),    scalar_to_char},
        {"Server",          "port",     &(cnf->listen_port),    scalar_to_int},
        {"Client",          "number",   &(cnf->clients_number), scalar_to_int},
        {"Client",          "timeout",  &(cnf->client_timeout), scalar_to_int},
    };

    FILE *fd;
    yaml_token_t token;
    char result[MAX_LEN_VALUE];
    int val;
    int i;

    fd = fopen(fname, "r");
    if (fd == NULL)
    {
        fprintf(stderr, "Config file open unable %s\n", fname);
        return false;
    }

    if (!yaml_parser_initialize(&parser))
    {
        fprintf(stderr, "Failed to initialize parser!\n");
    }
    yaml_parser_set_input_file(&parser, fd);

    for(i = 0; i < sizeof(items)/sizeof(ConfItems); i++)
    {
        if(get_scalar(result, items[i].sec, items[i].key, &token))
        {
            items[i].converter(items[i].val, result);
        }

        yaml_token_delete(&token);
    }

    yaml_parser_delete(&parser);

    pclose(fd);
    return true;
}

void free_conf(MainConf *cnf)
{
    yaml_parser_delete(&parser);

    free(cnf->storage_host);
    free(cnf->upstream_dns_host);
    free(cnf->answ_addr);
    free(cnf->listen_host);
}

bool open_blacklist(redisContext *srg, const char *fname)
{
    FILE *fd;
    yaml_token_t token;

    fd = fopen(fname, "r");
    if (fd == NULL)
    {
        fprintf(stderr, "Blacklist file open unable %s\n", fname);
        return false;
    }

    if (!yaml_parser_initialize(&parser))
    {
        fprintf(stderr, "Failed to initialize parser!\n");
    }
    yaml_parser_set_input_file(&parser, fd);

    while(key_search(&token, NULL))
    {
        write_set(srg, NS_COLLECTION_KEY, token.data.scalar.value);
    }

    // fprintf(stderr, "There in not any key\n");

    yaml_parser_delete(&parser);
    pclose(fd);

    return true;
}
