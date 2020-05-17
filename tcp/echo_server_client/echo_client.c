/* This is a simple client that connects and sends messages to an echo server.*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MYPORT "6666"
#define MAXDATASIZE 280

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int csock;                                       // sock fd to connect to the server
    int numbytes;                                    // number of bytes sent/received
    struct addrinfo hints, *srvinfo, *pAf;           // hold addr info related to the server
    int retval;
    char tbuf[MAXDATASIZE] = {0,};                   // message to be sent
    char addr_str[INET6_ADDRSTRLEN] = {0,};          // array to hold human-readable addr

    
    if(argc != 2)
    {
        fprintf(stderr, "usage: echo_client hostname\n");
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((retval = getaddrinfo(argv[1], MYPORT, &hints, &srvinfo) != 0))
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retval));
        return 1;
    }

    for(pAf = srvinfo; pAf != NULL; pAf->ai_next)
    {
        if((csock = socket(pAf->ai_family, pAf->ai_socktype,
            pAf->ai_protocol)) == -1)
        {
            perror("echo_cleint: socket");
            continue;
        }

        if(connect(csock, pAf->ai_addr, pAf->ai_addrlen) == -1)
        {
            close(csock);
            perror("echo_client: connect");
            continue;
        }

        break;
    }

    if(pAf == NULL)
    {
        fprintf(stderr, "echo_client: failed to connect\n");
        return 2;
    }

    inet_ntop(pAf->ai_family, get_in_addr((struct sockaddr*)pAf->ai_addr),
            addr_str, sizeof(addr_str));
    printf("echo_client: connecting to %s\n", addr_str);

    freeaddrinfo(srvinfo);

    if ((numbytes = recv(csock, tbuf, MAXDATASIZE-1, 0)) == -1) 
    {
        perror("echo_client: recv()");
        exit(EXIT_FAILURE);
    }
    tbuf[numbytes] = '\0';
    printf("Received:  %s\n", tbuf);

    while(1)
    {
        if((fgets(tbuf, MAXDATASIZE, stdin) == NULL))
        {
            fprintf(stderr, "echo_server: error reading input, please try again\n");
            continue;
        }

        numbytes = strlen(tbuf);

        if(send(csock, tbuf, numbytes, 0) == -1)
        {
            perror("echo_server: send()");
            exit(EXIT_FAILURE);
        }

        if(strncmp(tbuf, "disconnect", strlen("disconnect"))== 0)
        {
            close(csock);
            break;
        }
        
        if ((numbytes = recv(csock, tbuf, MAXDATASIZE-1, 0)) == -1) 
        {
            perror("echo_client: recv()");
            exit(EXIT_FAILURE);
        }
        
        printf("Received:  %s\n", tbuf);
    }

    return 0;
}
