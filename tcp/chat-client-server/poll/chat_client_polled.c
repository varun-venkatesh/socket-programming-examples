/* This is a simple client 
 * that connects to and chats with server.
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
#define MAXDATASIZE 280
#define DISCONNECT 0xFF

static char addr_str[INET6_ADDRSTRLEN] = {0,};          // array to hold human-readable addr

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
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
    int retv;
    char tbuf[MAXDATASIZE] = {0,};                   // message to be sent
    struct pollfd pfds[2];                           // to poll the connection socket for read/write viabilityi
    int fd_cnt;                                      // number of fds being polled
    
    if(argc != 2)
    {
        fprintf(stderr, "usage: chat_client hostname\n");
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((retv = getaddrinfo(argv[1], MYPORT, &hints, &srvinfo) != 0))
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retv));
        return 1;
    }

    for(pAf = srvinfo; pAf != NULL; pAf->ai_next)
    {
        if((csock = socket(pAf->ai_family, pAf->ai_socktype,
            pAf->ai_protocol)) == -1)
        {
            perror("chat_cleint: socket");
            continue;
        }

        if(connect(csock, pAf->ai_addr, pAf->ai_addrlen) == -1)
        {
            close(csock);
            perror("chat_client: connect");
            continue;
        }

        break;
    }

    if(pAf == NULL)
    {
        fprintf(stderr, "chat_client: failed to connect\n");
        return 2;
    }

    inet_ntop(pAf->ai_family, get_in_addr((struct sockaddr*)pAf->ai_addr),
            addr_str, sizeof(addr_str));
    printf("chat_client: connecting to %s\n", addr_str);

    freeaddrinfo(srvinfo);
    
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
            perror("chat_client_polled: poll");
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
                    perror("chat_client: recv()");
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
                perror("chat_client_polled: read from stdin");
                fprintf(stderr, "Error reading input. Try again\n");
            }
            else
            {
                if(send(csock, tbuf, numbytes, 0) == -1)
                {
                    perror("chat_client: send()");
                    break;
                }
            }
        }
    }

    close(csock);
    printf("Session Terminated!\n");
    return 0;
}

