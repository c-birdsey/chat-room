//calder birdsey
//cs315 systems programming 
//assignment 7: chat-server

/* FUNCTIONS
 * time(2) -- to get the time stamp in seconds:
 * kill(2)
 * strncopy(3)
 * snprintf(3)
*/
 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h> 
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>

#define BACKLOG 10
#define BUF_SIZE 4096

/* STRUCT AND INTIALIZER */
struct client {
    int fd;
    char remote_ip[15]; 
    char remote_port[15]; 
    char nickname[45];  
    struct client * next_client; 
}; 

struct client *first_client;

/* PROTOTYPES */
struct client *add_client(int fd, char * ip, uint16_t port); 
void *remove_client(int fd); 
void *reciever(void *client_data); 
void *send_all(char *message, int size);
char *check_nickname(char *msg); 
void *change_nickname(struct client *client_data, char *nick_name); 
void *print_clients(); //for testing

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; 

int 
main(int argc, char *argv[]){
    char *listen_port;
    int listen_fd, conn_fd;
    struct addrinfo hints, *res;
    int rc;
    struct sockaddr_in remote_sa;
    uint16_t remote_port;
    socklen_t addrlen;
    char *remote_ip; 
    listen_port = argv[1];



    //intialized head of linked list of clients
    if (first_client == NULL) {
        first_client = malloc(sizeof(struct client)); 
        first_client->fd = -1; 
        first_client->next_client = NULL; 
    }

    //create socket and check for error 
    if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0))  == -1) {
        perror("socket"); 
        exit(1); 
    }
    
    // bind to a port 
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if((rc = getaddrinfo(NULL, listen_port, &hints, &res)) != 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(2);
    }

    if (bind(listen_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind"); 
        exit(3); 
    }

    //start listening
    listen(listen_fd, BACKLOG);

    // infinite loop of accepting new connections and handling them
    while(1) {
        // accept a new connection
        addrlen = sizeof(remote_sa);
        if((conn_fd = accept(listen_fd, (struct sockaddr *) &remote_sa, &addrlen)) == -1) {
            perror("accept"); 
            exit(4); 
        }
    
        //announce connection 
        remote_ip = inet_ntoa(remote_sa.sin_addr);
        remote_port = ntohs(remote_sa.sin_port);
        printf("new connection from %s:%d\n", remote_ip, remote_port);

        //add new client ip/nickname to struct
        struct client *new_client = add_client(conn_fd, remote_ip, remote_port); 

        //new thread for reciever 
        pthread_t client_reciever; 
        pthread_create(&client_reciever, NULL, reciever, (void *) new_client); 
    }

    //close
    close(listen_fd); 
    close(conn_fd); 
    free(first_client);
    pthread_exit(NULL); 
}

void *
reciever(void *client) {
    int bytes_received;
    char message[BUF_SIZE];
    struct client * client_data = (struct client *) client; 
    int conn_fd = client_data->fd; 
    char *nick_name; 
    
    //extract needed data from struct 
    char ip[15]; 
    char port[15]; 
    char cur_nickname[45];  
    snprintf(port, 15, "%s", client_data->remote_port); 
    snprintf(ip, 15, "%s", client_data->remote_ip);

    while((bytes_received = recv(conn_fd, message, BUF_SIZE, 0)) > 0) {
        fflush(stdout);
        strtok(message, "\n"); 
        snprintf(cur_nickname, 45, "%s", client_data->nickname); 

        //check for nick name command 
        if(strncmp(message, "/nick", 5) == 0) { 
            //parse out nickname 
            nick_name = (strchr(message, ' ') + 1); 

            //compose nickname message
            char nickname_msg[sizeof(cur_nickname) + sizeof(nick_name) + 37]; 
            snprintf(nickname_msg, sizeof(nickname_msg), "%s is now known as %s.", cur_nickname, nick_name); 

            //write message to server output
            write(1, nickname_msg, sizeof(nickname_msg)); 
            write(1, "\n", 1); 

            //send nickname update message to all clients 
            send_all(nickname_msg, sizeof(nickname_msg));

            //change nickname in struct and clear msgs 
            change_nickname(client_data, nick_name);
            memset(nickname_msg, 0, sizeof(nickname_msg)); 
            memset(message, 0, BUF_SIZE); 
            continue; 
        }

        //send client message to all clients 
        char msg[sizeof(cur_nickname) + bytes_received + 1];
        snprintf(msg, sizeof(msg), "%s: %s", cur_nickname, message); 
        send_all(msg, sizeof(msg)); 

        //clear msgs
        memset(msg, 0, sizeof(msg)); 
        memset(message, 0, BUF_SIZE); 
    }
    //remove client from linked list and get nickname prior to removing node
    snprintf(cur_nickname, 45, "%s", client_data->nickname); 
    remove_client(conn_fd); 

    //print disconnection in server
    char discon_msg[sizeof(cur_nickname) + 23]; 
    snprintf(discon_msg, sizeof(discon_msg), "Lost connection from %s.\n", cur_nickname); 
    write(1, discon_msg, sizeof(discon_msg)); 

    //send disconnection to all other clients-- conditional for whether nickname was created
    if(strncmp(cur_nickname, "User unknown", 12) == 0) {
        char client_msg[sizeof(cur_nickname) + 19]; 
        snprintf(client_msg, sizeof(client_msg), "%s has disconnected.", cur_nickname);
        send_all(client_msg, sizeof(client_msg)); 
    } else {
        char client_msg[sizeof(cur_nickname) + sizeof(ip) + sizeof(port) + 28]; 
        snprintf(client_msg, sizeof(client_msg), "User %s (%s:%s) has disconnected.", cur_nickname, ip, port); 
        send_all(client_msg, sizeof(client_msg));    
    }
    return NULL; 
}

void *
send_all(char *msg, int size) {
    pthread_mutex_lock(&mutex);
    if(first_client->fd == -1){ //if no clients connected, do not send 
        pthread_mutex_unlock(&mutex);
        return NULL; 
    }
    struct client *cur_client;
    for (cur_client = first_client; cur_client; cur_client = cur_client->next_client){
        if((send(cur_client->fd, msg, size, 0)) == -1) {
            perror("send");
            pthread_mutex_unlock(&mutex); 
            exit(5); 
        } 
    }
    pthread_mutex_unlock(&mutex);
    return NULL; 
}

struct client *
add_client(int fd, char *ip, uint16_t port){ 
    pthread_mutex_lock(&mutex);
    if(first_client->fd == -1) {
        first_client->fd = fd; 
        snprintf(first_client->nickname, 45, "User unknown (%s:%d)", ip, port); 
        snprintf(first_client->remote_ip, 15, "%s", ip); 
        snprintf(first_client->remote_port, 15, "%d", port); 
        pthread_mutex_unlock(&mutex);
        return first_client; 
    }
    struct client *cur_client = first_client; 
    while(cur_client->next_client != NULL) {
        cur_client = cur_client->next_client;
    }
    struct client *new_client = malloc(sizeof(struct client)); 
    new_client->fd = fd; 
    snprintf(new_client->nickname, 45, "User unknown (%s:%d)", ip, port); 
    snprintf(new_client->remote_ip, 15, "%s", ip); 
    snprintf(new_client->remote_port, 15, "%d", port); 
    new_client->next_client = NULL; 
    cur_client->next_client = new_client;  
    pthread_mutex_unlock(&mutex);
    return new_client;
}

void *
remove_client(int fd) {
    pthread_mutex_lock(&mutex);
    if(first_client->fd == fd) {
        if(first_client->next_client == NULL) {
            first_client->fd = -1; 
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        struct client *next = first_client->next_client; 
        first_client->fd = next->fd; 
        first_client->next_client = next->next_client; 
        snprintf(first_client->remote_port, sizeof(next->remote_port), "%s", next->remote_port); 
        snprintf(first_client->nickname, sizeof(next->nickname), "%s", next->nickname); 
        snprintf(first_client->remote_ip, sizeof(next->remote_ip), "%s", next->remote_ip); 
        free(next);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    struct client *cur = first_client; 
    struct client *last;  
    while(cur->fd != fd) {
        last = cur; 
        cur = cur->next_client; 
    }
    last->next_client = cur->next_client; 
    free(cur);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void *
change_nickname(struct client *client_data, char *nick_name) {
    pthread_mutex_lock(&mutex);
    snprintf(client_data->nickname, 45, "%s", nick_name); 
    pthread_mutex_unlock(&mutex);
    return NULL; 
}

void *
print_clients() { //for testing
    struct client *cur = first_client; 
    printf("\nNEW PRINT:\n");
    while(cur != NULL) {
        printf("fd: %d\n", cur->fd); 
        printf("next: %p\n", (void *) cur->next_client); 
        printf("nickname: %s\n", cur->nickname); 
        printf("ip: %s\n", cur->remote_ip); 
        printf("port: %s\n", cur->remote_port);
        cur = cur->next_client; 
    }
    return NULL; 
}

