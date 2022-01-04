#ifndef _job_h_
#define _job_h_

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

typedef enum {
	TERM,
	STOPPED,
	BG,
	FG,
} JobStatus;

typedef struct {
	char* name;
	pid_t* pids;
	unsigned int npids;
	unsigned int total_npids;
	pid_t pgid;
	JobStatus status;
} Job;

#define NUM_MAX 100
pid_t PSSH_PGID;
Job JOB_INFO[NUM_MAX];

void print_job(char* str);
void put_in_FG(pid_t pgid);
int parse_job(Job* JOB_INFO, Parse* P, char* dup_cmdline);
int look_for_job(Job* JOB_INFO, pid_t pid);
void remove_job(Job* JOB_INFO, int number);
void handler (int sig);
void job_exec(Job* JOB_INFO);
void fg_exec(Job* JOB_INFO, Parse* P);
void bg_exec(Job* JOB_INFO, Parse* P);
void kill_exec(Job* JOB_INFO, Parse* P);
#endif
