// Usage: ./client <IP_address> <port_number>

// The client attempts to connect to a server at the specified IP address and port number.
// The client should simultaneously do two things:
//     1. Try to read from the socket, and if anything appears, print it to the local standard output.
//     2. Try to read from standard input, and if anything appears, print it to the socket. 

// Remember that you can use a pthread to accomplish both of these things simultaneously.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define BUFFER_SIZE 1024 // for storing incoming/outgoing messages
#define USAGE "./client <IP Address> <Port Number>"

void *receiveMsg(void *arg);
void *sendMsg(void *arg);

// This is simply in order to pass the socket information to each thread function
typedef struct {
    int sockFD;
} t_data;


int main(int argc, char *argv[]) {

    if(argc != 3) {
        printf("Error running program. Please refer to proper usage:\n%s\n", USAGE);
        exit(-1);
    }

    // Pull in user connection information
    const char *ip_add = argv[1];
    int port = atoi(argv[2]);

    // Create socket endpoint 
    int sockFD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFD == -1) {
        perror("Could not create socket");
        exit(-1);
    }
    t_data data = {sockFD};

    // Connect to server 
    struct sockaddr_in address;
    memset(&address, 0, sizeof(struct sockaddr_in));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip_add, &address.sin_addr);

    int ret = connect(sockFD, (struct sockaddr*)&address, sizeof(struct sockaddr_in) );
    if(ret == -1) {
        perror("Could not successfully connect");
        exit(-1);
    }
    printf("Connected to IP: %s, Port: %d\n", ip_add, port);

    // Initialize two threads: one to focus on sending and one to focus on reading 
    // This ensures that the main flow of the program is never blocked by one or the other
    pthread_t receiver_t, sender_t;
    
    pthread_create(&receiver_t, NULL, receiveMsg, &data);
    pthread_create(&sender_t, NULL, sendMsg, &data);

    pthread_join(receiver_t, NULL);
    pthread_join(sender_t, NULL);

    close(sockFD);

    return 0;
}

void *receiveMsg(void *arg) {
    char buffer[BUFFER_SIZE];
    t_data *data = (t_data*)arg;

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        // Try to read from the socket first 
        // This thread will block if nothing has been written yet 
        int ret = recv(data->sockFD, buffer, BUFFER_SIZE - 1, 0);

        if(ret > 0) {
            printf("%s\n", buffer);
        }
        else if (ret == 0) {
            printf("Lost Connection with the Server.\n");
            exit(0);
        }
    }

}

void *sendMsg(void *arg) {
    t_data *data = (t_data*)arg;
    char buffer[BUFFER_SIZE];

    while(1) {
        // Wait to receive user input
        if(fgets(buffer, BUFFER_SIZE, stdin)) {
            buffer[strcspn(buffer, "\n")] = 0; // Replace newline with terminating zero 
            
            send(data->sockFD, buffer, strlen(buffer), 0);

            if((strncmp(buffer, "quit", 4) == 0) && (strlen(buffer) == 4)) { // Make sure the message only contains "quit"
                printf("Disconnecting.\n");
                close(data->sockFD);
                exit(0);
            }
        }
    }

}