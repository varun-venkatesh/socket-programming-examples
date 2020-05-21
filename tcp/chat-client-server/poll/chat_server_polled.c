/* This is a simple chat server
 * It accepts a connection from a client which initiates the chat.
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
#include <poll.h>

#define MYPORT "6666"
#define BACKLOG 5
#define MAXDATASIZE 280
#define DISCONNECT 0XFF

static char addr_str[INET6_ADDRSTRLEN] = {0,};               

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
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
    char tbuf[MAXDATASIZE] = {0,};                          // message to be sent
    int retv;                                               // return values of various system calls
    int yes = 1;                                            // for use in setsockopt()
    struct pollfd pfds[2];                                  // to poll the connection socket for read/write viabilityi
    int fd_cnt;                                             // number of fds being polled
    int numbytes;                                           // number of bytes read/written

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
            perror("chat_server: socket()");
            continue;
        }

        if(setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &yes, 
            sizeof(int)) == -1)
        {
            perror("chat_server: setsockopt()");
            exit(EXIT_FAILURE);
        }

        if(bind(lsock, pAf->ai_addr, pAf->ai_addrlen) == -1)
        {
            close(lsock);
            perror("chat_server: bind()");
            continue;
        }

        break;
    }

    freeaddrinfo(srvinfo);
    
    if(pAf == NULL)
    {
        fprintf(stderr, "chat_server: failed to bind\n");
        exit(EXIT_FAILURE);
    }

    if(listen(lsock, BACKLOG) == -1)
    {
        perror("chat_server: listen");
        exit(EXIT_FAILURE);
    }

    printf("chat_server: waiting for connection... ");

    addr_sz = sizeof(cli_addr);
    csock = accept(lsock, (struct sockaddr *)&cli_addr, &addr_sz);

    if(csock == -1)
    {
        perror("chat_server: accept");
        exit(EXIT_FAILURE);
    }

    inet_ntop(cli_addr.ss_family, 
        get_in_addr((struct sockaddr *)&cli_addr), 
        addr_str, sizeof(addr_str));
    printf("chat_server: connection from %s\n", addr_str);

/* The socket file descriptor - csock is used for sending and recieving data.
 * in order to send data, the user will have to input data into stdin
 * which is traditionally read using blocking calls like gets(), scanf() etc.
 * Similarly, receiving data also uses a blocking call recv(). 
 * These calls don't return until there's data on the file descriptor they're 
 * trying to read from.
 * In order to send/receive without either of these blocking calls holding up
 * other - we poll() the csock fd for incoming data - and then call recv() to read
 * the data that came in from the other side (the peer we're connected to).
 * Likewise, we poll() the standard input fd (fd==0) - stdin - 
 * for any data input by the user. We then use read to gather this data and
 * send() it to our connected peer.
 * This way, with one single thread of execution - main - we can "interlace" 
 * sending and receiving data over a socket to another peer.
 */


    pfds[0].fd = csock;
    pfds[0].events = POLLIN; 
    
    pfds[1].fd = 0; //polling stdin for user input
    pfds[1].events = POLLIN; 
    
    fd_cnt = 2;

    for(;;)
    {
        retv = poll(pfds, fd_cnt, -1);

        if(retv == -1)
        {
            perror("chat_server_polled: poll");
            exit(EXIT_FAILURE);
        }

        if(pfds[0].revents & POLLIN)
        {
            // We've probably got something to read!
            numbytes = recv(csock, tbuf, MAXDATASIZE-1, 0); 
            if(numbytes <= 0) 
            {
                if(numbytes == 0)
                {
                    printf("%s hung up\n", addr_str);
                }
                else
                {
                    perror("chat_server: recv()");
                }
                break;
            }
            else
            {
                tbuf[numbytes-1] = '\0';
                printf("%s:  %s\n", addr_str, tbuf);
            }
        }
        

        if(pfds[1].revents & POLLIN)
        {
            // something may have been input to stdin (fd==0)
            numbytes = read(0, tbuf, MAXDATASIZE-1);

            if(numbytes == -1)
            {
                perror("chat_server_polled: read from stdin");
                fprintf(stderr, "Error reading input. Try again\n");
            }
            else
            {
                if(send(csock, tbuf, numbytes, 0) == -1)
                {
                    perror("chat_server: send()");
                    break;
                }
            }
        }
    }

    close(csock);
    close(lsock);
    printf("Session Terminated!\n");
    return 0;
}

