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
#include <pthread.h>

#define MYPORT "6666"
#define MAXDATASIZE 280
#define DISCONNECT 0xFF

/* don't need a mutex for locking the screen - stdin/stdout
 * as the messages to be sent go to stdin and the received messages 
 * are places in stdout. Needless to say, these are separate buffers.
 */
//static pthread_mutex_t stdin_lock = PTHREAD_MUTEX_INITIALIZER;
static char addr_str[INET6_ADDRSTRLEN] = {0,}; // array to hold human-readable addr

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void* receive_msg(void *sockfd)
{
    char tbuf[MAXDATASIZE] = {0,};
    int numbytes;

    while(1)
    {
        numbytes = recv(*(int*)sockfd, tbuf, MAXDATASIZE-1, 0);
        
        if(numbytes <=0)
        {
            if(numbytes == 0)
            {
                printf("%s hung up\n", addr_str);
            }
            else
            {
                perror("chat_client: recv()");
            }
            
            close(*(int*)sockfd);
            *(int*)sockfd = -1;
            break;
        }
        else
        {
            tbuf[numbytes-1] = '\0';
            printf("%s:  %s\n", addr_str, tbuf);
        }
    }
}

int main(int argc, char *argv[])
{
    int csock;                                       // sock fd to connect to the server
    struct addrinfo hints, *srvinfo, *pAf;           // hold addr info related to the server
    int retval;                                      // return values of various system calls
    pthread_t rx_thread;                             // threads to send/receive
    char tbuf[MAXDATASIZE] = {0,};                   // buffer to hold send data
    int numbytes;                                    // number of bytes sent
    
    if(argc != 2)
    {
        fprintf(stderr, "usage: chat_client hostname\n");
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
            perror("chat_client: socket");
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

    if ((numbytes = recv(csock, tbuf, MAXDATASIZE-1, 0)) == -1) 
    {
        perror("chat_client: recv()");
        exit(EXIT_FAILURE);
    }
    tbuf[numbytes] = '\0';
    printf("%s:  %s\n", addr_str, tbuf);

    if((pthread_create(&rx_thread, NULL, &receive_msg, (void*)&csock) != 0))
    {
        perror("chat_client: pthread_create to recieve");
        exit(EXIT_FAILURE);
    }

    while(((fgets(tbuf, MAXDATASIZE, stdin)) != NULL) & (csock >= 0))
    {
        numbytes = strlen(tbuf);
        
        if(send(csock, tbuf, numbytes, 0) == -1)
        {
            perror("chat_client: send()");
            exit(EXIT_FAILURE);
        }
    }

    close(csock);
    printf("Session Terminated!\n");
    return 0; 
}

