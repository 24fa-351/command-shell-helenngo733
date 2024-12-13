#include "cmd_shell.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "path.h"

char *env_vars_name[MAX_ENV_VARS];
char *env_var_value[MAX_ENV_VARS];
int env_count = 0;

// Implemented the setting, deleting (unsetting) and getting of xsh environment vars:
char *get_env_value(const char *var_name) {
    for (int ix = 0; ix < env_count; ix++)
        if (strcmp(env_vars_name[ix], var_name) == 0)
            return env_var_value[ix];
    return NULL;
}

void set_env_value(const char *var_name, const char *var_value) {
    for (int ix = 0; ix < env_count; ix++) {
        if (strcmp(env_vars_name[ix], var_name) == 0) {
            free(env_var_value[ix]);
            env_var_value[ix] = strdup(var_value);
            return;
        }
    }
    if (env_count < MAX_ENV_VARS) {
        env_vars_name[env_count] = strdup(var_name);
        env_var_value[env_count] = strdup(var_value);
        env_count++;
    }
}

void unset_env_values(const char *var_name) {
    for (int ix = 0; ix < env_count; ix++) {
        if (strcmp(env_vars_name[ix], var_name) == 0) {
            free(env_vars_name[ix]);
            free(env_var_value[ix]);
            for (int jx = ix; jx < env_count - 1; jx++) {
                env_vars_name[jx] = env_vars_name[jx + 1];
                env_var_value[jx] = env_var_value[jx + 1];
            }
            env_count--;
            return;
        }
    }
}

// Scan the command for a $<something> and replace it if found
void replace_env_vars(char *cmd) {
    char buffer[MAX_INPUT_LEN] = {0};
    char *ptr = cmd;
    char *dest_ptr = buffer;

    while (*ptr) {
        if (*ptr == '$') {
            ptr++;
            char var_name[MAX_INPUT_LEN] = {0};
            int counter = 0;

            while (*ptr && ((*ptr >= 'A' && *ptr <= 'Z') || (*ptr >= 'a' && *ptr <= 'z') ||
                            (*ptr == '_'))) {
                var_name[counter++] = *ptr++;
            }
            var_name[counter] = '\0';

            char *var_value = get_env_value(var_name);
            if (var_value)
                while (*var_value) *dest_ptr++ = *var_value++;
        } else {
            *dest_ptr++ = *ptr++;
        }
    }

    *dest_ptr = '\0';
    strcpy(cmd, buffer);
}

// Implemented "|" to separate commands and pipe output from one to the input of another
void piping(char *cmd) {
    char *commands[MAX_ARGS];
    int num_of_cmd = 0;

    split(cmd, commands, '|');

    while (commands[num_of_cmd] != NULL) num_of_cmd++;

    int pipe_fd[2 * (num_of_cmd - 1)];

    for (int ix = 0; ix < num_of_cmd - 1; ix++) {
        if (pipe(pipe_fd + 2 * ix) == -1) {
            perror("piping failed");
            exit(1);
        }
    }

    for (int ix = 0; ix < num_of_cmd; ix++) {
        pid_t pid = fork();
        if (pid == 0) {
            if (ix > 0)
                dup2(pipe_fd[2 * (ix - 1)], STDIN_FILENO);
            if (ix < num_of_cmd - 1)
                dup2(pipe_fd[2 * ix + 1], STDOUT_FILENO);

            for (int jx = 0; jx < 2 * (num_of_cmd - 1); jx++) close(pipe_fd[jx]);

            char *args[MAX_ARGS];
            split(commands[ix], args, ' ');
            execvp(args[0], args);
            perror("failed to execute");
            exit(1);
        }
    }
    for (int ix = 0; ix < 2 * (num_of_cmd - 1); ix++) close(pipe_fd[ix]);
    for (int ix = 0; ix < num_of_cmd; ix++) wait(NULL);
}

// cd, pwd, &, <, >
void commands(char *cmd) {
    char *args[MAX_ARGS];
    char *file_input = NULL;
    char *file_output = NULL;
    int background = 0;
    int redir_input = 0;
    int redir_output = 0;

    split(cmd, args, ' ');

    for (int ix = 0; args[ix] != NULL; ix++) {
        if (strcmp(args[ix], "&") == 0) {
            background = 1;
            args[ix] = NULL;
        } else if (strcmp(args[ix], "<") == 0) {
            redir_input = 1;
            file_input = args[++ix];
            args[ix - 1] = NULL;
        } else if (strcmp(args[ix], ">") == 0) {
            redir_output = 1;
            file_output = args[++ix];
            args[ix - 1] = NULL;
        }
    }

    // Implemented "cd" and "pwd" (directly). cd must take a full or relative path
    // includes set, unset, echo
    if (strcmp(args[0], "cd") == 0) {
        if (chdir(args[1]) != 0)
            perror("cd error");
        return;
    }
    if (strcmp(args[0], "pwd") == 0) {
        char cwd[MAX_INPUT_LEN];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("%s\n", cwd);
        else
            perror("pwd error");
        return;
    }
    if (strcmp(args[0], "set") == 0) {
        set_env_value(args[1], args[2]);
        return;
    }
    if (strcmp(args[0], "unset") == 0) {
        unset_env_values(args[1]);
        return;
    }
    if (strcmp(args[0], "echo") == 0) {
        for (int jx = 1; args[jx] != NULL; jx++) {
            replace_env_vars(args[jx]);
            printf("%s ", args[jx]);
        }
        printf("\n");
        return;
    }

    char absolute_path[MAX_INPUT_LEN];
    if (find_absolute_path(args[0], absolute_path))
        args[0] = absolute_path;

    // read the PATH environment variable and execute command if found
    pid_t pid = fork();
    if (pid == 0) {
        // input redir
        if (redir_input) {
            int input_fd = open(file_input, O_RDONLY);
            if (input_fd == -1) {
                perror("failed to open input file");
                exit(1);
            }
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }

        // output redir
        if (redir_output) {
            int output_fd = open(file_output, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (output_fd == -1) {
                perror("failed to open output file");
                exit(1);
            }
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        // execute the command
        execvp(args[0], args) == -1;
        perror("command not found");
        exit(1);

    } else {
        if (background)
            printf("running in background: process %d\n", pid);
        else
            waitpid(pid, NULL, 0);
    }
}