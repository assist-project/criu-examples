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

#define SERVER_PORT 12346

void do_stuff_server(int); /* function prototype */


static int s_pid = 0; /* PID of server process */

void error(char *msg)
{
    if (s_pid) {
        kill(s_pid, SIGKILL);
        waitpid(s_pid, NULL, 0);
    }
    perror(msg);
    exit(1);
}

int run_server()
{
    int sockfd, newsockfd, portno, clilen, pid, option;
    struct sockaddr_in serv_addr, cli_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(SERVER_PORT);
    if(setsockopt(sockfd,SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0)
    {
        printf("setsockopt failed\n");
        close(sockfd);
        exit(2);
    }

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
  //   while (1) {
        newsockfd = accept(sockfd, 
            (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) 
            error("ERROR on accept");
        do_stuff_server(newsockfd);
        close(newsockfd);
//     } /* end of while */
     return 0; 
}

void do_stuff_server (int sock)
{
    int n;
    char buffer[256];
    int expected_num=0, received_num;

    while (1) {
        bzero(buffer,256);
        n = read(sock,buffer,255);
        if (n < 0) 
            error("ERROR reading from socket");
        if (n == 0) {
            printf("Connection closed by client\n");
            break ;
        } 
        printf("(server) Recv from client: %s\n", buffer);
        sscanf(buffer, "%d", &received_num);
        bzero(buffer, 256);
        if (received_num == expected_num) {
            sprintf(buffer, "ACK %d", expected_num);
            expected_num ++;
            n = write(sock, buffer, strlen(buffer));
            if (n < 0) 
                error("ERROR writing to socket");
        } else {
            sprintf(buffer, "Invalid ACK %d, expected %d", received_num, expected_num );
            n = write(sock, buffer, strlen(buffer));
            if (n < 0) 
                error("ERROR writing to socket");
            break;
        }
    }
    close(sock);
}

void connect_to_server(int *sockfd) {
    struct sockaddr_in servaddr, cli; 
  
    // socket create and varification 
    *sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (*sockfd == -1) 
        error("socket creation failed...\n");  
    else
        printf("Socket successfully created..\n"); 

    bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(SERVER_PORT); 
  
    // connect the client socket to server socket 
    if (connect(*sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
        error("connection with the server failed...\n"); 
    } 
    else
        printf("connected to the server..\n"); 
}

void send_num(int sockfd, int i) {
    char buffer[256];
    bzero(buffer,256);
    sprintf(buffer, "%d", i);
    int n = write(sockfd,buffer,strlen(buffer));
    if (n < 0) 
        error("ERROR writing to socket\n");
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n == 0) {
        printf("Connection closed by server\n");
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
    int dump_fd, sockfd, status;
    int sec_sleep = 5;
    char dump_dir [] = "dump";
    int leave_running = 0;

    s_pid = fork();

    if (s_pid < 0) {
        error("ERROR forking");
    } else {
        if (!s_pid) {
            setsid(); 
            close(STDIN_FILENO);
		    //close(STDOUT_FILENO); 
		    close(STDERR_FILENO);
            run_server();
            return 0;
        } 
        else {

            // printf("Server PID %d\n", s_pid);
            // printf("Own PID %d\n", getpid());
            // printf("Parent PID %d\n", getppid()); 

            // create the target folder
            if(mkdir(dump_dir, 0777) && errno != EEXIST) {
                error("ERROR Failed to create dump directory");
            }

            if ( (dump_fd=open(dump_dir, __O_DIRECTORY | __O_PATH)) < 0) {
                error("ERROR Failed to retrieve file descriptor");
            }

            connect_to_server(&sockfd);
            send_num(sockfd, 0);
            send_num(sockfd, 1);
            send_num(sockfd, 2);
            send_num(sockfd, 3);

            printf("Sleeping for %d seconds\n", sec_sleep);
            sleep(sec_sleep);
            
            printf("Dumping server\n");
            check(criu_init_opts(), "init_opts");
            criu_set_pid(s_pid);
            criu_set_images_dir_fd(dump_fd);
            criu_set_leave_running(leave_running);
            criu_set_log_level(4);
            criu_set_log_file("dump.log");
            criu_set_tcp_established(1); /* this is necessary, since at the moment of dumping TCP connection was in established state*/
            criu_set_shell_job(1);
            check(criu_dump(), "dump");
            printf("Dumped successfully\n");

            /* Unfortunately, leaving the server running prevents us from restoring the connection */
            if (leave_running) {
                printf("Left server running\n");
                send_num(sockfd, 4);
                send_num(sockfd, 5);
                send_num(sockfd, 6);
                send_num(sockfd, 7);
                printf("Killing server\n");
                close(sockfd);
            }
            
            waitpid(s_pid, NULL, 0); /* we have to wait for the server process to stop, otherwise restoring will fail */
            printf("Sleeping for 1 second\n");
            sleep(1);

            printf("Now restoring\n");
            check(criu_init_opts(), "init_opts");
            criu_set_log_level(4);
            criu_set_images_dir_fd(dump_fd);
            criu_set_log_file("restore.log");
            criu_set_tcp_established(1);
            criu_set_shell_job(1);
            criu_set_pid(s_pid);
            int pid = criu_restore_child();
            if (pid <= 0) {
                printf("Restoration failed\n");
                close(sockfd);
                return -1;
            }
            printf("   `- Restore returned pid %d\n", pid);

            send_num(sockfd, 4);
            send_num(sockfd, 5);
            send_num(sockfd, 6);
            close(sockfd);
            waitpid(s_pid, NULL, 0);

        }
    }
    return 0; 
}