#include <stdlib.h>
#include <stdio.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define ARRAY_SIZE 10
#define MALLOC_SIZE 100
#define SHM_SIZE 100
#define SHM_MODE 0600/* user read/write */

/* uninitialized data = bss */
char array[ARRAY_SIZE];

int child;
int parent;

void err_sys(char *buf) {
    perror(buf);
    exit(0);
}

void print_array(char *arrptr, int size) {
    char c;
    int i;
    printf("\n Status: \n {");
    for (i=0; i<size-1; i++) {
        printf("%d, ", arrptr[i]);
    }
    if (i>0) {
        printf("%d}\n", arrptr[i]);
    }
}

void fill_array(int shmid, int start_offset, int len, char el) {
    char *shmptr;
    int i;
    if ((shmptr = shmat(shmid, 0, 0)) == (void *)-1)
        err_sys("shmat error");

    for (i=start_offset; i<start_offset+len; i++) {
        shmptr[i] = el;
    }

    shmdt(shmptr);
}


int main(void) {
    char *shmptr;
    int shmid, c_pid = 0;
    // we use signals, the only sync mechanism that seems not to bother CRIU
    int stat, sig;
    // Now let's block SIGUSR1
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
    const sigset_t set;

    parent = getpid();
    
    if ((shmid = shmget(IPC_PRIVATE, SHM_SIZE, SHM_MODE)) < 0)
        err_sys("shmget error");

    printf("shmid %d\n", shmid);
    
    if ((shmptr = shmat(shmid, 0, 0)) == (void *)-1)
        err_sys("shmat error");

    printf("shared memory attached from %p to %p\n", (void *)shmptr,(void *)shmptr+SHM_SIZE);
    strcpy(shmptr, "");
    fill_array(shmid, 0, ARRAY_SIZE, 0);

    child = fork();
    if (child == 0) {
        fill_array(shmid, 0, 5, 1);
        kill(parent, SIGUSR1);
        sigwait(&sigset, &sig);
        fill_array(shmid, 5, ARRAY_SIZE-5, 1);
        exit(EXIT_SUCCESS);
    } else {
        sigwait(&sigset, &sig);
        print_array(shmptr, ARRAY_SIZE);
        kill(child, SIGUSR1);
        waitpid(child, NULL, 0);
        print_array(shmptr, ARRAY_SIZE);
        if (shmctl(shmid, IPC_RMID, 0) < 0) {
            err_sys("shmctl error");
        }
        fflush(NULL);
        exit(EXIT_SUCCESS);
    }
}