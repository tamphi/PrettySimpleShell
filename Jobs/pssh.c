#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <signal.h>
#include <fcntl.h>

#include "builtin.h"
#include "parse.h"
#include "job.h"

/*******************************************
 * Set to 1 to view the command line parse *
 *******************************************/
#define DEBUG_PARSE 0
#define READ_SIDE 0
#define WRITE_SIDE 1
  
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
	return "$ ";
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

 	if (access (cmd, F_OK) == 0)
        return 1;

    PATH = strdup (getenv("PATH"));

    for (tmp=PATH; ; tmp=NULL) {
        dir = strtok_r (tmp, ":", &state);
        if (!dir)
            break;

        strncpy (probe, dir, PATH_MAX);
        strncat (probe, "/", PATH_MAX);
        strncat (probe, cmd, PATH_MAX);

        if (access (probe, F_OK) == 0) {
            ret = 1;
            break;
        }
    }

    free (PATH);
    return ret;
}

static int close_safe(int fd){
   if ( (fd != STDIN_FILENO) && (fd != STDOUT_FILENO) ){
      return close(fd);
   }
   return -1;
}
static void redirect(int fd_new, int fd_old){
   if (fd_new != fd_old){
      dup2(fd_new,fd_old);
      close(fd_new);
   }
}

static int get_infile (Parse *P){
   if (P->infile)
      return open(P->infile,0);
   else
      return STDIN_FILENO;
}

static int get_outfile (Parse *P){
   if (P->outfile)
      return creat(P->outfile,0644);
   else
      return STDOUT_FILENO;
}


/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */

void execute_tasks (Parse* P, int number)
{
   unsigned int t;
   int infile,outfile;
   int prev_pipe;
   pid_t* pid;

    signal(SIGCHLD, handler);
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    JOB_INFO[number].pids = malloc(P->ntasks * sizeof(*pid));
    JOB_INFO[number].npids = P->ntasks;
    JOB_INFO[number].total_npids = P->ntasks;
    pid = JOB_INFO[number].pids;
    for (t = 0; t < P->ntasks; t++) {
       int fd[2];
       pipe(fd);
       pid[t]  = fork();
       setpgid(pid[t], pid[0]); // Set group PID				
       if ((t == 0) && (pid[t] > 0)) {
          JOB_INFO[number].pgid = pid[0];
          if (JOB_INFO[number].status != BG){
             put_in_FG(JOB_INFO[number].pgid);
          } 
          else put_in_FG(PSSH_PGID);	
         
       }
		
       if (pid[t] == 0) {
          if (t == 0 && P->ntasks > 1) {     
             redirect(fd[WRITE_SIDE],STDOUT_FILENO);
          } 
          else if (t < (P->ntasks-1) && P->ntasks > 1) {
             redirect(prev_pipe,STDIN_FILENO);
             redirect(fd[WRITE_SIDE],STDOUT_FILENO);
          } else if (t == (P->ntasks-1) && P->ntasks > 1) {
             redirect(prev_pipe,STDIN_FILENO);
          }
          if ( t == (P->ntasks-1)){
             outfile = get_outfile(P);
             redirect(outfile,STDOUT_FILENO);
             close_safe(outfile);
          }
          if (P->infile){
             infile = get_infile(P);
             redirect(infile,STDIN_FILENO);
             close_safe(infile);
          }
          if (command_found (P->tasks[t].cmd)){
             execvp(P->tasks[t].cmd,&P->tasks[t].argv[0]);
          }
          if (is_builtin (P->tasks[t].cmd)) {
             builtin_execute (P->tasks[t]);
          }              
       } else {
          if (prev_pipe != 0)  close(prev_pipe);
          close(fd[WRITE_SIDE]);
          if (t == (P->ntasks-1)) close(fd[READ_SIDE]);
          prev_pipe = fd[READ_SIDE];
       }
    }    
    if (JOB_INFO[number].status == BG ){
       printf("[%d] ", number);
       for (t = 0; t < JOB_INFO[number].npids; t++) {
          printf("%d ", pid[t]);
       }   
       printf("\n");	
    }
}


int main (int argc, char** argv)
{
    char* cmdline;
    Parse* P;
    int number;
    PSSH_PGID = getpgrp();
    print_banner ();
    while (1) {
       while(tcgetpgrp(STDIN_FILENO) != getpid()){
          pause();
       }
       signal(SIGINT,SIG_IGN);
       signal(SIGQUIT, SIG_IGN);
       signal(SIGTSTP, SIG_IGN);
       signal(SIGTTIN, SIG_IGN);
       signal(SIGTTOU, SIG_IGN);	
       cmdline = readline (build_prompt());
        if (!cmdline)       /* EOF (ex: ctrl-d) */
            exit (EXIT_SUCCESS);
		
        int size = sizeof(cmdline);
        char dup_cmdline[size];
        strcpy(dup_cmdline,cmdline);	
        if (strcmp(cmdline, "exit")== 0){
           free(cmdline);
           exit(EXIT_SUCCESS);
        }

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
		if (strcmp(P->tasks[0].cmd, "jobs")==0){
			job_exec(JOB_INFO);
			goto next;
		} else if (strcmp(P->tasks[0].cmd, "fg")==0){
			fg_exec(JOB_INFO,P);
			goto next;
		} else if (strcmp(P->tasks[0].cmd, "bg")==0){
			bg_exec(JOB_INFO,P);
			goto next;
		} else if (strcmp(P->tasks[0].cmd, "kill")==0){
			kill_exec(JOB_INFO,P);
			goto next;
		}

		number = parse_job(JOB_INFO, P, dup_cmdline);
		execute_tasks (P, number);
   		
	next:
        parse_destroy (&P);
        free(cmdline);
    }
}
