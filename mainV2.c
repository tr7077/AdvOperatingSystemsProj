#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#define READ 0
#define WRITE 1
#define BS 100

size_t extractName(const char* src, char* dest);
int isDataAvailable(int fd);
void self_term(int signum);
void kill_children(int signum);
void closeAllResources();

struct child{
   int pid;
   char name[BS];
   int toParent[2];
   int toChild[2];
};

int proccesses;
int fd;
sem_t *semaphore_fd;
struct child* children;

int main(int argc, char **argv){
    // input check /------------------------------------------------------------
    if(argc != 3){
        printf("[USAGE]: [FILENAME] [NUMBER OF CHILD PROCCESSES > 0]\n");
        exit(1);
    }
    
    const char* filename = argv[1];
    proccesses = atoi(argv[2]);

    if(!isalpha(filename[0]) || proccesses == 0){
        printf("[USAGE]: [FILENAME] [NUMBER OF CHILD PROCCESSES > 0]\n");
        exit(1);
    }
    //--------------------------------------------------------------------------

    const int parentPid = getpid();
    fd = open(filename, O_CREAT | O_WRONLY, 0666);
    semaphore_fd = sem_open("sem", O_CREAT, 0777, 1);
    char fileBuffer[BS]; memset(fileBuffer, '\0', sizeof(fileBuffer));
    children = (struct child*)malloc(proccesses * sizeof(struct child));

    // parent writes to file first
    int s = sprintf(fileBuffer, "[PARENT] -> <%d>\n", parentPid);
    write(fd, fileBuffer, s*sizeof(char)); memset(fileBuffer, '\0', sizeof(fileBuffer));

    // create the pipes for each child
    for(int i=0; i<proccesses; i++){
        int ok = 1;
        if(pipe(children[i].toChild) < 0){
            ok = 0;
        }
        if(pipe(children[i].toParent) < 0){
            ok = 0;
        }
        if(!ok) exit(1);
    }
    
    // if parent is running then fork and save the child pid
    for(int i=0; i<proccesses; i++){
        if(getpid() == parentPid){
            children[i].pid = fork();
            if(children[i].pid == 0) children[i].pid = getpid();
        }
    }

    // father code
    if(getpid() == parentPid){
        //singnal
        signal(SIGTSTP, (void (*)(int))kill_children);
        sleep(5);
        // close unecessary pipes
        for(int i=0; i<proccesses; i++){
            close(children[i].toChild[READ]);
            close(children[i].toParent[WRITE]);
        }

        // send message to every child
        char messageBuffer[BS]; memset(messageBuffer, '\0', sizeof(messageBuffer));
        for(int i=0; i<proccesses; i++){
            sprintf(messageBuffer, "Hello child, I am your father and I call you: <child%d>", i+1);
            write(children[i].toChild[WRITE], messageBuffer, sizeof(char) * BS);
            memset(messageBuffer, '\0', sizeof(fileBuffer));
        }
        
        // read "done"
        int answers = 0, k = 0;
        while(answers < proccesses){
            if(isDataAvailable(children[k].toParent[READ])){
                answers++;
                read(children[k].toParent[READ], messageBuffer, sizeof(messageBuffer));
                printf("<%d> -> %s\n", children[k].pid, messageBuffer);
                memset(messageBuffer, '\0', sizeof(messageBuffer));
            }
            k++;
            if(k == proccesses) k = 0;
        }

        // wait for all the children to finish
        for(int i=0; i<proccesses; i++){
            if(wait(NULL) <= 0){
                printf("SOME ERROR OCCURED\n");
            }
        }
    }
    else{   // child code
        // signal
        signal(SIGTSTP, (void (*)(int))self_term);
        // close unecessary pipes
        for(int i=0; i<proccesses; i++){
            if(getpid() == children[i].pid){
                close(children[i].toChild[WRITE]);
                close(children[i].toParent[READ]);
                break;
            }
        }

        for(int i=0; i<proccesses; i++){
            if(getpid() == children[i].pid){
                char messageBuffer[BS]; memset(messageBuffer, '\0', sizeof(messageBuffer));
                read(children[i].toChild[READ], messageBuffer, sizeof(messageBuffer));
                extractName(messageBuffer, children[i].name);
                
                // all children are going to write to same file so semaphore will prevent any (in file) interfierance
                sem_wait(semaphore_fd);
                int s = sprintf(fileBuffer, "<%d> -> <%s>\n", children[i].pid, children[i].name);
                write(fd, fileBuffer, s);
                memset(fileBuffer, '\0', sizeof(fileBuffer));
                sem_post(semaphore_fd);

                // answer "done"
                write(children[i].toParent[WRITE], "done", sizeof(char) * BS);
                
                break;
            }
        }
    }
    
    closeAllResources();

    return 0;
}

size_t extractName(const char* src, char* dest){
    
    int i=0, k=0;
    while(src[i++] != '<'){}
    while(src[i] != '>'){
        dest[k++] = src[i++];
    }
    return k;
}

int isDataAvailable(int fd){
    // dataset of fds
    fd_set read_fds;

    struct timeval timeout;

    // dataset init
    FD_ZERO(&read_fds);
    // add fd to set
    FD_SET(fd, &read_fds);

    // return immidiately with no delay
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int result = select(fd + 1, &read_fds, NULL, NULL, &timeout);

    if (result == -1) {
        perror("select");
        exit(EXIT_FAILURE);
    }

    return FD_ISSET(fd, &read_fds);
}

void self_term(int signum){
    closeAllResources();
    printf("I am a child.. Goodbye!\n");
    exit(0);
}

void kill_children(int signum){
    for(int i=0; i<proccesses; i++){
        kill(children[i].pid, SIGTERM);
    }
    closeAllResources();
    printf("Father terminating..\n");
    exit(0);
}

void closeAllResources(){
    sem_close(semaphore_fd);
	sem_unlink("sem");
    close(fd);
    for(int i=0; i<proccesses; i++){
        close(children[i].toChild[READ]);
        close(children[i].toChild[WRITE]);
        close(children[i].toParent[READ]);
        close(children[i].toParent[WRITE]);
    }
    free(children);
}
