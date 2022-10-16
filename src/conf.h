#ifndef CONF_H
#define CONF_H

#include <stdio.h>
// #include "../Yams/Sources/CYaml/include/yaml.h"

struct conf
{
    const char *fname;         
    FILE *fh;
    // yaml_parser_t parser;
    // yaml_event_t event;
    // yaml_token_t token;
};

struct MainOpt
{
    const char *fname;
};

void open_conf(struct conf * cnf, const char *fname);
void get_main_options(struct MainOpt *options);

#endif /* #ifndef CONF_H */
