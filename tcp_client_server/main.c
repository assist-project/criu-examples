/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
   gcc server2.c -lsocket
*/
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
//#include <linux/socket.h>
//#include <netinet/in.h>
//#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
//#include <strings.h>
#include <arpa/inet.h>
//#include <netdb.h> 
#include "criu/criu.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/un.h>
#include "connection.h"
#include "client.h"


#define SUCC_ECODE	42

static int s_pid = 0; /* PID of server process */
static int c_pid = 0; /* PID of client process */
static int c_send = 0; /* tell client to send */

void error(char *msg)
{
    if (s_pid) {
        kill(s_pid, SIGKILL);
        waitpid(s_pid, NULL, 0);
    }
    if (c_pid) {
        kill(c_pid, SIGUSR2);
        waitpid(c_pid, NULL, 0);
    }
    perror(msg);
    exit(EXIT_FAILURE);
}

void connect_to_client(int *data_socket, char *socket_name) {
    struct sockaddr_un addr;
    int ret;
    char buffer[BUFFER_SIZE];
    /* Create local socket. */

    *data_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*data_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /*
     * For portability clear the whole structure, since some
     * implementations have additional (nonstandard) fields in
     * the structure.
     */
    memset(&addr, 0, sizeof(struct sockaddr_un));
    /* Connect socket to socket address */
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_name, sizeof(addr.sun_path) - 1);
    sleep(5);
    ret = connect((*data_socket), (const struct sockaddr *) &addr, sizeof(struct sockaddr_un));
    if (ret == -1) {
        perror("ERROR connecting.\n");
        exit(EXIT_FAILURE);
    }
}


void send_num(int sockfd, int i) {
    char buffer[BUFFER_SIZE];
    bzero(buffer,BUFFER_SIZE);
    sprintf(buffer, "%d", i);
    printf("Sending %s\n", buffer);
    int n = write(sockfd,buffer,strlen(buffer));
    if (n < 0) 
       error("ERROR writing to socket\n");
    bzero(buffer,BUFFER_SIZE);
    n = read(sockfd,buffer,BUFFER_SIZE);
    if (n == 0) {
       printf("Connection closed by server\n");
       return;
    }
    if (n < 0) 
       error("ERROR reading from socket\n");
    printf("Recv from server: %s\n", buffer);
}

void check(int ret_val, char *operation) {
    if (ret_val) {
        printf("Operation %s failed\n", operation);
        if (s_pid) {
            kill(s_pid, SIGKILL);
        }
        exit(-1);
    }
}

int main() {
    int dump_fd, cmd_sockfd, status;
    int sec_sleep = 5;
    char dump_dir [] = "dump";
    int leave_running = 0;
    int  ret;

    c_pid = fork();
    printf("c_pid=%d\n", c_pid);

    if (c_pid < 0) {
        error("ERROR forking");
    } else {
        if (!c_pid) {
            printf("Running client\n");
            if (setsid() < 0) {
			    printf("Fail signal\n");
                exit(1);
            }
            run_client(SERVER_PORT, SOCKET_NAME);
            //close(STDIN_FILENO);
		    //close(STDOUT_FILENO); 
		    //close(STDERR_FILENO);
		    
            return 0;
        } 
        else {

            // // create the target folder
            // if(mkdir(dump_dir, 0777) && errno != EEXIST) {
            //     error("ERROR Failed to create dump directory");
            // }

            // if ( (dump_fd=open(dump_dir, __O_DIRECTORY | __O_PATH)) < 0) {
            //     error("ERROR Failed to retrieve file descriptor");
            // }

            connect_to_client(&cmd_sockfd, SOCKET_NAME);

            send_num(cmd_sockfd, 0);
            send_num(cmd_sockfd, 1);
            send_num(cmd_sockfd, 2);
            send_num(cmd_sockfd, 3);
            send_num(cmd_sockfd, 4);

            close(cmd_sockfd);

            // printf("Sleeping for %d seconds\n", sec_sleep);
            // sleep(sec_sleep);

            // //kill(c_pid, SIGUSR2);
            // waitpid(c_pid, NULL, 0);
            
            // printf("Dumping server\n");
            // check(criu_init_opts(), "init_opts");
            // criu_set_pid(s_pid);
            // criu_set_images_dir_fd(dump_fd);
            // criu_set_leave_running(leave_running);
            // criu_set_log_level(4);
            // criu_set_log_file("dump.log");
            // criu_set_tcp_established(1); /* this is necessary, since at the moment of dumping TCP connection was in established state*/
            // criu_set_shell_job(1);
            // check(criu_dump(), "dump");
            // printf("Dumped successfully\n");

            // /* Unfortunately, leaving the server running prevents us from restoring the connection */
            // if (leave_running) {
            //     printf("Left server running\n");
            //     send_num(sockfd, 4);
            //     send_num(sockfd, 5);
            //     send_num(sockfd, 6);
            //     send_num(sockfd, 7);
            //     printf("Killing server\n");
            //     //close(sockfd);
            // }
            
            // waitpid(s_pid, NULL, 0); /* we have to wait for the server process to stop, otherwise restoring will fail */
            // printf("Sleeping for 1 second\n");
            // sleep(1);

            // printf("Now restoring\n");
            // check(criu_init_opts(), "init_opts");
            // criu_set_log_level(4);
            // criu_set_images_dir_fd(dump_fd);
            // criu_set_log_file("restore.log");
            // criu_set_tcp_established(1);
            // criu_set_shell_job(1);
            // criu_set_pid(s_pid);
            // int pid = criu_restore_child();
            // if (pid <= 0) {
            //     printf("Restoration failed\n");
            //     close(sockfd);
            //     return -1;
            // }
            // printf("   `- Restore returned pid %d\n", pid);

            // send_num(sockfd, 4);
            // send_num(sockfd, 5);
            // send_num(sockfd, 6);
            // close(sockfd);
            // waitpid(s_pid, NULL, 0);

        }
    }
    return 0; 
}