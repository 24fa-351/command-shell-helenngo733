#ifndef CMD_SHELL_H
#define CMD_SHELL_H

#define MAX_INPUT_LEN 1024
#define MAX_ARGS 100
#define MAX_ENV_VARS 100

extern char *env_vars_name[MAX_ENV_VARS];
extern char *env_var_value[MAX_ENV_VARS];
extern int env_count;

char *get_env_value(const char *var_name);
void set_env_value(const char *var_name, const char *var_value);
void unset_env_values(const char *var_name);
void replace_env_vars(char *cmd);
void piping(char *cmd);
void commands(char *cmd);

#endif
