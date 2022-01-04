#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "parse.h"

static char* builtin[] = {
    "exit",   /* exits the shell */
    "which",  /* displays full path to command */
    "fg",
    "bg",
    "jobs",
    "kill",
    NULL
};


int is_builtin (char* cmd)
{
    int i;

    for (i=0; builtin[i]; i++) {
        if (!strcmp (cmd, builtin[i]))
            return 1;
    }

    return 0;
}


void builtin_execute (Task T)
{
    if (!strcmp (T.cmd, "exit")) {
       exit (EXIT_SUCCESS);

    } else if (!strcmp(T.cmd, "which")){
       if (!(T.argv[1])){
          exit(EXIT_SUCCESS);
       }
       if (is_builtin(T.argv[1])){
          printf("%s: shell built-in command\n", T.argv[1]);
          exit(EXIT_SUCCESS);
       }
       execvp(T.cmd,T.argv);
    } else {
        printf ("pssh: builtin command: %s (not implemented!)\n", T.cmd);
    }
}

