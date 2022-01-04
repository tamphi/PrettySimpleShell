#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>
#include <readline/readline.h>

#include "builtin.h"
#include "parse.h"

/*******************************************
 * Set to 1 to view the command line parse *
 *******************************************/
#define DEBUG_PARSE 0

void print_banner ()
{
    printf ("                    ________   \n");
    printf ("_________________________  /_  \n");
    printf ("___  __ \\_  ___/_  ___/_  __ \\ \n");
    printf ("__  /_/ /(__  )_(__  )_  / / / \n");
    printf ("_  .___//____/ /____/ /_/ /_/  \n");
    printf ("/_/ Type 'exit' or ctrl+c to quit\n\n");
}


/* returns a string for building the prompt
 *
 * Note:
 *   If you modify this function to return a string on the heap,
 *   be sure to free() it later when appropirate!  */
static char* build_prompt ()
{
    return  "$ ";
}


/* return true if command is found, either:
 *   - a valid fully qualified path was supplied to an existing file
 *   - the executable file was found in the system's PATH
 * false is returned otherwise */
static int command_found (const char* cmd)
{
    char* dir;
    char* tmp;
    char* PATH;
    char* state;
    char probe[PATH_MAX];

    int ret = 0;

    if (access (cmd, X_OK) == 0)
        return 1;

    PATH = strdup (getenv("PATH"));

    for (tmp=PATH; ; tmp=NULL) {
        dir = strtok_r (tmp, ":", &state);
        if (!dir)
            break;

        strncpy (probe, dir, PATH_MAX-1);
        strncat (probe, "/", PATH_MAX-1);
        strncat (probe, cmd, PATH_MAX-1);

        if (access (probe, X_OK) == 0) {
            ret = 1;
            break;
        }
    }

    free (PATH);
    return ret;
}


/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */
void execute_tasks (Parse* P)
{
    unsigned int t;
    unsigned int tasks=0;

    if (P->ntasks < 2){
       if (is_builtin (P->tasks[0].cmd) )
          builtin_execute_1(P);
       else if (command_found(P->tasks[0].cmd) )
          execute_cmd(P);
       else
          printf("pssh: command not found :%s \n", P ->tasks[t].cmd);
    }

    //if multi cmd!
    else{

       for (t = 0; t < P->ntasks; t++){
          if (is_builtin (P->tasks[t].cmd) || command_found(P->tasks[t].cmd))
                tasks +=1;
          else{
             printf("pssh: command not found %s\n", P->tasks[t].cmd);
             break;
          }
        }
    }
       
    if (tasks == P->ntasks) multi_cmd(P);
}

void execute_cmd(Parse * P){

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
            execvp(cmd,argv);
         default:
            wait(NULL);
      }
      close(fopen1); close(fopen2);

}

void run (Task * T, int infile, int outfile){
   //if infile is not yet STDIN
   if (infile != STDIN_FILENO){
      dup2(infile,STDIN_FILENO);
      close(infile);
   }
   //if outfile is not yet STDOUT
   if (outfile != STDOUT_FILENO){
      dup2(outfile,STDOUT_FILENO);
      close(outfile);
   }
   if (is_builtin(T->cmd))
     builtin_execute(*T); 
   else
      execvp(T->cmd,T->argv);
}

void multi_cmd(Parse * P){

   int fd[2];
   pid_t * pid = malloc (P->ntasks * sizeof(pid_t));
   
   int infile, outfile;

   //if there's no input file, infile = stdin 
   //else infile = input file from user!
   infile = (P->infile == NULL) ? STDIN_FILENO : open(P->infile,0);

   int t;
   for (t=0; t < P->ntasks -1; t++){
      pipe(fd);
      pid[t] = fork();

      //child process
      if (pid[t] == 0){
         close(fd[READ]); // close read side of child
         //execute and perform redirection
         run(&P->tasks[t],infile, fd[WRITE]);
      }

      close(fd[WRITE]); //close write side of child

      //close infile if not output and input file descriptor!
      if ( (infile != STDIN_FILENO) && (infile != STDOUT_FILENO) ){
         close(infile);
      }

      infile = fd[READ];
   }
   //if there is no output file, outfile = STDOUT
   outfile = (P->outfile == NULL) ? STDOUT_FILENO : creat(P->outfile, 0644);
   
   //execute last command!
   pid[t] = fork();
   
   //child process
   if (pid[t] == 0)
      run(&P->tasks[t],infile,outfile);

   //parent
   //wait for all child!
   for (t=0; t < P->ntasks; t++)
      wait(NULL);

   if ( (infile != STDIN_FILENO) && (infile != STDOUT_FILENO) )
      close(infile);

   if ( (outfile != STDIN_FILENO) && (outfile != STDOUT_FILENO) )
      close(outfile);

   free(pid);

}



int main (int argc, char** argv)
{
    char* cmdline;
    Parse* P;

    print_banner ();

    while (1) {
        cmdline = readline (build_prompt());
        if (!cmdline)       /* EOF (ex: ctrl-d) */
            exit (EXIT_SUCCESS);

        P = parse_cmdline (cmdline);
        if (!P)
            goto next;

        if (P->invalid_syntax) {
            printf ("pssh: invalid syntax\n");
            goto next;
        }

#if DEBUG_PARSE
        parse_debug (P);
#endif

        execute_tasks (P);

    next:
        parse_destroy (&P);
        free(cmdline);
    }
}
