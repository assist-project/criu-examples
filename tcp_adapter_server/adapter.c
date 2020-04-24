#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/wait.h>
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
#include "adapter.h"

static int stop = 0;

static void sh(int sig)
{
    printf("Signal\n");
    if (sig == SIGUSR1) {
        stop = 1;
    }
}

void show_linger(int sockfd, char *where) {
    struct linger sl2;
    int r, opt_len = sizeof(struct linger);
    bzero((struct linger *) &sl2, opt_len);
    r = getsockopt(sockfd, SOL_SOCKET, SO_LINGER, &sl2, &opt_len);
    printf("%s : %d SL(onoff=%d,linger=%d)\n", where, r, sl2.l_onoff, sl2.l_linger);
}

void connect_to_server(int *sockfd, int server_port) {
    struct sockaddr_in servaddr, claddr; 
  
    // socket create and varification 
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1, off = 0;
    int opt_len = sizeof(struct linger);
    struct linger sl;
    bzero((struct linger *) &sl, sizeof(opt_len));
    sl.l_onoff = 1;		/* non-zero value enables linger option in kernel */
    sl.l_linger = 0;	/* timeout interval in seconds */

    if(setsockopt(*sockfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR ),(char*)&on,sizeof(on)) < 0
    || setsockopt(*sockfd, SOL_SOCKET, SO_LINGER, &sl, sizeof(struct linger)) < 0
    )
    {
        perror("ERROR setsockopt\n");
        exit(EXIT_FAILURE);
    } 
    if (*sockfd == -1) {
        perror("(adapter) socket creation failed...\n");  
        exit(EXIT_FAILURE);
    }
    else
        printf("(adapter) socket successfully created..\n"); 

    show_linger(*sockfd, "after setopt");
    bzero((char *) &claddr, sizeof(claddr));
    claddr.sin_family = AF_INET;
    claddr.sin_addr.s_addr = INADDR_ANY;
    claddr.sin_port = htons(ADAPTER_PORT);
    if (bind(*sockfd, (struct sockaddr *) &claddr, sizeof(claddr)) < 0) {
        perror("ERROR on binding");
        exit(EXIT_FAILURE);
    }

    bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(server_port); 
  
    // connect the client socket to server socket 
    if (connect(*sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
        perror("(adapter) connection with the server failed...\n"); 
        exit(EXIT_FAILURE);
    } 
    else
        printf("(adapter) connected to the server..\n"); 
    
    show_linger(*sockfd, "after connect");
}

void setup_command_socket(int *sockfd, char * socket_name) {
     int newsockfd, portno, clilen, pid, option;
    struct sockaddr_un cmd_addr;
    *sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*sockfd < 0)  {
        perror("(adapter) ERROR creating socket\n");
        exit(EXIT_FAILURE);
    }

    bzero((char *) &cmd_addr, sizeof(cmd_addr));

    cmd_addr.sun_family = AF_UNIX;
    strncpy(cmd_addr.sun_path, socket_name, sizeof(cmd_addr.sun_path) - 1);

    if (bind((*sockfd), (const struct sockaddr *) &cmd_addr, sizeof(struct sockaddr_un)) < 0) {
        perror("(adapter) ERROR on binding\n");
        exit(EXIT_FAILURE);
    }
}

/*
The adapter mediates communication between the server and the main process.
The idea is, when we take a snapshot of the server, we also include the adapter (which should parent the server).
That way we can reliably snapshot the TCP connection.

The adapter offers the following functions:
1. deliver data to the server
2. terminate the current command connection with main, while maintaining data connection with server. 
This prepares the adapter to be snapshoted.
This is enacted by simply closing the connection in the main.
3. terminate the server process and the current command connection, and exit.
*/

int run_adapter(int s_pid, int server_port, char *command_socket_name) {
    int server_data_sockfd, cmd_sockfd, cmd_data_sockfd, num, ret;
    char buffer[BUFFER_SIZE];
    if (signal(SIGUSR1, sh) == SIG_ERR) {
        perror("(adapter) ERROR signal\n");
		exit(EXIT_FAILURE);
    }

    setup_command_socket(&cmd_sockfd, command_socket_name);
    //connect_to_server(&dsockfd, server_port);
    if (listen(cmd_sockfd, 5) < 0) {
        perror("(adapter) ERROR listen");
        close(cmd_sockfd);
        exit(EXIT_FAILURE);
    }

    sleep(1);
    connect_to_server(&server_data_sockfd, server_port);

    while(!stop) {
    cmd_data_sockfd = accept(cmd_sockfd, NULL, NULL);
    if (cmd_data_sockfd == -1) {
        perror("(adapter) ERROR accept");
        goto error;
    }

        for(;;) {
            show_linger(server_data_sockfd, "in server loop");
            int on = 1, off = 0;
            bzero(buffer, BUFFER_SIZE); 
            ret = read(cmd_data_sockfd, buffer, BUFFER_SIZE);

            /* Ensure buffer is 0-terminated. */

            buffer[BUFFER_SIZE - 1] = 0;

            /* main has closed socket */
            if (ret ==0) {
                printf("Main has closed data cmd connection. Doing the same here.\n");
                break;
            } else {
                if (ret < 0) {
                    perror("(adapter) ERROR on read\n");
                    goto error;
                }
            }

            /* main has issued stop command */
            if (strcmp(buffer, STOP_CMD) == 0) {
                stop = 1;
                break;
            }
            
            /* main is just delivering data */
            printf("(adapter) Received %d bytes (%s) to send to server\n", ret, buffer);
            ret = write(server_data_sockfd, buffer, strlen(buffer));
            if (ret < 0) {
                perror("(adapter) ERROR on write to server\n");
                goto error;
            }

            bzero(buffer, BUFFER_SIZE); 
            ret = read(server_data_sockfd, buffer, BUFFER_SIZE);
            if (ret < 0) {
                perror("(adapter) ERROR on read from server\n");
                goto error;
            }

            printf("(adapter) Received %d bytes (%s) from server to main\n", ret, buffer);
            
            ret = write(cmd_data_sockfd, buffer, ret);
            if (ret < 0) {
                perror("(adapter) ERROR on write to main\n");
                goto error;
            }
        }
        //setsockopt(cmd_data_sockfd, )
        //kill(s_pid, SIGKILL);
        //close(cmd_data_sockfd);
    }
    printf("(adapter) Main has terminated all communication %d\n", server_data_sockfd);
    int status;
    kill(s_pid, SIGKILL);
    waitpid(s_pid, &status, 0);
    close(server_data_sockfd);
    printf("(adapter) closing command data and listening sockets\n");
    close(cmd_data_sockfd);
    close(cmd_sockfd);
    printf("(adapter) unlinking\n");
    unlink(command_socket_name);
    return EXIT_SUCCESS;

    error: 
    close(cmd_data_sockfd);
    close(cmd_sockfd);
    close(server_data_sockfd);
    unlink(command_socket_name);
    exit(EXIT_FAILURE);
}