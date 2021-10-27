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

#define PORT "3490"
#define BACKLOG 10

void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
int main(void)
{
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
    } // p->ai_next is the equivalent of ding (*p).ai_next (remember that p is a pointer)
    
    freeaddrinfo(servinfo);

    if(p==NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if((listen(sockfd, BACKLOG))==-1){
        perror("listen");
        exit(1);
    }

    //line below is for dead process but I didn't make a helper function for it
    //sa.sa_handler = sigchild_handler;
    //sigempty(&sa.sa_mask)
    //sa.sa_flags = SA_RESTART;
    //if (sigaction(SIGCHILD, &sa, NULL) == -1) {
    //    perror("sigaction");
    //    exit(1);
    //}

    printf("server: waiting for connections ... \n");
    while(1){
        sin_size = sizeof(their_addr);
        if((new_fd = accept(sockfd,(struct sockaddr *)&their_addr, &sin_size))==-1){
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
        printf("server got connection from %s\n", s);
        char buffer[30000]={0};
        if(read(new_fd, buffer, 30000)==-1){
            perror("read");
            continue;
        }
        printf("%s\n", buffer);
        close(new_fd);
    }


}