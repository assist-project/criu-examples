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
#include "adapter.h"
#include "server.h"

#define SUCC_ECODE	42
#define MAX_ATTEMPTS 5 /* Maximum attempts to connect to adapter*/

static int s_pid = 0; /* PID of server process */
static int a_pid = 0; /* PID of adapter process */
static int cmd_sockfd = 0; /* command socket*/

void error(char *msg)
{
    if (s_pid) {
        kill(s_pid, SIGKILL);
        waitpid(s_pid, NULL, 0);
    }
    if (a_pid) {
        kill(a_pid, SIGUSR2);
        waitpid(a_pid, NULL, 0);
    }
    perror(msg);
    exit(EXIT_FAILURE);
}

void connect_to_adapter(int *data_socket, char *socket_name) {
    struct sockaddr_un addr;
    int ret=-1, attempts = 0;
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
    while (ret == -1 && attempts < MAX_ATTEMPTS) {
        ret = connect((*data_socket), (const struct sockaddr *) &addr, sizeof(struct sockaddr_un));
        attempts++;
        usleep(10000);
    } 
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

void send_cmd(int sockfd, char *cmd) {
    int ret;
    ret = write(sockfd, cmd, strlen(cmd));
    if (ret < 0) {
        error("Error sending command");
    }
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

int main(int argc, char **argv) {
    int dump_fd, status;
    int sec_sleep = 5;
    char dump_dir [] = "dump", dummy_dump_dir [] = "dummy-dump";
    int leave_running = 0;
    int ret, with_bkp=0, dummy_dump=1;

    // it is necessary to eliminate TW states, otherwise restoring after kill will not work
    system("sysctl net.ipv4.tcp_max_tw_buckets=0");

    // enable breakpoints
    if (argc == 2 && strcmp(argv[1], "-b")) {
        with_bkp = 1;
    }
    with_bkp = 1;

    a_pid = fork();
    printf("a_pid=%d\n", a_pid);

    if (a_pid < 0) {
        error("ERROR forking");
    } 
    if (!a_pid) {
        printf("Running adapter in separate session\n");
        if (setsid() < 0) {
		    printf("Fail signal\n");
            exit(EXIT_FAILURE);
        }

        s_pid = fork();
        if (s_pid>0) {
            run_server(SERVER_PORT);
            exit(EXIT_SUCCESS);
        }
        run_adapter(s_pid, SERVER_PORT, SOCKET_NAME);
        //close(STDIN_FILENO);
		//close(STDOUT_FILENO); 
		//close(STDERR_FILENO);
		
        exit(EXIT_SUCCESS);
    } 

    // create the target folder
    if(mkdir(dump_dir, 0777) && errno != EEXIST) 
        error("ERROR Failed to create dump directory");
    if ( (dump_fd=open(dump_dir, __O_DIRECTORY | __O_PATH)) < 0) 
        error("ERROR Failed to retrieve file descriptor");
    
    connect_to_adapter(&cmd_sockfd, SOCKET_NAME);
    send_num(cmd_sockfd, 0);
    send_num(cmd_sockfd, 1);
    close(cmd_sockfd);
    
    if (with_bkp) {
        printf("(bkp) Before first dump\n");
        getchar();
    }
    
    printf("Dumping server\n");
    check(criu_init_opts(), "init_opts");
    criu_set_pid(a_pid);
    criu_set_images_dir_fd(dump_fd);
    criu_set_leave_running(leave_running);
    criu_set_log_level(4);
    criu_set_log_file("dump.log");
    criu_set_tcp_established(1); /* Necessary since at the moment of dumping, the TCP connection between adapter and server is in the ESTABLISHED state. */
    criu_set_ext_unix_sk(1); /* Necessary since we have a UNIX socket connection with the dumpee. */
    criu_set_shell_job(1); /* Necessary unless we close STDIN/STDOUT/STDERR */
    check(criu_dump(), "dump");
    printf("Dumped successfully\n");
    waitpid(a_pid, NULL, 0); /* we have to wait for the server process to stop, otherwise restoring will fail */
    
    if (with_bkp) {
        printf("(bkp) After dump, before first restore\n");
        getchar(); // after dump, before fr
    }
    
    printf("Restoring for the first time\n");
    check(criu_init_opts(), "init_opts");
    criu_set_log_level(4);
    criu_set_images_dir_fd(dump_fd);
    criu_set_log_file("restore1.log");
    criu_set_tcp_established(1);
    criu_set_ext_unix_sk(1);
    criu_set_shell_job(1);
    criu_set_pid(s_pid);
    int pid = criu_restore_child();
    if (pid <= 0) {
        printf("Restoration failed\n");
        goto error;
    }
    printf("   `- Restore returned pid %d\n", pid);
    printf("Sending follow-up numbers\n");
    connect_to_adapter(&cmd_sockfd, SOCKET_NAME);
    send_num(cmd_sockfd, 2);
    send_num(cmd_sockfd, 3);

    if (with_bkp) {
        printf("(bkp) Before stopping adapter/server\n");
        getchar();
    }
    
    printf("Telling adapter to stop, and waiting for it to exit\n");
    send_cmd(cmd_sockfd, STOP_CMD);
    close(cmd_sockfd);
    waitpid(a_pid, &status, 0);

    if (with_bkp) {
        printf("(bkp) After killing, before second restore\n");
        getchar();
    }
    
    printf("Restoring for a second time\n");
    criu_set_log_file("restore2.log");
    pid = criu_restore_child();
    if (pid <= 0) {
        printf("Restoration failed\n");
        goto error;
    }
    
    printf("   `- Restore returned pid %d\n", pid);
    printf("Sending follow-up numbers again\n");
    connect_to_adapter(&cmd_sockfd, SOCKET_NAME);
    send_num(cmd_sockfd, 2);
    send_num(cmd_sockfd, 3);
    send_cmd(cmd_sockfd, STOP_CMD);
    printf("We are done here\n");
    close(cmd_sockfd);
    waitpid(a_pid, &status, 0);
    unlink(SOCKET_NAME);
    return EXIT_SUCCESS;

    error:
    close(cmd_sockfd);
    kill(SIGKILL, a_pid);
    waitpid(a_pid, &status, 0);
    unlink(SOCKET_NAME);
    return EXIT_FAILURE;
}