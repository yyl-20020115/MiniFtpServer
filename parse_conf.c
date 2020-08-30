#include "parse_conf.h"
#include "common.h"
#include "strutil.h"
#include "configure.h"
static void load_setting(const char *setting);

static struct bool_setting
{
    const char* p_setting_name;
    int* p_variable;
}
bool_array[] =
{
    { "pasv_enable", &tunable_pasv_enable },
    { "port_enable", &tunable_port_enable },
    { NULL, NULL }
};

static struct uint_setting
{
    const char* p_setting_name;
    unsigned int* p_variable;
}
uint_array[] =
{
    { "listen_port", &tunable_listen_port },
    { "max_clients", &tunable_max_clients },
    { "max_per_ip", &tunable_max_per_ip },
    { "accept_timeout", &tunable_accept_timeout },
    { "connect_timeout", &tunable_connect_timeout },
    { "idle_session_timeout", &tunable_idle_session_timeout },
    { "data_connection_timeout", &tunable_data_connection_timeout },
    { "local_umask", &tunable_local_umask },
    { "upload_max_rate", &tunable_upload_max_rate },
    { "download_max_rate", &tunable_download_max_rate },
    { NULL, NULL }
};

static struct str_setting
{
    const char* p_setting_name;
    const char** p_variable;
}
str_array[] =
{
    { "listen_address", &tunable_listen_address },
    { NULL, NULL }
};
void free_config()
{
    for (int i = 0; i < sizeof(str_array) / sizeof(struct str_setting); i++) {
        const char** p_cur_setting = (str_array+i)->p_variable;
        if (p_cur_setting !=0 && *p_cur_setting!=0) {
            free((char*)*p_cur_setting);
        }
    }
}
void load_config(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        exit_with_error("fopen");
        return;
    }
    char setting_line[1024] = {0};
    while (fgets(setting_line, sizeof(setting_line), fp) != NULL)
    {
        if (strlen(setting_line) == 0
                || setting_line[0] == '#'
                || str_all_space(setting_line))
            continue;

        str_trim_crlf(setting_line);
        load_setting(setting_line);
        memset(setting_line, 0, sizeof(setting_line));
    }

    fclose(fp);
}

static void load_setting(const char *setting)
{
    while (isspace(*setting))
        ++setting;

    char key[128] ={0};
    char value[128] = {0};
    str_split(setting, key, value, '=');
    if (strlen(value) == 0)
    {
        exit_with_error("mising value in config file for: %s\n", key);
        return;
    }

    {
        
        const struct str_setting *p_str_setting = str_array;
        while (p_str_setting->p_setting_name != NULL)
        {
            if (strcmp(key, p_str_setting->p_setting_name) == 0)
            {
                const char **p_cur_setting = p_str_setting->p_variable;
                if (p_cur_setting !=0 && *p_cur_setting!=0) {
                    free((char*)*p_cur_setting);
                }
                *p_cur_setting = _strdup(value);
                return;
            }

            ++p_str_setting;
        }
    }

    {
        const struct bool_setting *p_bool_setting = bool_array;
        while (p_bool_setting->p_setting_name != NULL)
        {
            if (strcmp(key, p_bool_setting->p_setting_name) == 0)
            {
                str_upper(value);
                if (strcmp(value, "YES") == 0
                        || strcmp(value, "TRUE") == 0
                        || strcmp(value, "1") == 0)
                    *(p_bool_setting->p_variable) = 1;
                else if (strcmp(value, "NO") == 0
                        || strcmp(value, "FALSE") == 0
                        || strcmp(value, "0") == 0)
                    *(p_bool_setting->p_variable) = 0;
                else
                {
                    exit_with_error("bad bool value in config file for: %s\n", key);
                }

                return;
            }

            ++p_bool_setting;
        }
    }

    {
        const struct uint_setting *p_uint_setting = uint_array;
        while (p_uint_setting->p_setting_name != NULL)
        {
            if (strcmp(key, p_uint_setting->p_setting_name) == 0)
            {
                if (value[0] == '0')
                    *(p_uint_setting->p_variable) = str_octal_to_uint(value);
                else
                    *(p_uint_setting->p_variable) = atoi(value);

                return;
            }

            ++p_uint_setting;
        }
    }
}

