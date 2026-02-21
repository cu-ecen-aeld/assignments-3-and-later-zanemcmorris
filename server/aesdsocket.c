#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <syslog.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "queue.h"
#include <pthread.h>
#include <sys/time.h>
#include <stdatomic.h>
#include <time.h>

#define MAX_SOCK_CONNECTIONS (100)
#define LOG_PATH ("/var/tmp/aesdsocketdata")
#define SOCKET_PORT ("9000")
#define RECV_BUFFER_LENGTH_BYTES (32)
#define ITIMER_PERIOD_SEC (10)

typedef struct sockaddr sockaddr_t;
typedef struct addrinfo addrinfo_t;

// Globally available addrinfo to clean after program terminates
addrinfo_t *addrinfo = NULL;
int sockfd = -1;
int logfd = -1;
// int clientfd = -1;
volatile int endProgram = 0;
bool runAsDaemon = false;
pthread_mutex_t logMutex;
timer_t intervalTimerID = 0;

// SLIST.
typedef struct slist_data_s slist_data_t;
struct slist_data_s {
    pthread_t thread;      
    _Atomic bool isThreadComplete;
    int clientfd;
    SLIST_ENTRY(slist_data_s) entries;
};

SLIST_HEAD(threadListHead, slist_data_s) head;

int cleanupProgram();

/**
 * @brief Signal Handler. Expects SIGINT and SIGTERM to end program.
 */
void handle_sigint(int sig) {
    syslog(LOG_DEBUG, "Caught signal, exiting");

    if(sig == SIGINT || sig == SIGTERM){
        endProgram = 1;
    } else {
        syslog(LOG_DEBUG, "Caught unknown signal. Exiting.");
    }

    endProgram = 1; // Set flag to cleanup and end the program
}

/**
 * @brief POSIX Interval timer callback thread. Logs timestamps to LOG_PATH
 */
static void timerCallback()
{
    // TODO: Print something to log
    char str[64] = {0};
    time_t rawTime;
    struct tm timeStruct = {0};

    time(&rawTime);
    localtime_r(&rawTime, &timeStruct);

    snprintf(str, sizeof(str), "timestamp:");
    
    int charsWritten = strftime(str+10, sizeof(str) - 10, "%a, %d %b %Y %T %z", &timeStruct);
    if(charsWritten > sizeof(str)){
        printf("Zane made bad size of arr!\n");
        return;
    }

    charsWritten += 10; // Increment by the "timestamp:" chars
    
    str[charsWritten] = '\n';
    charsWritten += 1; // Make room for newline

    pthread_mutex_lock(&logMutex);
    write(logfd, str, charsWritten);
    pthread_mutex_unlock(&logMutex);

    return;
}

/**
 * @brief Create and start interval timer for logging service
 */
int startIntervalLoggingTimer()
{
    struct sigevent sev = {0};
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timerCallback;
    sev.sigev_notify_attributes = NULL;

    int rc = timer_create(CLOCK_MONOTONIC, &sev, &intervalTimerID);
    if(rc == -1){
        perror("timer create failed");
        return -1;
    }

    struct itimerspec timerData = {0};
    timerData.it_interval.tv_sec = ITIMER_PERIOD_SEC;
    timerData.it_value.tv_sec = ITIMER_PERIOD_SEC;

    rc = timer_settime(intervalTimerID, 0, &timerData, NULL);
    if(rc == -1){
        perror("timer_settime failed");
        return -1;
    }

    printf("itimer should be set up\n");

    return 0;
}

/**
 * @brief Frees allocated memory and cleans up logfile
 */
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

/**
 * @brief Helper function to print the connecting client's address and port.
 */
void printClientNameConnected(struct sockaddr *clientaddr, size_t clientAddrSize){
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    int rc;

    rc = getnameinfo((struct sockaddr *) clientaddr,
                        clientAddrSize,
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

/**
 * @brief Helper function to show the disconnected client's address and port 
 * 
 */
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

/**
 * @brief Opens a stream-style socket on port specified
 * @return 0 on success, -1 on failure.
 */
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

/**
 * @brief Sends the full log of what was received on the socket to newfd
 * @details NOT thread-safe!! Use mutex around me for logdata!
 * @return Returns 0 on success, -1 on failure.
 */
int sendFullLog(int newfd)
{
    int fd = open(LOG_PATH, O_RDONLY);
    if (fd == -1) {
        perror("open log for read");
        return -1;
    }

    struct stat st;
    int rc = stat(LOG_PATH, &st);
    if(rc == -1){
        perror("Stat failed");
        return -1;
    }

    int fsize = st.st_size;
    printf("fsize: %d\n", fsize);

    char *buf = malloc(sizeof(char) * fsize);
    if(buf == NULL)
    {
        // Malloc failed
        syslog(LOG_DEBUG, "Malloc failed in sendFullLog");
        return -1;
    }

    while (1) {
        ssize_t n = read(fd, buf, fsize);
        if (n == 0) break;
        if (n < 0) {
            perror("read log");
            close(newfd);
            free(buf);
            return -1;
        }

        // send() may write fewer bytes than requested, so loop until done
        ssize_t sent = 0;
        while (sent < n) {
            ssize_t m = send(newfd, buf + sent, (size_t)(n - sent), 0);
            if (m < 0) {
                perror("send");
                close(newfd);
                free(buf);
                return -1;
            }
            sent += m;
        }
    }

    free(buf);
    close(fd);
    return 0;
}

/**
 * @brief Pthread recieving thread for multi-threaded server. Spawned on each new connection
 * @arg Pointer to linkedlist node containing thread-specific information  
 * @return None
 */
void* repsondingThread(void* arg)
{
    slist_data_t *myNodeData = (slist_data_t*) (arg);
    int clientFD = myNodeData->clientfd;

    // Set up to recv from client
    size_t bufferCapacity = RECV_BUFFER_LENGTH_BYTES;
    uint8_t *buffer = (uint8_t*) malloc(bufferCapacity);
    memset(buffer, 0, bufferCapacity);
    int totalBytesRecvd = 0; // Reset num bytes received for this message
    bool failedToRead = false;

    // Receive all the bytes now
    while(1)
    {
        // If received bytes on last ittr was at buffer capacity, then double capacity and keep going
        if(totalBytesRecvd == bufferCapacity){
            bufferCapacity *= 2;
            uint8_t *temp = realloc(buffer, bufferCapacity);
            if(temp == NULL){
                free(buffer);
                // toss this message and go next if malloc failed 
                failedToRead = true;
                printf("Failed to malloc large enough buffer.\n");
                break;
            }

            buffer = temp;
            printf("Doubled buffer size. Now %ld bytes\n", bufferCapacity);
        }

        // Recieve n bytes from the client
        int n = recv(clientFD, buffer + totalBytesRecvd, bufferCapacity - totalBytesRecvd, 0);
        if(n < 0){
            // Error on recv
            free(buffer);
            buffer = NULL;
            perror("recv failed");
            return NULL;
        }

        if(n == 0){
            // We read all the data out of the socket
            break;
        }

        totalBytesRecvd += n;
        printf("Just read %d bytes, making total of %d\n", n, totalBytesRecvd);

        // Only stop loop if the final char is a newline
        if(buffer[totalBytesRecvd-1] == '\n'){
            break;
        }
    }

    // Trap for failed to read to hit the cleanup and return step at the end of function
    if(!failedToRead){
        // If we read completely, write message to the log, free the buffer and echo back log
        buffer[totalBytesRecvd] = 0;
        printf("new buffer: %s", buffer);

        pthread_mutex_lock(&logMutex);
        write(logfd, buffer, totalBytesRecvd); // Protext the log write
        pthread_mutex_unlock(&logMutex);

        if(buffer != NULL)
        {
            free(buffer);
            buffer = NULL;
        }   

        pthread_mutex_lock(&logMutex);
        sendFullLog(clientFD); // Protect the log read
        pthread_mutex_unlock(&logMutex);
    }

    shutdown(clientFD, SHUT_RDWR);
    close(clientFD);
    clientFD = -1;
    atomic_store(&myNodeData->isThreadComplete, true);
    printf("Thread cleaned up!\n");

    return NULL;

}

/**
 * @brief Main loop that starts listening on the opened socket and dispatches new threads upon connection
 *          Only returns via upon receiving SIGINT or SIGTERM.
 * @return Returns 1 on successful cleanup, -1 on failure. Does not return without catching a signal to do so.
 */
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

    int clientAddrSize = sizeof(clientaddr);
    SLIST_INIT(&head); // Create LL for threadss

    do
    {
        // accept is a blocking call. Execution will wait here for a connection
        printf("Main thread accepting new connections...\n");
        int clientfd = accept(sockfd, (sockaddr_t*) &clientaddr, &clientAddrSize);
        if(clientfd == -1){
            if(errno == EINTR && endProgram){
                break;
            } else {
                perror("accept failed");
            }
        }
        printClientNameConnected((sockaddr_t *) &clientaddr, clientAddrSize);

        // Create new LinkedList node for the new pthread and give it the clientfd to act on
        slist_data_t *newNode = NULL;
        newNode = malloc(sizeof(slist_data_t));
        memset(newNode, 0, sizeof(*newNode));
        newNode->clientfd = clientfd;
        SLIST_INSERT_HEAD(&head, newNode, entries);

        // Create thread to respond to server request
        pthread_create(&newNode->thread, NULL, repsondingThread, newNode);
        if(newNode == NULL){
            printf("Failed to make newNode in listenloop\n");
            return -1;
        }

        // Look to clean up any threads that are complete
        slist_data_t* dataptr = NULL, *tmp = NULL;
        SLIST_FOREACH_SAFE(dataptr, &head, entries, tmp){
            if(atomic_load(&dataptr->isThreadComplete)){
                SLIST_REMOVE(&head, dataptr, slist_data_s, entries);
                pthread_join(dataptr->thread, NULL);
                free(dataptr);

            }
        }

    }while(!endProgram);

    // Program is ending. Clean up whole LL
    slist_data_t* dataptr = NULL, *tmp = NULL;
    SLIST_FOREACH_SAFE(dataptr, &head, entries, tmp){
        SLIST_REMOVE(&head, dataptr, slist_data_s, entries);
        pthread_join(dataptr->thread, NULL);
        free(dataptr);
    }

}

/**
 * @brief entry point for program
 */
int main(int argc, char ** argv){

    if(argc == 2)
    {
        if (strcmp(argv[1], "-d") == 0){
            runAsDaemon = true;
        }
    } else if(argc != 1){
        printf("Bad args to aesdsocket\n");
        return -1;
    }

    openlog("aesdsocket", LOG_PERROR, LOG_USER);
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // open a socket on port 9000
    int rc = openSocket(SOCKET_PORT);

    // Now init mutex to procet the log data
    rc = pthread_mutex_init(&logMutex, NULL);
    if(rc == -1){
        perror("mutex create failed");
    }

    
    startIntervalLoggingTimer();

    // Listen for and accept new connection
    if(rc == 0)
        rc = listenLoop();

    cleanupProgram();
    return rc;
}