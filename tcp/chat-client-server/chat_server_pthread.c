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
#include <pthread.h>

#define MYPORT "6666"
#define BACKLOG 5
#define MAXDATASIZE 280
#define DISCONNECT 0XFF

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
                perror("chat_server: recv()");
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

int main(int argc, char* argv[])
{
    int lsock, csock;                                       // listen on lsock and connect on csock
    struct addrinfo hints, *srvinfo, *lstptr, *pAf;         // to create/fill the listener socket for the server
    struct sockaddr_storage cli_addr;                       // client addr/port
    socklen_t addr_sz;                                      // client addr/port struct len
    int retv;                                               // return values of various system calls
    int yes = 1;                                            // for use in setsockopt()
    pthread_t rx_thread;                                    // threads to receive
    char tbuf[MAXDATASIZE] = {0,};                          // buffer to hold send data
    int numbytes;                                           // number of bytes sent
    
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

    if(send(csock, "Greetings!\0", 11, 0) == -1)
    {
        perror("chat_server: recv()");
        exit(EXIT_FAILURE);
    }

/* cscok here is used to tx and rx data between the connected peers.
 * tx data comes in from stdin - 
 * grabbed by fgets - which is a blocking call - 
 * doesn't return until user enters something followed by '/r'.
 * In the meanwhile, we'd want to recieve anything being sent by the peer
 * via recv() which also blocks till it recieves from the other peer.
 * in order to be able to enter messages to send as well as receive
 * without one blocking the other - We need 2 threads of execution.
 */

    if(pthread_create(&rx_thread, NULL, &receive_msg, (void*)&csock) != 0)
    {
        perror("chat_server: pthread_create to recieve");
        exit(EXIT_FAILURE);
    }

    while(((fgets(tbuf, MAXDATASIZE, stdin)) != NULL) & (csock >= 0))
    {
        numbytes = strlen(tbuf);
        
        if(send(csock, tbuf, numbytes, 0) == -1)
        {
            perror("chat_server: send()");
            exit(EXIT_FAILURE);
        }
    }

    close(lsock);

    printf("Session Terminated!\n");
    return 0; 
}

