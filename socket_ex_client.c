#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include <pthread.h>

struct sigaction old_action;
pthread_t listen_thread;

void sigint_handler(int signum)
{
    printf("Ctrl C Pressed!\n");
    sigaction(SIGINT, &old_action, NULL);
    
    pthread_kill(listen_thread, SIGINT);

    kill(0, SIGINT);
}

/* Listening thread */
void *listen_func(void *arg)
{
    char message[100];
    int sockfd = *((int *)arg);
    int nbytes = 0; // number of bytes receieved from server
    
    printf("Sockfd: %d\n", sockfd);

    //num_bytes_received = recv(sockfd, message, 99, 0);
    while((nbytes = recv(sockfd, message, 99, 0)) > 0) 
    {
        message[nbytes] = '\0';

        printf("\r:%s", "> ");
        fflush(stdout);

        printf("%s", message);
    }

    printf("nbytes: %d\n", nbytes);
    printf("errno: %d\n", errno);

    pthread_exit(NULL);
}

void *get_in_addr(struct sockaddr *sa)
{
    // IPv4
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    // IPv6
    else {
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
}

int main(int argc, char *argv[])
{
    struct addrinfo hints, *servinfo;
    int sockfd;
    char servname[INET6_ADDRSTRLEN];
    int num_bytes_received;
    char *message = NULL;
    size_t message_len;

    /* Make sure we got the hostname to use */
    if (argc != 2) {
        printf("Usage: %s <host>\n", argv[0]);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my ip thanks

    getaddrinfo(argv[1], "8096", &hints, &servinfo);

    /* Create a socket */
    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    printf("Sockfd: %d\n", sockfd);

    /* Connect to the socket */
    connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);

    /* Get some server info */
    inet_ntop(servinfo->ai_family, get_in_addr((struct sockaddr*)servinfo->ai_addr), servname, sizeof(servname));
    printf("Client connected to: %s\n", servname);

    /* We're done with the addrinfo */
    freeaddrinfo(servinfo);

    /* Kill the pthread on close */
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sigint_handler;
    sigaction(SIGINT, &action, &old_action);

    /* Create a listen thread */
    pthread_create(&listen_thread, NULL, listen_func, (void *)&sockfd);

    /* Get user input and send it */
    do
    {
        //printf("Enter a message: >");
        getline(&message, &message_len, stdin);
        //printf("Got message: %s", message);

        send(sockfd, message, message_len, 0);

    /* This while doesn't work atm... */
    } while (strcmp(message, "quit\n") != 0 && strcmp(message, "exit\n") != 0);

    /* Wait for the listening thread to finish */
    pthread_join(listen_thread, NULL);

    /* `message` is allocated by getline, we need to free it */
    free(message);

    close(sockfd);
    pthread_exit(NULL);
}


