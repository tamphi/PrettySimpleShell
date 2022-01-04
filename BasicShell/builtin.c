#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#include "builtin.h"
#include "parse.h"

static char* builtin[] = {
    "exit",   /* exits the shell */
    "which",  /* displays full path to command */
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

static int check_which(Task T){
   int i;
   for (i = 0; builtin[i];i++){
        if (!strcmp (T.argv[1], builtin[i]))
           return 5;
   }
   return 0;
}
void builtin_execute_1(Parse * P){
    int t =0;
    while(P->tasks[0].argv[t]){
       t +=1;
    }
    if (t >2) return;
   if (!strcmp(P->tasks[0].cmd,"exit"))
         exit(EXIT_SUCCESS);

   char *cmd = P->tasks[0].cmd;
   char **argv = P->tasks[0].argv;


   int fopen1 = creat(P->outfile, 0644); 
   int fopen2 = open(P->infile, 0); 

      switch(fork()){
         case -1:
            fprintf(stderr,"failed to fork() \n");
            exit(EXIT_FAILURE);
         case 0:
            if(fopen2) dup2(fopen2,STDIN_FILENO);
            if(fopen1) dup2(fopen1,STDOUT_FILENO);
            if(fopen2&&fopen1){
               dup2(fopen2,STDIN_FILENO);
               dup2(fopen1,STDOUT_FILENO);
            }

            (check_which(P->tasks[0]) != 0) ? execlp("echo","echo",argv[1],": shell built-in command",NULL)
                                             : execvp(cmd,argv);
         default:
            wait(NULL);
      }
   close(fopen1);close(fopen2);
}

void builtin_execute (Task T)
{
    int t =0;
    while(T.argv[t]){
       t +=1;
    }
    if (t >2) return;
    if (!strcmp (T.cmd, "exit")) {
        exit (EXIT_SUCCESS);
    }
    
    execvp(T.cmd,T.argv);
}
