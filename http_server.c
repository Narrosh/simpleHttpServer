//HTTP SERVER Clayton
//Started: 10-30-2024
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //syscalls
#include <arpa/inet.h> //networking
#include <sys/socket.h> //socket api stuff
#include <pthread.h> //posix threads MAKE SURE YOU -lpthread in GCC
#include <sys/stat.h> //file stuff
#include <fcntl.h> //file control
#include <stdbool.h> //boolean stuff

#define MAX_BUFFER 1024
#define MAX_CONNECTIONS 10 //setting limit on how many simultaneous connections 
                           //for thread spawning management purposes
//const string for document root
const char *documentRoot = "./www";
//forward declarations
void *client_thread_boi(void *clientSockPointer);
void receive_client_request(int client_sock);
void extract_request_info(char *request, char *method, char *url, char *version);
void handle_request(int client_sock, char *method, char *url, char *version);
void finish_request(int client_sock, const char *header, const char *file_path);

int main(int argc, char *argv[]) 
{
    if (argc != 2) //error handling for improper usage
    {
        fprintf(stderr, "Use it this way: ./server <port>\n");
        exit(-1);
    }

    int server_sock, client_sock; //forward declare sockets
    struct sockaddr_in server_addy; //struct for server addy
    struct sockaddr_in client_addy; //struct for client addy
    socklen_t client_len = sizeof(client_addy); //set client length var of socket address
    int port = atoi(argv[1]);
    //taking port from input and making it int.....

    //creating the server socket, checking for error if failure
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        fprintf(stderr, "socket creation failed");
        exit(-1);
    }

    //binding the socket to the port
    server_addy.sin_family = AF_INET; //set addyfamily to ipv4
    server_addy.sin_addr.s_addr = INADDR_ANY; //bind to any avail addy
    server_addy.sin_port = htons(port); //htons to convert usint from host byte order to net byte order
    //basically changing endianness of bytes because RFC1700....
    //fun read actually, "On Holy Wars and A Plea for Peace" the holy wars of byte ordering lol
    
    //using bind() according to PA1 instructions/man page info, bind/associate it to
    //number on local machine will return -1 on error
    //server_addy has port info inside of the struct as set earlier
    if (bind(server_sock, (struct sockaddr *)&server_addy, sizeof(server_addy)) < 0) 
    {
        fprintf(stderr, "binding failed");
        close(server_sock); // must close before exit
        exit(-1);
    }

    //begin listening for incoming connections
    //from man page: int listen(int sockfd, int backlog)
    //backlog = max amount of connections
    //returns less than zero for failure
    if (listen(server_sock, MAX_CONNECTIONS) < 0) {
        fprintf(stderr, "Listen failed");
        close(server_sock); //must close before exit
        exit(-1);
    }
    //print this to inform user
    printf("Running your server with a port # of %d\n", port);
    //infinite loop of listening
    //each connnection returns a new socketdescriptor client_sock
    while (true) 
    {
        //accept works a bit like bind()
        client_sock = accept(server_sock, (struct sockaddr *)&client_addy, &client_len);
        if (client_sock < 0) 
        {
            fprintf(stderr, "Accept failed");
            continue;
        }

        //spawn posix thread to handle the client request
        pthread_t thread;
        int *clientSockPointer = malloc(sizeof(int)); //malloc sizeof(int) because socketfds are ints
        *clientSockPointer = client_sock; //give *clientSockPointer client_sock struct
        pthread_create(&thread, NULL, client_thread_boi, clientSockPointer);
        //create the thread and throw it the client_sock info through the clientSockPointer pointer
        //also send it to the client_thread_boi function to continue it's work
        if (pthread_detach(thread) != 0) //make sure it returns zero, or else im screwed
        {
            //detach thread so it can go service the request and die
            //without needing to be joined n shizz
            fprintf(stderr, "pthread detach error!");
            exit(-1);
        }
    }
    //upon exiting the infinite loop of despair close server_sock, return 0, die
    close(server_sock);
    return 0;
}

void *client_thread_boi(void *clientSockPointer)
{
    int client_sock = *(int *)clientSockPointer; //cast the clientSockPointer to an int
    free(clientSockPointer); //free the malloc'd *clientSockPointer argument passed into this after setting client_sock
    receive_client_request(client_sock);//throw thread to receive_client_request
    close(client_sock); //after handling close client_sock and die
    return NULL;
}
//where threads go to service requests
void receive_client_request(int client_sock) 
{
    char buffer[MAX_BUFFER]; //max buffer for receive
    ssize_t bytes_received; //forwDeclare bytes_received

    //read the client's request
    if ((bytes_received = recv(client_sock, buffer, MAX_BUFFER - 1, 0)) < 0) //receive into buffer
    {
        fprintf(stderr, "receiving failed =(");
        return;
    }
    buffer[bytes_received] = '\0'; //null terminator on end of recv data

    //extract info out of the HTTP request
    char method[10], url[256], version[10]; //create strings for method req, url, and version info
    extract_request_info(buffer, method, url, version); //pass these to parse the request get the info out
    handle_request(client_sock, method, url, version); //now send parsed info to handle request to get it done
}

void extract_request_info(char *request, char *method, char *url, char *version)
{
    //get info out of request
    sscanf(request, "%s %s %s", method, url, version);
}
// Function to get the content type based on file extension
const char* get_content_type(const char* path) {
    if (strstr(path, ".html"))
        {
            return "text/html";
        }
    if (strstr(path, ".txt"))
        {
            return "text/plain";
        }
    if (strstr(path, ".png"))
        {
            return "image/png";
        }
    if (strstr(path, ".gif"))
        {
            return "image/gif";
        }
    if (strstr(path, ".jpg") || strstr(path, ".jpeg")) //I am filled with hatred at the inconsistent jpg file endings
        {//but to be sure I include both despite this
            return "image/jpg";
        }
    if (strstr(path, ".ico"))
        {
            return "image/x-icon";
        }
    if (strstr(path, ".css"))
        {
            return "text/css";
        }
    if (strstr(path, ".js"))
        {
            return "application/javascript";
        }
    //if unknown RFC 2046 seems to state that this will be returned
    //I may be misunderstanding but I believe that is what it means
    //"The "octet-stream" subtype is used to indicate that a body contains
    //arbitrary binary data." -RFC2046 4.5.1
    return "application/octet-stream";
}
void handle_request(int client_sock, char *method, char *url, char *version)
{
    //header string arr
    char header[256];
    //THIS IS COMMENTED OUT BECAUSE I COULD NOT FINISH THAT STRICT HTTP 1.0 OR 1.1 adherence TO WORK
    // if (strcmp(version, "HTTP/1.1") != 0 || strcmp(version, "HTTP/1.0") != 0)
    // {
    //     snprintf(header, sizeof(header), "%s 505 HTTP Version Not Supported\r\n", version);
    //     finish_request(client_sock, header, NULL); //finish request without path info because method not allowed
    //     return;
    // }
    //only GET is supported here
    if (strcmp(method, "GET") != 0) //strcmp returns 0 when match is found
    {
        snprintf(header, sizeof(header), "%s 405 Method Not Allowed\r\n", version);
        finish_request(client_sock, header, NULL); //finish request without path info because method not allowed
        return;
    }
    //path string
    char path[512];
    //construct path string with document root and url requested
    snprintf(path, sizeof(path), "%s%s", documentRoot, url);
    //making stat struct for the path, will use on stat() to get info
    struct stat path_stat;
    //stat returns 0 on success, make sure successful, pass it path_stat struct
    if(stat(path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode))
    {
        //S_ISDIR - This macro returns non-zero if the file is a directory. (source: https://www.gnu.org/software/libc/manual/html_node/Testing-File-Type.html)
        //if directory, append /index.html! 
        //attempt to find a /index.html or index.htm in a directory as default behavior
        strcat(path, "/index.html"); //appending /index.html to path if S_ISDIR as wanted
        if(stat(path, &path_stat) < 0) //running another stat on updated path w/ index.html... If error, try index.htm
        {
            //generate new path with index.htm instead of .html. reconcatenate documentRoot and url
            snprintf(path, sizeof(path), "%s%s/index.htm", documentRoot, url);
            if(stat(path, &path_stat) != 0) //test if exists
            {
                snprintf(header, sizeof(header), "%s 404 Not Found\r\n", version);
                finish_request(client_sock, header, NULL); //finish request without path info because 404 not found!
                return;
            }
        }
    }
    else if(stat(path, &path_stat) != 0) //make sure that it exists regardless
    {
        snprintf(header, sizeof(header), "%s 404 Not Found\r\n", version);
        finish_request(client_sock, header, NULL); //finish request without path info because 404 not found!
        return;
    }
    //otherwise proceed as normal
    const char* content_type = get_content_type(path); //get the content type for header
    snprintf(header, sizeof(header), "%s 200 OK\r\n", version);
    //in order to avoid truncation of input, I append the strlen and substract size of current header
    //this allows me to effectively append to my header string without
    //snprintf screwing me over and causing weird undefined behavior
    //originally tried to append in a more naive way
    snprintf(header + strlen(header), sizeof(header) - strlen(header), "Content-Type: %s\r\n", content_type);
    finish_request(client_sock, header, path);
}
void finish_request(int client_sock, const char *header, const char *file_path)
{
    //response arr
    char response[MAX_BUFFER];
    //open provided file for reading
    int req_file_fd = open(file_path, O_RDONLY);
    //check if open worked/file exists
    if (req_file_fd < 0)
    {
        //CONTENT LENGTH = ZERO IF FILE DOESNT WORK/EXIST
        snprintf(response, sizeof(response), "%sContent-Length: 0\r\n\r\n", header);
        //sending response out
        send(client_sock, response, strlen(response), 0);
    }
    else 
    {
        //if file exist, then it's time to handle it
        struct stat file_stat; //declare file stat buffer for fstat to do it's business
        fstat(req_file_fd, &file_stat); //call fstat with the fd and stat struct
        //use file stat struct to determine content length and then print it to header
        snprintf(response, sizeof(response), "%sContent-Length: %ld\r\n\r\n", header, file_stat.st_size);
        send(client_sock, response, strlen(response), 0);
        //send header friend to client

        ssize_t bytes_read;
        //read file in response[MAX_BUFFER] sized chunks and pipe it out to client until done
        while((bytes_read = read(req_file_fd, response, sizeof(response))) > 0) //0 will indicate end of file, then while loop is liberated
        {
            //send current chunk
            send(client_sock, response, bytes_read, 0);
        }
        //close file, finally freedom
        close(req_file_fd);
    }
}