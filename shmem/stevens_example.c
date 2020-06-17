#include <stdlib.h>
#include <stdio.h>
#include <sys/shm.h>

/*
Imported/Adapted from the "advanced programming in the unix environment" book.
*/

#define ARRAY_SIZE 40000
#define MALLOC_SIZE 100000
#define SHM_SIZE 100000
#define SHM_MODE 0600/* user read/write */

/* uninitialized data = bss */
char array[ARRAY_SIZE];

void err_sys(char *buf) {
    perror(buf);
    exit(0);
}

int main(void) {
    char *ptr, *shmptr;
    int shmid;
    printf("array[] from %p to %p\n", (void *)&array[0],(void *)&array[ARRAY_SIZE]);
    printf("stack around %p\n", (void *)&shmid);
    if ((ptr = malloc(MALLOC_SIZE)) == NULL){
        err_sys("malloc error");
    }
    printf("malloced from %p to %p\n", (void *)ptr, (void *)ptr+MALLOC_SIZE);
    if ((shmid = shmget(IPC_PRIVATE, SHM_SIZE, SHM_MODE)) < 0)
        err_sys("shmget error");

    printf("shmid %d\n", shmid);
    
    if ((shmptr = shmat(shmid, 0, 0)) == (void *)-1)
        err_sys("shmat error");
        
    printf("shared memory attached from %p to %p\n", (void *)shmptr,(void *)shmptr+SHM_SIZE);
    
    if (shmctl(shmid, IPC_RMID, 0) < 0)
        err_sys("shmctl error");
    exit(0);
}