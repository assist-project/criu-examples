#include "criu/criu.h"
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "lib.h"

/*
Adapted from:
https://github.com/checkpoint-restore/criu/blob/88aaae3ace1eace0399244a753895334c727c708/test/others/libcriu/test_sub.c
*/

static int stop = 0;
static void sh(int sig)
{
	stop = 1;
}

#define SUCC_ECODE	42


void error(char *msg)
{
    perror(msg);
    exit(1);
}

void run_counter() {
	int i = 0;
	while (!stop) {
		printf("Counted seconds: %d\n", i++);
		sleep(1);
	}
}


int main(int argc, char **argv)
{
	int pid, ret, dump_fd, p[2];
	char dump_dir[] = "dump";

	printf("--- Start loop ---\n");
	pipe(p);
	pid = fork();
	if (pid < 0) {
		perror("Can't");
		return -1;
	}

	if (!pid) {
		printf("   `- loop: initializing\n");
		if (setsid() < 0)
			exit(1);
		if (signal(SIGUSR1, sh) == SIG_ERR)
			exit(1);
		
		// closing known FILENO, linked to criu_set_shell_job command
		close(STDIN_FILENO);
		// close(STDOUT_FILENO); // we keep STDOUT open, otherwise the child's actions would be completely transparent
		close(STDERR_FILENO);
		close(p[0]);

		ret = SUCC_ECODE;
		write(p[1], &ret, sizeof(ret));
		close(p[1]);
		run_counter();
		exit(SUCC_ECODE);
	}

	close(p[1]);

	/* Wait for kid to start */
	ret = -1;
	read(p[0], &ret, sizeof(ret));
	if (ret != SUCC_ECODE) {
		printf("Error starting loop\n");
		goto err;
	} 

	/* Wait for pipe to get closed, then dump */
	read(p[0], &ret, 1);
	close(p[0]);

	printf("--- Dump loop ---\n");
	criu_init_opts();	
	//criu_set_service_binary(argv[1]); // 
	criu_set_pid(pid);
	criu_set_shell_job(1); // this has to be set if child does not close STDOUT, STDERR or STDIN
	criu_set_log_file("dump.log");
	criu_set_log_level(4);
	criu_set_leave_running(1);

	/* create the target folder if it doesn't exit and open it to get file descriptor */
    if(mkdir(dump_dir, 0777) && errno != EEXIST) {
		kill(pid, SIGKILL);
        error("ERROR Failed to create dump directory\n");
    }
    if ( (dump_fd=open(dump_dir, __O_DIRECTORY | __O_PATH)) < 0) {
		kill(pid, SIGKILL);
        error("ERROR Failed to open directory\n");
    }
	
	// set images dir, a folder for dumping to/restoring from
	criu_set_images_dir_fd(dump_fd);

	sleep(5);
	printf("Dumping...\n");

	ret = criu_dump();
	if (ret < 0) {
		what_err_ret_mean(ret);
		kill(pid, SIGKILL);
		goto err;
	}

	printf("   `- Dump succeeded, waiting 5 seconds\n");
	sleep(5);
	printf("Stopping child (first time)\n");
	kill(pid, SIGUSR1);
	waitpid(pid, NULL, 0);

	printf("--- Restore loop ---\n");
	printf("Restoring (first time)\n");
	criu_init_opts();
	criu_set_log_level(4);
	criu_set_shell_job(1);
	criu_set_log_file("restore.log");
	criu_set_images_dir_fd(dump_fd);

	pid = criu_restore_child();
	if (pid <= 0) {
		what_err_ret_mean(pid);
		return -1;
	}

	//printf("   `- Restore returned pid %d\n", pid);
	sleep(5);
	printf("Stopping child (second time)\n");
	kill(pid, SIGUSR1);
	waitpid(pid, NULL, 0);
	sleep(1);
	printf("Restoring (second time)...\n");
	pid = criu_restore_child();
	if (pid <= 0) {
		what_err_ret_mean(pid);
		return -1;
	}

	sleep(5);

	printf("Stopping child (third and final time)\n");
	kill(pid, SIGUSR1);
err:
	if (waitpid(pid, &ret, 0) < 0) {
		perror("   Can't wait kid");
		return -1;
	}

	return chk_exit(ret, SUCC_ECODE);
}
