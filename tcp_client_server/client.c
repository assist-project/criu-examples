#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
//#include <sys/wait.h>
//#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
//#include <dirent.h>
#include <arpa/inet.h>
#include "server.h"
//#include "criu/criu.h"
//#include <fcntl.h>
#include <signal.h>
#include <sys/un.h>
#include "connection.h"
#include "client.h"

static int stop = 0;

static void sh(int sig)
{
    if (sig == SIGUSR1) {
        stop = 1;
    }
}

void connect_to_server(int *sockfd, int server_port) {
    struct sockaddr_in servaddr, cli; 
  
    // socket create and varification 
    *sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (*sockfd == -1) {
        perror("socket creation failed...\n");  
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket successfully created..\n"); 

    bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(server_port); 
  
    // connect the client socket to server socket 
    if (connect(*sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
        perror("connection with the server failed...\n"); 
        exit(EXIT_FAILURE);
    } 
    else
        printf("connected to the server..\n"); 
}

void setup_command_socket(int *sockfd, char * socket_name) {
     int newsockfd, portno, clilen, pid, option;
    struct sockaddr_un cmd_addr;
    *sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*sockfd < 0)  {
        perror("ERROR creating socket");
        exit(EXIT_FAILURE);
    }

    bzero((char *) &cmd_addr, sizeof(cmd_addr));

    cmd_addr.sun_family = AF_UNIX;
    strncpy(cmd_addr.sun_path, socket_name, sizeof(cmd_addr.sun_path) - 1);

    if (bind((*sockfd), (const struct sockaddr *) &cmd_addr, sizeof(struct sockaddr_un)) < 0) {
        perror("ERROR on binding");
        exit(EXIT_FAILURE);
    }
}

int run_client(int server_port, char *command_socket_name) {
    int server_data_sockfd, cmd_sockfd, cmd_data_sockfd, num, ret;
    char buffer[BUFFER_SIZE];
    if (signal(SIGUSR1, sh) == SIG_ERR) {
        perror("ERROR signal\n");
		exit(EXIT_FAILURE);
    }

    setup_command_socket(&cmd_sockfd, command_socket_name);
    //connect_to_server(&dsockfd, server_port);
    if (listen(cmd_sockfd, 5) < 0) {
        perror("ERROR listen");
        close(cmd_sockfd);
        exit(EXIT_FAILURE);
    }
    cmd_data_sockfd = accept(cmd_sockfd, NULL, NULL);
    if (cmd_data_sockfd == -1) {
        perror("ERROR accept");
        close(cmd_sockfd);
        exit(EXIT_FAILURE);
    }

    for(;;) {
        ret = read(cmd_data_sockfd, buffer, BUFFER_SIZE);

         /* Ensure buffer is 0-terminated. */

        buffer[BUFFER_SIZE - 1] = 0;

        if (ret>0) {
            printf("Received %d bytes (%s) to send to server\n", ret, buffer);
            ret = write(cmd_data_sockfd, buffer, strlen(buffer));
            if (ret < 0) {
                perror("ERROR on write\n");
                close(cmd_data_sockfd);
                close(cmd_sockfd);
                unlink(command_socket_name);
                exit(EXIT_FAILURE);
            }
        } else {
            if (ret ==0) {
                printf("Peer has closed data cmd connection. Doing the same here.\n");
                close(cmd_data_sockfd);
                break;
            } else {
                perror("ERROR on read\n");
                close(cmd_data_sockfd);
                close(cmd_sockfd);
                unlink(command_socket_name);
                exit(EXIT_FAILURE);
            }
        }
    }
    close(cmd_sockfd);
    unlink(command_socket_name);

    return 0;
}