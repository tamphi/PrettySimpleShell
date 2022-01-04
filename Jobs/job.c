
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
#include "job.h"


void remove_job(Job* JOB_INFO, int number)
{
	JOB_INFO[number].npids = 0;
	JOB_INFO[number].total_npids = 0;
	JOB_INFO[number].status = TERM;
        JOB_INFO[number].name = '\0';
        free(JOB_INFO[number].pids);
        //free(JOB_INFO[number].name);
}
int parse_job(Job* JOB_INFO, Parse* P, char* dup_cmdline){
	int i;
	for (i = 0; i < NUM_MAX; i++){
		if (JOB_INFO[i].npids == 0){
			if (P->background){
				JOB_INFO[i].status = BG;
			} else {
				JOB_INFO[i].status = FG;
			}
			JOB_INFO[i].name = dup_cmdline;
			//JOB_INFO[i].name = malloc(sizeof(dup_cmdline));
                        //strcpy(JOB_INFO[i].name, dup_cmdline);
			return i;
		}
	}
	return -1; 
}

int look_for_job(Job* JOB_INFO, pid_t pid){
	int i,j;
	for (i = 0; i < NUM_MAX; i++){
		for (j=0; j < JOB_INFO[i].npids; j++){
			if (JOB_INFO[i].pids[j] == pid){
				return i;
			}
		}
	}
	return -1;
}


void put_in_FG(pid_t pgid){
	void (*sav)(int);
	sav = signal(SIGTTOU, SIG_IGN);
	tcsetpgrp(STDOUT_FILENO,pgid);
	signal(SIGTTOU, sav);
}

void print_job (char* str)
{
	pid_t FG_pgid;
	FG_pgid = tcgetpgrp (STDOUT_FILENO);
	put_in_FG(getpgrp());
	printf("%s", str);
	put_in_FG(FG_pgid);
}

void handler (int sig)
{
   pid_t chld_pid;
   int status,number;
   char str_buff[1024];
   while (( chld_pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0){
      number = look_for_job(JOB_INFO,chld_pid);
      if (WIFEXITED (status)){
         JOB_INFO[number].total_npids -= 1;
         if (JOB_INFO[number].total_npids == 0){
            if (JOB_INFO[number].status == BG){
               kill(JOB_INFO[number].pgid, SIGTTOU);
               printf("\n[%d] + done	%s\n",number,JOB_INFO[number].name); 
            }
            remove_job(JOB_INFO, number);
            put_in_FG(PSSH_PGID);
         }
      } if (WIFSIGNALED (status)) {
         put_in_FG(PSSH_PGID);
         remove_job(JOB_INFO, number);
      } if (WIFSTOPPED (status)) {
         put_in_FG(PSSH_PGID);
         JOB_INFO[number].status = STOPPED;
         sprintf(str_buff,"[%d] + suspended	%s\n", number, JOB_INFO[number].name);
         print_job(str_buff);
      } if (WIFCONTINUED (status)) {
         JOB_INFO[number].status = FG;
      } 
        }
}

void job_exec(Job* JOB_INFO)
{	
	int i;
	char str_buff[1024];
	for (i = 0; i < NUM_MAX; i++){
		if (JOB_INFO[i].status == STOPPED){
			sprintf(str_buff,"[%d] + stopped	%s\n",i, JOB_INFO[i].name);
			print_job(str_buff);
		} if ((JOB_INFO[i].status ==  FG) || (JOB_INFO[i].status == BG)){
			sprintf(str_buff,"[%d] + running	%s\n",i, JOB_INFO[i].name);
			print_job(str_buff);
		}
	}
}

void fg_exec(Job* JOB_INFO, Parse* P)
{
	int number;

	if (P->tasks[0].argv[1] == NULL){
		printf("%s\n","Usage: fg %<job number>");
	} else {
		number = atoi(P->tasks[0].argv[1]+1);
		if ((number == 0) && JOB_INFO[number].status == TERM){
			printf("pssh: invalid job number: %s\n", P->tasks[0].argv[1]);
		} if (JOB_INFO[number].status == TERM) {
			printf("pssh: invalid job number: %s\n", P->tasks[0].argv[1]);
		} else {	
			if (JOB_INFO[number].status == STOPPED){
				JOB_INFO[number].status = FG;
				printf("[%d] + continued	%s\n\n", number ,JOB_INFO[number].name);
				put_in_FG(JOB_INFO[number].pgid);
				kill(-JOB_INFO[number].pgid,SIGCONT);		
			} else {
				JOB_INFO[number].status = FG;
				printf("%s\n\n", JOB_INFO[number].name);
				put_in_FG(JOB_INFO[number].pgid);
			}	
		}
	}
}

void bg_exec(Job* JOB_INFO, Parse* P)
{
	int number;

	if (P->tasks[0].argv[1] == NULL){
		printf("%s\n","Usage: bg %<job number>");
	} else {
		number = atoi(P->tasks[0].argv[1]+1);
		if ((number == 0) && JOB_INFO[number].status == TERM){
			printf("pssh: invalid job number: %s\n", P->tasks[0].argv[1]);
		} if (JOB_INFO[number].status == TERM) {
			printf("pssh: invalid job number: %s\n", P->tasks[0].argv[1]);
		} else {	
			if (JOB_INFO[number].status == STOPPED){
				JOB_INFO[number].status = FG;
				printf("\n");
				printf("[%d] + continued		%s\n\n", number ,JOB_INFO[number].name);
				kill(-JOB_INFO[number].pgid,SIGCONT);		
			} else {
				JOB_INFO[number].status = FG;
				printf("\n");
				printf("%s\n\n", JOB_INFO[number].name);
			}	
		}
	}
}

void kill_exec(Job* JOB_INFO, Parse* P)
{
   int i;
   int number,sig,pid;
   if (P->tasks[0].argv[1] == NULL){
      printf("%s\n","Usage: kill [-s <signal>] <pid> | %<job> ...");
   }
   else {
      if (strcmp(P->tasks[0].argv[1],"-s") == 0) {
         sig = atoi(P->tasks[0].argv[2]);
         if (P->tasks[0].argv[3][0] == '%') {
            i = 3;
            while(P->tasks[0].argv[i] != NULL){
               number = atoi(P->tasks[0].argv[i]+1);
               if (JOB_INFO[number].status == TERM){
                  printf("pssh: invalid job number %d\n",number);
               } else {
                  kill(-JOB_INFO[number].pgid,sig);
                  printf("[%d] + done %s\n",number, JOB_INFO[number].name);
               }
               i++;
            }
         } else {
            i = 3;
            while(P->tasks[0].argv[i] != NULL){
               pid = atoi(P->tasks[0].argv[i]);
               if (kill(pid,0) == 0){
                  kill(pid,sig);
               } 
               else 
               {
                  printf("pssh: invalid pid number %d\n",pid);
               }
               i++;
            }
         }
      } else {
         if (P->tasks[0].argv[1][0] == '%'){
            i = 1;
            while(P->tasks[0].argv[i] != NULL){
               number = atoi(P->tasks[0].argv[i]+1);
               if (JOB_INFO[number].status == TERM){
                  printf("pssh: invalid job number %d\n",number);
               } else {
                  kill(-JOB_INFO[number].pgid, SIGINT);
                  printf("[%d] + done	%s\n",number, JOB_INFO[number].name);
               }
               i++;
            }
         } else {
            i = 1;
            while(P->tasks[0].argv[i] != NULL){
               pid = atoi(P->tasks[0].argv[i]);
               if (kill(pid,0) == 0){
                  kill(pid,SIGINT);
               } else {
                  printf("pssh: invalid pid number %d\n",pid);
               }
               i++;
            }
         }	
      }
   }
}

