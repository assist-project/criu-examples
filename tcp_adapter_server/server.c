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
#include "connection.h"
//#include "criu/criu.h"
//#include <fcntl.h>
//#include <signal.h>


void do_stuff_server (int sock)
{
    int n;
    char buffer[BUFFER_SIZE];
    int expected_num=0, received_num;

    while (1) {
        bzero(buffer,BUFFER_SIZE);
        n = read(sock,buffer,BUFFER_SIZE);
        if (n < 0) {
            perror("(server) ERROR reading from socket\n");
            break;
        }
        if (n == 0) {
            printf("(server) Connection closed by client\n");
            break ;
        } 
        printf("(server) Recv from client: %s\n", buffer);
        sscanf(buffer, "%d", &received_num);
        bzero(buffer, BUFFER_SIZE);
        if (received_num == expected_num) {
            sprintf(buffer, "ACK %d", expected_num);
            expected_num ++;
            n = write(sock, buffer, strlen(buffer));
            if (n < 0) {
                perror("(server) ERROR writing to socket\n");
                break;
            }
        } else {
            sprintf(buffer, "Invalid ACK %d, expected %d\n", received_num, expected_num );
            n = write(sock, buffer, strlen(buffer));
            if (n < 0) {
                perror("ERROR writing to socket\n");
                break;
            }
        }
    }
    close(sock);
    printf("(server) Final seq number is %d\n", expected_num);
}

int run_server(int server_port)
{
    int sockfd=0, newsockfd=0, portno, clilen, pid, option=1;
    struct sockaddr_in serv_addr, cli_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket\n");
        goto error;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(server_port);
    if(setsockopt(sockfd,SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0)
    {
        perror("setsockopt failed\n");
        close(sockfd);
        exit(2);
    }

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding\n");
        goto error;
    }
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
  //   while (1) {
        newsockfd = accept(sockfd, 
            (struct sockaddr *) &cli_addr, &clilen);
        printf("(server) accepted new connection\n");
        if (newsockfd < 0) {
            perror("ERROR on accept\n");
            goto error;
        }
        do_stuff_server(newsockfd);
        close(newsockfd);
//     } /* end of while */
    close(sockfd);
    printf("(server) exiting\n");
    return 0; 

    error:
    printf("(server) exiting with error\n");
    if (!sockfd) {
        close(sockfd);
    }
    if (!newsockfd) {
        close(newsockfd);
    }
}