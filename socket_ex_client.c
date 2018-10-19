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

#include <ncurses.h>

struct thread_arg
{
    int sockfd;
    WINDOW *out_win;
};

struct sigaction old_action;
pthread_t listen_thread;

void sigint_handler(int signum)
{
    //printf("Ctrl C Pressed!\n");
    sigaction(SIGINT, &old_action, NULL);
    
    pthread_kill(listen_thread, SIGINT);

    kill(0, SIGINT);
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

WINDOW *create_newwin(int height, int width, int starty, int startx)
{
    WINDOW *local_win;

    local_win = newwin(height, width, starty, startx);
    box(local_win, 0, 0); // last two args specify the
                          // characters for the window border - 0,0 is default

    wrefresh(local_win);

    return local_win;
}

void update_win(WINDOW *win)
{
    // restore borders
    wborder(win, 0, 0, 0, 0, 0, 0, 0, 0);
    wrefresh(win);
}

void destroy_win(WINDOW *win)
{
    // remove the borders of the window
    wborder(win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');

    wrefresh(win);
    delwin(win);
}

/* Listening thread */
void *listen_func(void *arg)
{
    char message[100];
    //int sockfd = *((int *)arg);
    int sockfd;
    int nbytes = 0; // number of bytes receieved from server
    
    struct thread_arg *t = (struct thread_arg *)arg;
    sockfd = t->sockfd;
    WINDOW *out_win = t->out_win;
    
    int outy = 0, outx = 0;
    
    //printf("Sockfd: %d\n", sockfd);

    //num_bytes_received = recv(sockfd, message, 99, 0);
    while((nbytes = recv(sockfd, message, 99, 0)) > 0) 
    {
        message[nbytes] = '\0';

        //printf("\r:%s", "> ");
        //fflush(stdout);

        //printf("%s", message);
        /* Move the cursor one over from the border */
        getyx(out_win, outy, outx);
        wmove(out_win, outy, outx+1);

        /* Print out the message */
        wprintw(out_win, "%s\n", message);
        update_win(out_win);
    }

    //printf("nbytes: %d\n", nbytes);
    //printf("errno: %d\n", errno);

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    struct addrinfo hints, *servinfo;
    int sockfd;
    char servname[INET6_ADDRSTRLEN];
    int num_bytes_received;
    //char *message = NULL;
    //size_t message_len;
    char buff[100] = "";
    char buffIndex = 0;
    
    /* Ncurses info */
    WINDOW *test_win, *out_win;
    int startx, starty, width, height;
    int outy, outx;
    int ch;

    /* Make sure we got the hostname to use */
    if (argc != 2) {
        printf("Usage: %s <host>\n", argv[0]);
    }
    
    initscr();
    //cbreak(); // unlike raw(), allow for signals to be interpreted
    raw();

    noecho(); // don't print out as im typing plz

    //keypad(stdscr, TRUE); // lemme use F1 please

    height = 3;
    width = COLS;

    starty = (LINES - height); // center of window
    startx = 0;

    /* User input window */
    test_win = create_newwin(height, width, starty, startx);

    /* Total output window */
    out_win = create_newwin(LINES - height, COLS, 0, 0);
    scrollok(out_win, TRUE);

    refresh();
    
    update_win(out_win);
    update_win(test_win);
    
    /* Move the initial top window cursor position away from the corner */
    getyx(out_win, outy, outx);
    wmove(out_win, outy+1, outx);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my ip thanks

    getaddrinfo(argv[1], "8096", &hints, &servinfo);

    /* Create a socket */
    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    //printf("Sockfd: %d\n", sockfd);

    /* Connect to the socket */
    connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);

    /* Get some server info */
    inet_ntop(servinfo->ai_family, get_in_addr((struct sockaddr*)servinfo->ai_addr), servname, sizeof(servname));
    //printf("Client connected to: %s\n", servname);

    /* We're done with the addrinfo */
    freeaddrinfo(servinfo);

    /* Kill the pthread on close */
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sigint_handler;
    sigaction(SIGINT, &action, &old_action);

    /* Create a listen thread */
    struct thread_arg t = { sockfd, out_win };
    pthread_create(&listen_thread, NULL, listen_func, (void *)&t);
    
    while((ch = wgetch(test_win)) != 27) // escape key
    {
        /* Enter Key */
        if (ch == 10) {
            
            /* Send the message to the server */
            send(sockfd, buff, 99, 0);

            /* Clear the input window */
            wclrtoeol(test_win);
            update_win(test_win);

            /* Clear the input buffer */
            memset(buff, 0, 100);
            buffIndex = 0;
        }
        else {
            /* Backspace - delete the last buffer entry */
            if (ch == 127) {
                buff[buffIndex] = 0;
                buff[--buffIndex] = 0;
            }
            /* Otherwise enter the character into the buffer */
            else {
                buff[buffIndex++] = ch;
            }
            
            /* Clear the entry window and rerwrite the current buffer */
            wclrtoeol(test_win);
            mvwprintw(test_win, 1, 1, buff);
            update_win(test_win);
        }
    }

    /* Wait for the listening thread to finish */
    //pthread_join(listen_thread, NULL);
    pthread_kill(listen_thread, SIGINT);

    close(sockfd);
    destroy_win(test_win);
    destroy_win(out_win);
    endwin();
    pthread_exit(NULL);
}


