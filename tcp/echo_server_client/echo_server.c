/* This is a simple echo server
 * When it accepts a connection, it identfies itself to the client.
 * After this, it "echos" the client messages
 */

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
#define BACKLOG 5
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

int main(int argc, char* argv[])
{
    int lsock, csock;                                       // listen on lsock and connect on csock
    struct addrinfo hints, *srvinfo, *lstptr, *pAf;         // to create/fill the listener socket for the server
    struct sockaddr_storage cli_addr;                       // client addr/port
    socklen_t addr_sz;                                      // client addr/port struct len
    int retv;                                               // return values of various system calls
    int yes = 1;                                            // for use in setsockopt()
    char tbuf[MAXDATASIZE] = {0,};                          // data transfer buffer
    char addr_str[INET6_ADDRSTRLEN] = {0,};                 // array to hold human-readable addr
    int numbytes;                                           // data transferred in bytes

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((retv = getaddrinfo(NULL, MYPORT, &hints, &srvinfo) != 0))
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retv));
        exit(EXIT_FAILURE);
    }
   
    for(pAf = srvinfo; pAf != NULL; pAf = pAf->ai_next)
    {
        if((lsock = socket(pAf->ai_family, pAf->ai_socktype, 
            pAf->ai_protocol)) == -1)
        {
            perror("echo_server: socket()");
            continue;
        }

        if(setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &yes, 
            sizeof(int)) == -1)
        {
            perror("echo_server: setsockopt()");
            exit(EXIT_FAILURE);
        }

        if(bind(lsock, pAf->ai_addr, pAf->ai_addrlen) == -1)
        {
            close(lsock);
            perror("echo_server: bind()");
            continue;
        }

        break;
    }

    freeaddrinfo(srvinfo);
    
    if(pAf == NULL)
    {
        fprintf(stderr, "echo_server: failed to bind\n");
        exit(EXIT_FAILURE);
    }

    if(listen(lsock, BACKLOG) == -1)
    {
        perror("echo_server: listen");
        exit(EXIT_FAILURE);
    }

    printf("echo_server: waiting for connection... ");

    addr_sz = sizeof(cli_addr);
    csock = accept(lsock, (struct sockaddr *)&cli_addr, &addr_sz);

    if(csock == -1)
    {
        perror("echo_server: accept");
        exit(EXIT_FAILURE);
    }

    inet_ntop(cli_addr.ss_family, 
        get_in_addr((struct sockaddr *)&cli_addr), 
        addr_str, sizeof(addr_str));
    printf("echo_server: connection from %s\n", addr_str);

    if(send(csock, "Greetings, From Echo Server", 27, 0) == -1)
    {
        perror("echo_server: recv()");
        exit(EXIT_FAILURE);
    }

    while(1)
    {
        if ((numbytes = recv(csock, tbuf, MAXDATASIZE-1, 0)) == -1) 
        {
            perror("echo_server: recv()");
            exit(EXIT_FAILURE);
        }
        
        tbuf[numbytes-1] = '\0';

        printf("Received:  %s\n", tbuf);

        if(strncmp(tbuf, "disconnect", strlen("disconnect")) == 0)
        {
            close(csock);
            break;
        }

        printf("%s\n", "echo...");

        if(send(csock, tbuf, numbytes, 0) == -1)
        {
            perror("echo_server: send()");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
