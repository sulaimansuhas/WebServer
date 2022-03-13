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
#include <sys/wait.h>
#include <signal.h>
#include <sys/epoll.h>

#define PORT "3490"
#define BACKLOG 10
#define EPOLL_MAXEVENTS 10

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int epoll_register(int events, int efd, int sock){
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = sock;
    if (epoll_ctl(efd, EPOLL_CTL_ADD,sock, &ev) == -1){
        return 0;
    }
    return 1;
}


int main(void)
{
    int poller;
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p; //servinfo and p are pointers to addrinfo structs
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    memset(&hints,0,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;



    char *header = "HTTP/1.1 200 OK\r\n\r\n";
    // char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nHello world!";



    if((poller = epoll_create1(0))==0){
            perror("epoll creation");
    }

    if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo))!=0){ //deals with the error case with getaddrinfo()
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    //loop through all results and bind to the first one we can, as servinfo should be returned as a linked list

    for(p = servinfo ; p!=NULL;p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))==-1){
           perror("server:socket"); 
           continue;
        }
        if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)))==-1){
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p -> ai_addrlen)==-1){
            close(sockfd);
            perror("server:bind");
            continue;
        }
        
        break;
    } // p->ai_next is the equivalent of doing (*p).ai_next (remember that p is a pointer)
    
    freeaddrinfo(servinfo);
    if(epoll_register(EPOLLIN, poller, sockfd) == 0){
            perror("epoll add");
            exit(1);
        }

    if(p==NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if((listen(sockfd, BACKLOG))==-1){
        perror("listen");
        exit(1);
    }
    printf("server: waiting for connections ... \n");
    //line below is for dead process but I didn't make a helper function for it 
    //sigempty(&sa.sa_mask) 
    while(1){
        //epoll here blocks indefinitely. Should be ok
        struct epoll_event events[EPOLL_MAXEVENTS];
        int n = epoll_wait(poller, events, EPOLL_MAXEVENTS, -1);
        printf("number of events:%d\n", n);
        int i ;
        for(i = 0; i<n; i++){
            printf("have entered the loop\n");
            if (events[i].data.fd == sockfd){
                sin_size = sizeof(their_addr);
                inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
                printf("server got connection from %s\n", s);
                if((new_fd = accept(sockfd,(struct sockaddr *)&their_addr, &sin_size))==-1){
                    perror("accept");
                    continue;
                }
                if(epoll_register(EPOLLIN, poller, new_fd) == 0){
                    perror("epoll add");
                    exit(1);
                    close(sockfd);
                }
            }
            else{
                if ( (new_fd = events[i].data.fd) < 0)
                    continue;
                printf("non listening socket %d called\n", events[i].data.fd);
                char buffer[30000]={0};
                int nbr;
                if((nbr = read(events[i].data.fd, buffer, 30000))==-1){
                    if(errno == ECONNRESET){
                        close(new_fd);
                        events[i].data.fd = -1;
                    }
                    else{
                        perror("read");
                        continue;
                    }
                }
                else if(nbr == 0){
                    close(new_fd);
                    events[i].data.fd = -1;
                }
                printf("bytes read:%d\n", nbr);
                printf("Socket %d received: %s", events[i].data.fd, buffer);
                printf("---------------------\n");
                printf("sending back: %s\n", header);
                int nbs;
                nbs = send(events[i].data.fd,header, strlen(header), 0);
                // printf("number of bytes sent: %d\n\n", nbs);
                printf("%d is the size of the header\n",(int)(strlen(header)));
                // write(events[i].data.fd, header, strlen(header));
                
            }
        }
    }


}