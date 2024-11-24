// Project : Master - Slave Web server :) 
// developer : Dipanshu Garg :)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <math.h>
#include <sys/epoll.h>
#include "civetweb.h"

#define BACKLOG 50
#define PORT1 8085 // this is for the socket address :) 
#define PORT2 "8080" // this for the web server
#define FUNC_COUNT 4
#define MAX_SLAVE 50
#define MAX_EVENTS 10

char URLS[][20] = {"/file" , "/hello" , "/prime" , "/factorial"};

// make the structure variable 
struct sockaddr_in addr;
int serverfd;

typedef struct Slave{
    int fd;
    int load;
    char *ip;
    int PORT;
    char WEBPORT[5];
}Slave;

Slave slaves[100];

int slavecount = 0;

// maintaing the CACHE In memory

// -----------------------------

// making a trie to store the link and get the number :) simply to understand what to do or not :)

/*Declaration of the Search Tree :) */
typedef struct TrieNode {
    struct TrieNode *children[27];
    int fun_num;
} TrieNode;

TrieNode *trie;
TrieNode *create_node(void);
void insert(const char *word,int fun_number);
int search(const char *word);
void freeTrie();
/*Declaration of the function which uses the Trie to register links :) */
void intializeTrie();
// --------------------------------------------
// mostly completed :) 
// funtions will manage the slaves :) 
int chooseSlave(char *,char *);
void sendCommand();
void start_server();
void *ConnectSlaves(void *);
// -----------------
int GetFunction(const char*);
int GetRequestParameter(struct mg_connection *conn);
// -----------------

// Cache structure
typedef struct CacheEntry {
    char key[100];  // Key (request identifier, e.g., "prime_7")
    char value[1024]; // Cached response
    struct CacheEntry *next;
} CacheEntry;

CacheEntry *cacheHead = NULL;

// Cache functions
void AddToCache(const char *key, const char *value) {
    CacheEntry *newEntry = (CacheEntry *)malloc(sizeof(CacheEntry));
    strcpy(newEntry->key, key);
    strcpy(newEntry->value, value);
    newEntry->next = cacheHead;
    cacheHead = newEntry;
}

const char *GetFromCache(const char *key) {
    CacheEntry *current = cacheHead;
    while (current) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return NULL;  // Not found in cache
}

void DeleteFromCache(const char *key) {
    CacheEntry *current = cacheHead, *prev = NULL;
    while (current) {
        if (strcmp(current->key, key) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                cacheHead = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

void ClearCache() {
    CacheEntry *current = cacheHead;
    while (current) {
        CacheEntry *temp = current;
        current = current->next;
        free(temp);
    }
    cacheHead = NULL;
}

// Helper functions
int CalculateFibonacci(int n) {
    if (n <= 1) return n;
    return CalculateFibonacci(n - 1) + CalculateFibonacci(n - 2);
}

bool IsPrime(int n) {
    if (n < 2) return false;
    for (int i = 2; i <= sqrt(n); i++) {
        if (n % i == 0) return false;
    }
    return true;
}

unsigned long long CalculateFactorial(int n) {
    if (n == 0) return 1;
    return n * CalculateFactorial(n - 1);
}

// Main handler function
static int begin_request_handler(struct mg_connection *conn) {
    printf("Handling default request\n");

    const struct mg_request_info *request_info = mg_get_request_info(conn);
    char content[1024];  // Response content
    char *localURL = (char *)request_info->local_uri;

    if (strcmp(request_info->request_method, "GET") == 0) {
        int funNum = GetFunction(localURL);
        if (funNum == -1) {
            snprintf(content, sizeof(content), "ERROR 404, Bad URL request :(");
        } else {
            if (funNum == 1) {
                 // send the redirection request :) 
                 char slaveip[100]; 
                 char webport[5];
                 int x = chooseSlave(slaveip,webport);
                 // get the information about the slave 
                 if(x == -1){
                    sprintf(content,"Server is busy Try later");
                    // send the reply that request later :) 
                    mg_printf(conn,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: %zu\r\n"
                    "\r\n"
                    "%s",
                    strlen(content), content);
                 }
                 else{
                    printf("%s\n%s\n",request_info->uri,request_info->query_string);
                    // sent the response back to the client regarding the redirection 
                    mg_printf(conn,
                        "HTTP/1.1 302 Found\r\n"
                        "Location: http://%s:%s%s?%s\r\n"
                        "Content-Length: 0\r\n"
                        "Connection: close\r\n\r\n",
                        slaveip,webport, request_info->uri , request_info->query_string
                    );
                 }
            } else{ 
                if (funNum == 2) {
                    // Hello World
                    snprintf(content, sizeof(content), "Hello, World!");
                } else if (funNum == 3) {
                    // Prime number check
                    int n = GetRequestParameter(conn);
                    char cacheKey[100];
                    snprintf(cacheKey, sizeof(cacheKey), "prime_%d", n);

                    const char *cachedResponse = GetFromCache(cacheKey);
                    if (cachedResponse) {
                        snprintf(content, sizeof(content), "Cached Result: %s", cachedResponse);
                    } else {
                        snprintf(content, sizeof(content), "%d is %s", n, IsPrime(n) ? "prime" : "not prime");
                        AddToCache(cacheKey, content);
                    }
                } else if (funNum == 4) {
                    // Factorial calculation
                    int n = GetRequestParameter(conn);
                    char cacheKey[100];
                    snprintf(cacheKey, sizeof(cacheKey), "factorial_%d", n);

                    const char *cachedResponse = GetFromCache(cacheKey);
                    if (cachedResponse) {
                        snprintf(content, sizeof(content), "Cached Result: %s", cachedResponse);
                    } else {
                        snprintf(content, sizeof(content), "Factorial(%d) = %llu", n, CalculateFactorial(n));
                        AddToCache(cacheKey, content);
                    }
                }
                 // Respond to the client
              mg_printf(conn,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %zu\r\n"
                "\r\n"
                "%s",
                strlen(content), content);
            }
        }
    }

   

    return 1;
}

// get the function number
int GetFunction(const char *url) {
    // searh in the trie :) 
    int x = search(url);
    return x;  // Invalid URL
}

// function for extracting the parameters from the URL
int GetRequestParameter(struct mg_connection *conn) {
    if (conn == NULL) {
        return -1; // Invalid input
    }

    // Get the request information
    const struct mg_request_info *request_info = mg_get_request_info(conn);
    if (request_info == NULL || request_info->query_string == NULL) {
        return -1; // Unable to retrieve request info or no query string
    }

    // Buffer to hold the "num" parameter value
    char param_value[64]; // Adjust size based on expected input

    // Extract the "num" parameter from the query string
    int ret = mg_get_var(request_info->query_string, strlen(request_info->query_string),
                         "num", param_value, sizeof(param_value));

    if (ret <= 0) {
        return -1; // "num" parameter not found or buffer too small
    }

    // Convert the "num" parameter value to an integer
    return atoi(param_value); // Return the parsed integer value
}


int main(void) {
    
    // first start the server :)
    start_server();
    // and then make a thread which will accept the connections and manage them :) 
    pthread_t conn_t;

    if(pthread_create(&conn_t,NULL,&ConnectSlaves,NULL)<0){
        perror("Unable to create the thread :(\n");
        close(serverfd);
        exit(-1);
    }
    if(pthread_detach(conn_t)<0){
        perror("Unable to detach the Thread there is some issue :) \n");
        exit(-1);
    }
    intializeTrie();
    // detach the thread :)
    struct mg_context *ctx;
    struct mg_callbacks callbacks;

    // List of options. Last element must be NULL.
    const char *options[] = {"listening_ports",PORT2, NULL};
    // Prepare callbacks structure
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = begin_request_handler;

    
    // Start the web server
    ctx = mg_start(&callbacks, NULL, options);

    // Wait until user hits "enter". Server is running in a separate thread.
    getchar();
    // Stop the server
    close(serverfd);
    mg_stop(ctx);

    return 0;
}

void start_server(){
    /*Thsi function will start the server by starting the server and by binding the hostname*/    
    // open the socket
    serverfd = socket(AF_INET,SOCK_STREAM,0);
    if(serverfd == -1){
        perror("Unable to Open the Server Side FD :(\n");
        exit(-1);
    }
    // bind the adress of the server :) 
    
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(PORT1);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(serverfd,(struct sockaddr*)&addr,(socklen_t)sizeof(addr)) == -1){
        perror("There is an Error while binding the address of the Server :(\n");
        close(serverfd);
        exit(-1);
    }
    // make the server fd as the listening port :) 
    if(listen(serverfd,BACKLOG)<0){
        perror("Server fails to make the fd active :( \n");
        close(serverfd);
        exit(-1);
    }
    // server is started to connect with the SLAVE computers 
    printf("The Server is started to connect with the Slave Servers :) \port : 8085 \n");
}

pthread_mutex_t slave_lock = PTHREAD_MUTEX_INITIALIZER; // Mutex lock for thread safety

void *ConnectSlaves(void *arg) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Failed to create epoll instance");
        exit(EXIT_FAILURE);
    }

    // Add the server socket (for accepting new connections) to epoll
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN; // Monitor incoming connections on server socket
    event.data.fd = serverfd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverfd, &event) == -1) {
        perror("Failed to add server socket to epoll");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Wait for events
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events < 0) {
            perror("Epoll wait error");
            continue;
        }

        for (int i = 0; i < num_events; i++) {
            int fd = events[i].data.fd;

            if (fd == serverfd) {
                // New connection to accept
                int new_fd = accept(serverfd, (struct sockaddr *)&addr, &addrlen);
                if (new_fd < 0) {
                    perror("Failed to accept new connection");
                    continue;
                }

                // Read slave's web port and add to epoll
                pthread_mutex_lock(&slave_lock);

                char *slave_ip = inet_ntoa(addr.sin_addr);
                int slave_port = ntohs(addr.sin_port);
                printf("New Slave Connected! IP: %s, PORT: %d\n", slave_ip, slave_port);

                slaves[slavecount].fd = new_fd;
                slaves[slavecount].ip = strdup(slave_ip);
                slaves[slavecount].PORT = slave_port;
                slaves[slavecount].load = 0;

                // Read the web port from the slave
                int x = read(new_fd, slaves[slavecount].WEBPORT, sizeof(slaves[slavecount].WEBPORT) - 1);
                if (x < 0) {
                    perror("Failed to read slave's web port");
                    close(new_fd);
                } else {
                    slaves[slavecount].WEBPORT[x] = '\0';
                    printf("Slave WEBPORT: %s\n", slaves[slavecount].WEBPORT);

                    // Add the slave's file descriptor to epoll
                    event.events = EPOLLIN | EPOLLRDHUP;
                    event.data.fd = new_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &event) == -1) {
                        perror("Failed to add slave to epoll");
                        close(new_fd);
                    } else {
                        slavecount++;
                        printf("Total slaves connected: %d\n", slavecount);
                    }
                }

                pthread_mutex_unlock(&slave_lock);

            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
                // Handle slave disconnection
                pthread_mutex_lock(&slave_lock);

                printf("Slave disconnected: FD %d\n", fd);
                for (int j = 0; j < slavecount; j++) {
                    if (slaves[j].fd == fd) {
                        close(slaves[j].fd);
                        free(slaves[j].ip); // Free allocated memory for IP
                        for (int k = j; k < slavecount - 1; k++) {
                            slaves[k] = slaves[k + 1]; // Shift remaining slaves
                        }
                        slavecount--;
                        break;
                    }
                }
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL); // Remove from epoll
                printf("Updated slaves count: %d\n", slavecount);

                pthread_mutex_unlock(&slave_lock);

            } 
        }
    }
}
/*Implementation of the Trie :) */
TrieNode *create_node(void) {
    TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
    node->fun_num = -1;
    for (int i = 0; i<27; i++) {
        node->children[i] = NULL;
    }
    return node;
}

void insert(const char *word,int fun_number) {
    TrieNode *current = trie;
    while (*word) {
        int index = (*word == '/')?26:*word-'a';
        if (current->children[index] == NULL) {
            current->children[index] = create_node();
        }
        current = current->children[index];
        word++;
    }
    current->fun_num = fun_number;
}
// searching the url :) and getting the function number :)
int search(const char *word) {
    TrieNode *current = trie;
    while (*word) {
        int index = (*word == '/')?26:*word-'a';
        if (current->children[index] == NULL) {
            return -1;
        }
        current = current->children[index];
        word++;
    }
    return current->fun_num;
}
// free the memory of the Trie Gracefully :)
void freeTrie(TrieNode *node){
    for(int i=0;i<27;i++){
        if(node->children[i] != NULL){
            freeTrie(node->children[i]);
        }
    }
    free(node);
}

// intializing the Trie with all the URLs :)
void intializeTrie(){
    trie = create_node();
    for(int i=0;i<FUNC_COUNT;i++){
        insert(URLS[i],i+1);
    }
    // need to create this :) 
}

// --------------------------------------------------

int chooseSlave(char *ip, char *port) {
    static int currentSlaveIndex = 0;

    pthread_mutex_lock(&slave_lock); // Lock before accessing `slaves`

    if (slavecount == 0) {
        pthread_mutex_unlock(&slave_lock);
        printf("No slaves connected. Cannot process the request.\n");
        return -1; // Return -1 if no slaves are available
    }

    int selectedIndex = currentSlaveIndex % slavecount;
    currentSlaveIndex = (currentSlaveIndex + 1) % slavecount;

    // Copy the selected slave's IP and WebPort to the arguments
    sprintf(ip, "%s", slaves[selectedIndex].ip);
    sprintf(port, "%s", slaves[selectedIndex].WEBPORT);

    pthread_mutex_unlock(&slave_lock); // Unlock after accessing `slaves`

    return 1; // Return success
}

