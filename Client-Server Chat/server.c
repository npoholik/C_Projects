// Usage: ./server <port_number>

// This server should listen on the specified port, waiting for incoming connections.
// After a client connects, add it to a list of currently connected clients.
// If any message comes in from *any* connected client, then it is repeated to *all*
//    other connected clients.
// If reading or writing to a client's socket fails, then that client should be removed
//     from the linked list. 

// Remember that blocking read calls will cause your server to stall. Instead, set your
// your sockets to be non-blocking. Then, your reads will never block, but instead return
// an error code indicating there was nothing to read- this error code can be either
// EAGAIN or EWOULDBLOCK, so make sure to check for both. If your read call fails
// with that error, then ignore it. If it fails with any other error, then treat that
// client as though they have disconnected.

// You can create non-blocking sockets by passing the SOCK_NONBLOCK argument to both
// the socket() function, as well as the accept4() function.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <asm-generic/socket.h>
#include <ctype.h>


#define BUFFER_SIZE 1024
#define IP_Add "127.0.0.1"
#define USAGE "./server <Port Number>"

// Act as a linked list 
typedef struct client {
    int sockFD;
    char name[25];
    struct client *next;
} client;

int setNonBlocking(int sock);
void addClient(client **head, int sock);
void removeClient(client **head, int sock);
void processMsgs(client *clients, client *sender, char *msg);
void sendMsgs(client *head, int sock, char* msg, int isAllClients);


int main(int argc, char* argv[]) {

    if(argc != 2) {
        printf("Error running program. Please refer to proper usage:\n%s\n", USAGE);
        exit(-1);
    }

    int port = atoi(argv[1]);

    // Create socket endpoint
    int listenFD = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(listenFD == -1) {
        perror("Could not create socket.");
        exit(-1);
    }

    // Bind address to socket 
    struct sockaddr_in address;
    memset(&address,0, sizeof(struct sockaddr_in));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    int ret = bind(listenFD, (struct sockaddr*)&address, sizeof(struct sockaddr_in) );
    if (ret == -1) {
        perror("Could not bind the address.");
        exit(-1);
    }

    // Set socket to listening mode 
    ret = listen(listenFD, 20);
    if(ret == -1) {
        perror("Could not set socket to listening mode.");
    }

    // Create client list and buffer for incoming messages
    client *client_list = NULL;
    char buffer[BUFFER_SIZE];

    while(1) {
        
      // Look for new connections
      struct sockaddr_in client_add;
      socklen_t client_len = sizeof(client_add);
      int clientFD = accept(listenFD, (struct sockaddr *)&client_add, &client_len );

      if(clientFD >= 0) {
            ret = setNonBlocking(clientFD);
            if(ret == -1) {
                  printf("Failed setting flags for client. Aborting conenction.\n");
            }
            else {
                  addClient(&client_list, clientFD);
                  printf("New Connection Established. IP: %s; Port: %d\n", inet_ntoa(client_add.sin_addr), ntohs(client_add.sin_port));
            }
      }

      client *currClient = client_list;
      while(currClient != NULL) {
            memset(buffer, 0, BUFFER_SIZE); // Clear the message buffer after reading from each socket
            // Read from socket
            ret = recv(currClient->sockFD, buffer, BUFFER_SIZE, 0); // Success: returns # of bytes, 0 if no messages available; -1 for errno
            buffer[strcspn(buffer, "\n")] = 0;     // Strip newline

            if(ret > 0) {
                  // For message received
                  printf("Server received message: %s\n", buffer);
                  if((strncmp(buffer, "quit", 4) == 0) && strlen(buffer) == 4) {
                        removeClient(&client_list, currClient->sockFD);
                  }
                  else {
                        processMsgs(client_list, currClient, buffer);
                  }
            }
            // If no data is read, but the non-blocking socket would not block regardless, then this indicates a client has disconnected
            else if(read == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                  printf("Client disconnected. Client FD: %d\n", currClient->sockFD);
                  removeClient(&client_list, currClient->sockFD);
            }
            currClient = currClient->next; // Move onto the next client 
      }
      sleep(1);
    }
    return 0;
}


int setNonBlocking(int sock) {
      // Obtain socket flags
      int flags = fcntl(sock, F_GETFL, 0);
      if(flags == -1) {
            return -1;
      }
      flags |= O_NONBLOCK; // Use bitwise OR to set flag for O_NONBLOCK 
      return fcntl(sock, F_SETFL, flags); // Set the new flag in the socket and return the result to the main program
}

void addClient(client **head, int sock) {
      client *newClient = malloc(sizeof(client)); // Allocate memory for the new client struct 
      if(!newClient) {
            printf("Error with memory allocation of new client.\n");
            return;      
      }
      // Initialize the sockFD value and the default username of the client
      newClient->sockFD = sock;
      sprintf(newClient->name, "Unknown User");
      
      // Update the linked list with the new client as the head
      newClient->next = *head;
      *head = newClient;
}


void removeClient(client **head, int sock) {
      client *currClient = *head;
      client *prevClient = NULL;
      // Enter a loop to find the matching sockFD, remove it, and set the previous client to the next of the removed client 
      while(currClient) {
            // Look for matching socket through client list 
            if(currClient->sockFD == sock) {
                  char disconnectMsg[200];
                  snprintf(disconnectMsg, sizeof(disconnectMsg), "User %s has disconnected.", currClient->name);
                  printf("Broadcasting disconnect message: %s\n", disconnectMsg);
                  sendMsgs(*head, currClient->sockFD, disconnectMsg, 0);

                  // If not head node
                  // BE VERY CAREFUL ABOUT WHETHER SOMETHING IS NULL OR NOT 
                  if(prevClient) {
                        if(currClient->next) {
                              prevClient->next = currClient ->next;
                        }
                        else {
                              prevClient->next = NULL;
                        }
                  }
                  // If head node
                  // BE VERY CAREFUL ABOUT WHETHER SOMETHING IS NULL OR NOT 
                  else {
                        if(currClient->next) {
                              printf("Setting new head node...\n");
                              (*head) = currClient->next;
                        }
                        else {
                              printf("Setting new NULL head node...\n");
                              (*head) = NULL;
                        }
                  }
                  // Close socket and free memory allocated to client 
                  printf("Server freeing up memory...\n");
                  close(currClient->sockFD);
                  free(currClient);
                  // Make sure your not freeing a NULL head (took awhile to catch this one)
                  return;
            }
            // Moving onto next client 
            prevClient = currClient;
            currClient = currClient->next;
      }
}


void processMsgs(client *clients, client *sender, char *msg) {
      // We will check for built in commands (name)
      // This is expecting format "name<space><desired name>"
      if((strncmp(msg, "name", 4) == 0) && strlen(msg) > 5) {
            char *updatedName = msg + 5; // Want to skip to the sixth letter (start of new username)

            if(*updatedName) {
                  // Save the terminated zero old name to display the change
                  char prevName[25];
                  strncpy(prevName, sender->name, BUFFER_SIZE);
                  prevName[25 - 1] = '\0'; 

                  // Update the client to the new null terminated name
                  strncpy(sender->name, updatedName, sizeof(sender->name) - 1);
                  sender->name[sizeof(sender->name) - 1] = '\0';

                  char announcement[150]; // Allocate enough space for two full size usernames and extra information text
                  snprintf(announcement, sizeof(announcement), "User has changed their name from: %s to %s", prevName, sender->name);
                  sendMsgs(clients, sender->sockFD, announcement, 1);
                  
                  // Allow the server to display the information itself
                  printf("Name change successful. Client FD %d has changed their name from %s to %s\n", sender->sockFD, prevName, sender->name);
            }
      }
      else if (*msg) {
            char userChat[BUFFER_SIZE + 25];
            snprintf(userChat, sizeof(userChat), "%s: %s", sender->name, msg);
            sendMsgs(clients, sender->sockFD, userChat, 0);

            printf("Message broadcast successful: %s\n", userChat);
      }
}


void sendMsgs(client *head, int sock, char* msg, int isAllClients) {
      while(head != NULL) {
            if(head->sockFD != sock || isAllClients) {
                  int ret = send(head->sockFD, msg, strlen(msg), 0);
                  if(ret == -1) {
                        perror("Could not send message to client sockets.");
                  }
            }
            head = head->next;
      }
}