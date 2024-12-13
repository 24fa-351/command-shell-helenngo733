#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_shell.h"

int main() {
    char input[MAX_INPUT_LEN];

    while (1) {
        printf("xsh# ");
        if (!fgets(input, sizeof(input), stdin)) {
            perror("failed to read input");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0)
            break;

        if (strchr(input, '|')) {
            piping(input);
        } else {
            replace_env_vars(input);
            commands(input);
        }
    }
    return 0;
}