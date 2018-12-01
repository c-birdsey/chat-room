//calder birdsey
//cs315 systems programming 
//assignment 7: chat-client 

/****** HOW TO TERMINATE IF ONE OF THE THREADS TERMINATES ********/

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h> 
#include <signal.h>
#include <time.h>

#define BUF_SIZE 4096

//prototypes
void * client_reciever(void * fd); 
void * client_sender(void * fd);

pid_t sender_pid; 

int 
main(int argc, char *argv[]) {
    char *dest_hostname, *dest_port;
    struct addrinfo hints, *res;
    int conn_fd;
    int rc;
    //char buf[BUF_SIZE];
    //int n;

    dest_hostname = argv[1];
    dest_port = argv[2];

    // create socket and check for socket error 
    if((conn_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket"); 
        exit(1); 
    }
    
    // find IP of server 
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if((rc = getaddrinfo(dest_hostname, dest_port, &hints, &res)) != 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(2);
    }

    // connect to server and check for connect error 
    if(connect(conn_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        exit(2);
    }
    printf("Connected\n");

    pthread_t sender;
    pthread_create(&sender, NULL, client_sender, &conn_fd);

    pthread_t reciever;
    pthread_create(&reciever, NULL, client_reciever, &conn_fd);

    //pthread_join(reciever, NULL);
    pthread_join(sender, NULL);
   
    //close
    pthread_cancel(reciever); 
    close(conn_fd);
    pthread_exit(NULL); 
}

void *
client_sender(void * fd) {
    int n;
    char buf[BUF_SIZE];
    int conn_fd = *((int *) fd);
    sender_pid = getpid(); 

    while((n = read(0, buf, BUF_SIZE)) > 0) { //fgets?
        //send to server
        send(conn_fd, buf, n, 0); 
    }  
    write(1, "Exiting.\n", 10); 
    return NULL;
} 

void *
client_reciever(void * fd) {
    int bytes_recieved; 
    char msg[BUF_SIZE];
    int conn_fd = *((int *) fd); 
    
    //print messages recieved from server 
    while((bytes_recieved = recv(conn_fd, msg, BUF_SIZE, 0)) > 0) {
        fflush(stdout);
        //get time
        const time_t cur_time = time(NULL); 
        struct tm *local_time = localtime(&cur_time); 

        //print data recieved
        char time[11]; 
        strftime(time, 11, "%H:%M:%S: ", local_time);
        write(1, time, sizeof(time));
        puts(msg);
        memset(msg, 0, BUF_SIZE); 
    }   
    write(1, "Connection closed by remote host.\n", 35); 
    kill(sender_pid, SIGTERM); 
    return NULL; 
}