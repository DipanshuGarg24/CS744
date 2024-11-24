#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <netdb.h>
#include "civetweb.h"

// #define PORT 8085
#define CHUNK_SIZE 4096 // Define a reasonable chunk size for large files
char PORT2[5];

// its significance is that it will store the all information regarding the IP and 
//the port and the socket through which we have connected to the master 
typedef struct MasterConn{
    char hostname[50];
    int PORT;
    int sock;
}MasterConn;
// this is the master struct variable :) 
MasterConn *master;



void close_Self();
int connect_with_master();

// i need to maintain the loa as well :) 

// Function to check if a file exists
int FileExists(const char *filename) {
    return access(filename, F_OK) == 0; // Returns 0 if the file exists
}

// Function to send a file to the client in chunks
void SendFileToClient(struct mg_connection *conn, const char *filename) {
    FILE *file = fopen(filename, "rb"); // Open file in binary read mode
    if (!file) {
        // Handle file open failure
        mg_printf(conn,
                  "HTTP/1.1 500 Internal Server Error\r\n"
                  "Content-Type: text/plain\r\n"
                  "Content-Length: 29\r\n"
                  "\r\n"
                  "Failed to open requested file.");
        return;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Send HTTP response headers
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: application/octet-stream\r\n"
              "Content-Length: %ld\r\n"
              "\r\n",
              file_size);

    // Buffer for file chunks
    char buffer[CHUNK_SIZE];
    size_t bytes_read;

    // Read and send the file in chunks
    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        mg_write(conn, buffer, bytes_read);
    }

    // Close the file
    fclose(file);
}

static int begin_request_handler(struct mg_connection *conn) {
    printf("Handling default request\n");

    const struct mg_request_info *request_info = mg_get_request_info(conn);
    char content[1024]; // Response content buffer
    char *localURL = (char *)request_info->local_uri;

    if (strcmp(request_info->request_method, "GET") == 0) {
        // Extract the file name from the URL (assume "?file=" format)
        // const char *query_string = request_info->query_string;
        char filename[256] = {0};

        if (request_info->query_string) {
            // sscanf(query_string, "file=%255s", filename);
            mg_get_var(request_info->query_string,strlen(request_info->query_string),"filename",filename,sizeof(filename));
            // Check if the file exists
            if (FileExists(filename)) {
                printf("File '%s' found. Preparing to send to client.\n", filename);

                // Send the file to the client in chunks
                SendFileToClient(conn, filename);
                return 1; // Request successfully handled
            } else {
                snprintf(content, sizeof(content), "ERROR 404: File '%s' not found.", filename);
            }
        } else {
            snprintf(content, sizeof(content), "ERROR 400: Missing 'file=' parameter in the query string.");
        }
    } else {
        snprintf(content, sizeof(content), "ERROR 405: Unsupported request method.");
    }

    // Send error response
    mg_printf(conn,
              "HTTP/1.1 400 Bad Request\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: %zu\r\n"
              "\r\n"
              "%s",
              strlen(content), content);

    return 1; // Request handled
}

// the main function :) 
int main(int argc,char *argv[]){

    if(argc < 3){
        perror("Three arguments needed <master-hostname> <MASTER-PORT> <WEBPORT>");
        exit(-1);
    }
    // take the argument from the user and update the webport and the normal port on which the clinet will start 

    // assigning the memory to the master struct variable :) 
    master = (MasterConn*)malloc(sizeof(MasterConn));
    sprintf(master->hostname,argv[1]);
    sprintf(PORT2,argv[3]);
    master->PORT = atoi(argv[2]);

    connect_with_master();
    // start the thread then :) 
    // pthread_t thread1;

    // if(pthread_create(&thread1,NULL,&start,NULL) == -1){
    //     perror("Unable to Create Thread :( \n");
    //     exit(-1);
    // }

    // here start the web server with a particular port number :) 
    struct mg_context *ctx;
    struct mg_callbacks callbacks;

    // List of options. Last element must be NULL.
    const char *options[] = {"listening_ports",PORT2, NULL};
    // Prepare callbacks structure
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = begin_request_handler;

    
    // Start the web server
    ctx = mg_start(&callbacks, NULL, options);


    // stop the server , when the key is pressed :) 
    getchar();
    close(master->sock);
    mg_stop(ctx);
    return 0; // Dipanshu Garg Aggarwal :) 

}

// handle by the ctrl+c 
void close_self(){

}

// this function will connect with the master and also share the webport with the master :) 
int connect_with_master(){
   

   const char *hostname = master->hostname;
    struct addrinfo hints, *res;
    int status;
    char ipstr[INET_ADDRSTRLEN];  // For IPv4

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Resolve the hostname to an IP address
    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    // Convert the IP to a string and print it
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    inet_ntop(res->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
    printf("IP Address of %s: %s\n", hostname, ipstr);

    char *ip = ipstr;
    int port = master->PORT;

    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
    {   
        printf("Invalid address/ Address not supported\n");
        // close the program :)
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {   
        printf("Connection Failed\n");
        close(sock); 
        // closing the whole process :)
        return -1;
    }else{
        write(sock,PORT2,5);
        master->sock = sock;
        printf("Connection Successfully Established with the Master :) \n");
    }
    fflush(stdout);
    return 0;
}


