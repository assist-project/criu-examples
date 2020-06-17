#include <stdlib.h>
#include <stdio.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <criu/criu.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>


#define ARRAY_SIZE 10
#define MALLOC_SIZE 100
#define SHM_SIZE 100
#define SHM_MODE 0600/* user read/write */
#define SHMID_ENV_VAR "_SHMID"

/* uninitialized data = bss */
char array[ARRAY_SIZE];

int child;
int parent;
int shmid;

char * shmid_env_var = SHMID_ENV_VAR; 

void err_sys(char *buf) {
    perror(buf);
    exit(0);
}

void print_array() {
    char *shmptr;
    int i;
    char *shmid_str = getenv(SHMID_ENV_VAR);
    shmid = atoi(shmid_str);
    printf("%d\n", shmid);
    if ((shmptr = shmat(shmid, 0, 0)) == (void *)-1)
        err_sys("(print_array) shmat error");

    printf("\n Status: \n {");
    for (i=0; i<ARRAY_SIZE; i++) {
        printf("%d, ", shmptr[i]);
    }
    if (i>0) {
        printf("%d}\n", shmptr[i]);
    }

    shmdt(shmptr);
}

void fill_array(int start_offset, int len, char el) {
    char *shmptr;
    int i;
    char *shmid_str = getenv(SHMID_ENV_VAR);
    shmid = atoi(shmid_str);
    printf("%d\n", shmid);
    if ((shmptr = shmat(shmid, 0, 0)) == (void *)-1)
        err_sys("(fill_array) shmat error");

    for (i=start_offset; i<start_offset+len; i++) {
        shmptr[i] = el;
    }

    shmdt(shmptr);
}

void check(int ret_val, char *operation) {
    if (ret_val) {
        printf("Operation %s failed %d\n", operation, ret_val);
        kill(child, SIGKILL);
        exit(EXIT_FAILURE);
    }
}

void config_restore(int img_fd) {
    check(criu_init_opts(), "init_opts");
    criu_set_log_level(4);
    criu_set_images_dir_fd(img_fd);
    criu_set_log_file("restore.log");
    criu_set_shell_job(1);
}

void config_dump(int img_fd, int pid) {
    check(criu_init_opts(), "init_opts");
    criu_set_pid(pid);
    criu_set_images_dir_fd(img_fd);
    criu_set_leave_running(0);
    criu_set_log_level(4);
    criu_set_log_file("dump.log");
    criu_set_shell_job(1);
}

void release_shm() {
    if (shmid && child) {
        printf("Releasing shm\n");
        shmctl(shmid, IPC_RMID, 0);
    }
}

int main(void) {
    char *shmptr;
    int c_pid = 0, dump_fd;
    // we use signals, the only sync mechanism that seems not to bother CRIU
    int status, sig;
    char * dump_dir = "dump";
    char shmid_str[10];

    // Now let's block SIGUSR1
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
    const sigset_t set;

    int img_new = 0;

    if(mkdir(dump_dir, 0777) && errno != EEXIST) {
        perror("ERROR Failed to create dump directory");
        exit(EXIT_FAILURE);
    }
    
    if (errno != EEXIST) {
        img_new = 1;
    }

    parent = getpid();
    
    if ((shmid = shmget(IPC_PRIVATE, SHM_SIZE, SHM_MODE)) < 0)
        err_sys("shmget error");

    atexit(release_shm);
    printf("shmid %d\n", shmid);
    snprintf(shmid_str, sizeof(shmid_str), "%d", shmid);
    setenv(SHMID_ENV_VAR, shmid_str, 1);
   
    fill_array(0, ARRAY_SIZE, 0);

    if (img_new) {
        child = fork();
        if (child == 0) {
            printf("Filling first half\n");
            fill_array(0, 5, 1);
            kill(parent, SIGUSR1);
            sigwait(&sigset, &sig);
            printf("Filling second half\n");
            fill_array(5, ARRAY_SIZE-5, 1);
            exit(EXIT_SUCCESS);
        } else {
            sigwait(&sigset, &sig);
            print_array();
            printf("Taking snapshot before child executes job 2\n");
            if ( (dump_fd=open(dump_dir, __O_DIRECTORY | __O_PATH)) < 0) {
                perror("ERROR Failed to retrieve file descriptor");
                exit(EXIT_FAILURE);
            }
            config_dump(dump_fd, child);
            check(criu_dump(), "dump");
            waitpid(child, &status, 0);
            printf("Snapshot taken\n");
        }
    } else {
        if ( (dump_fd=open(dump_dir, __O_DIRECTORY | __O_PATH)) < 0) {
            perror("ERROR Failed to retrieve file descriptor");
            exit(EXIT_FAILURE);
        }
    }

    config_restore(dump_fd);
    child = criu_restore_child();
    if (child < 0) {
        printf("Failed to restore child %d\n", child);
        exit(EXIT_FAILURE);
    }

    kill(child, SIGUSR1);
    waitpid(child, &status, 0);
    print_array();
    exit(EXIT_SUCCESS);
}