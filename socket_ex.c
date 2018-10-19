/*
 * Most info from 
 * https://beej.us/guide/bgnet/
 *
 * With some notes/inspiration taken from:
 * https://github.com/lovenery/c-chatroom
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#include <pthread.h>

#define MAXCLIENTS 30
int client_socks[MAXCLIENTS];
int disconnected_clients[MAXCLIENTS];
int disconnected_clients_index = 0;
int num_clients = 0;

/* Gets the ip address as a string */
void *get_in_addr(struct sockaddr *sa)
{
    // IPv4
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    else {
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
}

void send_global_message(const char *message)
{
    int i;

    for (i = 0; i < num_clients; i++)
    {
        if (client_socks[i] != -1)
        {
            printf("Sending message: %s", message);
            send(client_socks[i], message, strlen(message), 0);    
        }
        else {
            printf("Skipping disconnected (-1) client!\n");
        }
        
    }
}

struct listen_thread_arg
{
    const int   sockfd; // socket number
    const char *name;   // username
    const char *ip;     // ip num
};

/* Continuously Listen to a client */
void *listen_to_client(void *data)
{
    struct listen_thread_arg *l = (struct listen_thread_arg *)data;
    int sockfd = l->sockfd;
    int nbytes; // number of bytes receieved
    char message[100];
    char sendmsg[100];
    char username[100];
    char ip[100];
    
    int i;

    /* Have to copy because if you use just the pointer
     * it will change (from outside the thread) as other
     * users connect */
    strcpy(username, l->name);
    strcpy(ip, l->ip);
    
    printf("Thread for user %s: sock %d\n", username, sockfd);

    while ((nbytes = recv(sockfd, message, 99, 0)) > 0)
    {
        message[nbytes] = '\0';

        printf("%s (%s):\t%s", username, ip, message);
        sprintf(sendmsg, "%s (%15s):\t%s", username, ip, message);

        send_global_message(sendmsg);
    }

    printf("Exiting thread for user %s: sock %d\n", username, sockfd);

    close(sockfd);
    
    /* Remove the user from the list of connected users */
    for (i = 0; i < num_clients; i++)
    {
        if (client_socks[i] == sockfd) {
            client_socks[i] = -1;
            
            printf("Found disconnected sockfd: index: %d\n", i);
            
            /* Store the index of the disconnected client, so it
             * can be replaced later */
            disconnected_clients[disconnected_clients_index++] = i;
            
            /* Send a message saying we've disconnected */
            sprintf(message, "SERVER: %s (%s) has disconnected!", username, ip);
            send_global_message(message);
            
            break;
        }
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    pthread_t listen_threads[MAXCLIENTS];
    int i;

    struct addrinfo hints, *res;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size;
    int sockfd, sockfd_client;

    char message[100];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my ip plz

    getaddrinfo(NULL, "8096", &hints, &res);

    /* Create a socket */
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    /* Bind it to the port passed into getaddrinfo */
    bind(sockfd, res->ai_addr, res->ai_addrlen);

    /* Listen for connections */
    listen(sockfd, MAXCLIENTS);

    /* Keep Listening for clients and print stuff to them! */
    for (;;)
    {
        char name[INET6_ADDRSTRLEN];

        /* Accept a connection, creating a new socket just for them! */
        client_addr_size = sizeof(client_addr);
        sockfd_client = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);

        /* Get some client info... */
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*)&client_addr), name, sizeof(name));
        printf("Server: got connection from %s\n", name);


        /* If there are no empty spots to fill */
        if (disconnected_clients_index == 0)
        {
            /* Push the client fd to the list of clients
             * and increase the total count */
            client_socks[num_clients++] = sockfd_client;
        }
        /* Otherwise fill in an empty spot */
        else {
            
            printf("Filling in empty socket at index: %d\n", disconnected_clients[disconnected_clients_index-1]);
            
            /* Overwrite the disconnected entry */
            client_socks[disconnected_clients[disconnected_clients_index-1]] = sockfd_client;
            
            /* Decrease the amount of blank entries */
            disconnected_clients_index--;
        }

        /* Create a thread to listen for messages from this client 
         * the `name` variable in this case is the string of the client's ip */
        struct listen_thread_arg l = { sockfd_client, "anon", name };
        pthread_create(&listen_threads[num_clients-1], NULL, listen_to_client, (void*)&l);


        /* Tell everyone another user has connected */
        sprintf(message, "SERVER: anon has joined from ip: %s", name);
        send_global_message(message);
    }


    for (i = 0; i < num_clients; i++)
    {
        pthread_join(listen_threads[i], NULL);
    }

    /* We're done sending data */
    close(sockfd);

    return 0;
}


