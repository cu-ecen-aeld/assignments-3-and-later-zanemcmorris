#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <stdbool.h>

#define MAX_SOCK_CONNECTIONS (1)
#define LOG_PATH ("/var/tmp/aesdsocketdata")
#define RECV_BUFFER_LENGTH_BYTES (32)

typedef struct sockaddr sockaddr_t;
typedef struct addrinfo addrinfo_t;

// Globally available addrinfo to clean after program terminates
addrinfo_t *addrinfo = NULL;
int sockfd = -1;
int logfd = -1;
int clientfd = -1;
volatile int endProgram = 0;
bool runAsDaemon = false;

int cleanupProgram();

void handle_sigint(int sig) {
    syslog(LOG_DEBUG, "Caught signal, exiting");

    // Perform cleanup here
    endProgram = 1;
}

int cleanupProgram()
{
    if(addrinfo != NULL){
        freeaddrinfo(addrinfo);
        addrinfo = NULL;
    }

    if(sockfd != -1){
        close(sockfd);
        sockfd = -1;
    }

    if(logfd != -1){
        close(logfd);
        logfd = -1;
    }

    if(clientfd != -1){
        close(clientfd);
        clientfd = -1;
    }

    int rc;
    rc = access(LOG_PATH, F_OK);
    if(rc == 0){
        rc = remove(LOG_PATH);
        if(rc == -1){
            perror("remove failed");
        }
    }

    printf("Cleaned!\n");
}

void printClientNameConnected(struct sockaddr *clientaddr){
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    int rc;

    rc = getnameinfo((struct sockaddr *) clientaddr,
                        sizeof(*clientaddr),
                        host,
                        sizeof(host),
                        service,
                        sizeof(service),
                        NI_NUMERICHOST | NI_NUMERICSERV);

    if (rc != 0) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rc));
    } else {
        syslog(LOG_DEBUG, "Accepted connection from %s:%s\n", host, service);
    }
}

void printClientNameDisconnected(struct sockaddr *clientaddr){
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    int rc;

    rc = getnameinfo((struct sockaddr *) clientaddr,
                        sizeof(*clientaddr),
                        host,
                        sizeof(host),
                        service,
                        sizeof(service),
                        NI_NUMERICHOST | NI_NUMERICSERV);

    if (rc != 0) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(rc));
    } else {
        syslog(LOG_DEBUG, "Closed connection from %s:%s\n", host, service);

    }
}

int openSocket(const char* port){
    sockfd = socket(AF_INET, SOCK_STREAM, 0);    
    if(sockfd == -1){
        perror("no sockfd returned from socket()");
        return -1;
    }

    struct addrinfo hints = {.ai_flags = AI_PASSIVE, .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM};
    int rc;
    rc = getaddrinfo(NULL, port, &hints, &addrinfo);
    if(rc != 0){
        perror("getaddrinfo failed");
        return -1;
    }

    int val = 1;
    rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if(rc == -1){
        perror("Failed setsockopt");
        return -1;
    }

    rc = bind(sockfd, addrinfo->ai_addr, addrinfo->ai_addrlen);
    if(rc == -1){
        perror("bind failed");
        return -1;
    }

    if(runAsDaemon)
    {
        pid_t newpid = fork();
        if(newpid != 0){
            cleanupProgram();
            exit(0);
        } else {
            // Daemon setup
            if(setsid() < 0){
                return -1;
            }

            newpid = fork();
            if(newpid < 0) return -1;
            if(newpid > 0) exit(0);

            int nullfd = open("/dev/null", O_RDWR);
            if (nullfd < 0) return -1;

            // Force stdin/stdout/stderr to /dev/null
            if (dup2(nullfd, STDIN_FILENO)  < 0) return -1;
            if (dup2(nullfd, STDOUT_FILENO) < 0) return -1;
            if (dup2(nullfd, STDERR_FILENO) < 0) return -1;

            return 0;
        }
    }

}

int sendFullLog(int newfd)
{
    int fd = open(LOG_PATH, O_RDONLY);
    if (fd == -1) {
        perror("open log for read");
        return -1;
    }

    char buf[4096];
    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == 0) break;              // EOF
        if (n < 0) {
            perror("read log");
            close(newfd);
            return -1;
        }

        // send() may write fewer bytes than requested, so loop until done
        ssize_t sent = 0;
        while (sent < n) {
            ssize_t m = send(newfd, buf + sent, (size_t)(n - sent), 0);
            if (m < 0) {
                perror("send");
                close(newfd);
                return -1;
            }
            sent += m;
        }
    }

    close(fd);
    return 0;
}

int listenLoop()
{
    int rc = 0;
    struct sockaddr_storage clientaddr;
    int newfd = 0;
    logfd = open(LOG_PATH, (O_APPEND | O_CREAT | O_RDWR), 0777);
    if(logfd < 0){
        perror("Could not open logfd");
        return -1;
    }


    printf("starting to listen...\n");
    rc = listen(sockfd, MAX_SOCK_CONNECTIONS);
    if(rc == -1){
        perror("listen failed");
    }

    uint32_t bufferCapacity = RECV_BUFFER_LENGTH_BYTES;
    uint8_t *buffer = NULL;
    uint8_t *tmp;
    size_t totalBytesRecvd = 0;
    bool failedToRead = false;

    do
    {
        int sockaddrsize = sizeof(sockaddr_t);
        clientfd = accept(sockfd, (sockaddr_t*) &clientaddr, &sockaddrsize);
        if(clientfd == -1){
            if(errno == EINTR && endProgram){
                break;
            } else {
                perror("accept failed");
            }
        }

        printClientNameConnected((sockaddr_t *) &clientaddr);
        memset(buffer, bufferCapacity, 0);
        bufferCapacity = RECV_BUFFER_LENGTH_BYTES;
        buffer = (uint8_t*) malloc(bufferCapacity);
        totalBytesRecvd = 0; // Reset num bytes received for this message

        // Receive all the bytes now
        while(1)
        {
            if(totalBytesRecvd == bufferCapacity){
                bufferCapacity *= 2;
                uint8_t *temp = realloc(buffer, bufferCapacity);
                if(temp == NULL){
                    free(buffer);
                    // toss this message and go next...
                    failedToRead = true;
                    printf("Failed to malloc large enough buffer.\n");
                    break;
                }

                buffer = temp;
                printf("Doubled buffer size. Now %d bytes\n", bufferCapacity);
            }

            int n = recv(clientfd, buffer + totalBytesRecvd, bufferCapacity - totalBytesRecvd, 0);
            if(n < 0){
                // Error on recv
                free(buffer);
                buffer = NULL;
                perror("recv");
                return -1;
            }

            if(n == 0){
                break;
            }

            

            totalBytesRecvd += n;
            printf("Just read %d bytes, making total of %ld\n", n, totalBytesRecvd);
            if(buffer[totalBytesRecvd-1] == '\n'){
                break;
            }

        }

        if(!failedToRead){
            buffer[totalBytesRecvd] = 0;
            printf("new buffer: %s", buffer);
            write(logfd, buffer, totalBytesRecvd);
            if(buffer != NULL)
            {
                free(buffer);
                buffer = NULL;
            }   
            sendFullLog(clientfd);
        }

        printClientNameDisconnected((sockaddr_t *) &clientaddr);
        shutdown(clientfd, SHUT_RDWR);
        close(clientfd);
        clientfd = -1;

    }while(!endProgram);    
}

int main(int argc, char ** argv){

    if(argc == 2)
    {
        if (strcmp(argv[1], "-d") == 0){
            runAsDaemon = true;
            // printf("Starting as daemon");
        }
    }

    openlog("aesdsocket", LOG_PERROR, LOG_USER);
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    // open a socket on port 9000
    // listen for and accept a connection
    int rc = openSocket("9000");
    
    if(rc == 0)
        listenLoop();

    cleanupProgram();
    return rc;
}