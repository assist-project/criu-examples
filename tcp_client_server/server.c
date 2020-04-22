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
//#include <signal.h>


int run_server(int server_port)
{
    int sockfd, newsockfd, portno, clilen, pid, option;
    struct sockaddr_in serv_addr, cli_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(server_port);
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