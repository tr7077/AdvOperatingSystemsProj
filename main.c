#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <ctype.h>

int main(int argc, char **argv){

    // input check /------------------------------------------------------------
    if(argc != 3){
        printf("[USAGE]: [FILENAME] [NUMBER OF CHILD PROCCESSES > 0]\n");
        exit(1);
    }

    const char* filename = argv[1];
    const int proccesses = atoi(argv[2]);

    if(!isalpha(filename[0]) || proccesses == 0){
        printf("[USAGE]: [FILENAME] [NUMBER OF CHILD PROCCESSES > 0]\n");
        exit(1);
    }
    //--------------------------------------------------------------------------

    int *childPids = malloc(proccesses * sizeof(int));
    const int parentPid = getpid();
    sem_t *semaphore = sem_open("sem", O_CREAT, 0777, 0);
    const int fd = open(filename, O_CREAT | O_WRONLY, 0666);
    char buffer[50];
    
    // if parent is running then fork and save the child pid
    for(int i=0; i<proccesses; i++){
        if(getpid() == parentPid){
            childPids[i] = fork();
        }
    }

    // parent will always run first
    if(getpid() == parentPid){
        printf("ParentProccess: %d, parent: %d\n", getpid(), getppid());
        int size = sprintf(buffer, "[PARENT] -> <%d>\n", parentPid);
        write(fd, buffer, size*sizeof(char));
        sem_post(semaphore);
    }
    else{
        sem_wait(semaphore);
        printf("ChildProccess: %d, parent: %d\n", getpid(), getppid());
        int size = sprintf(buffer, "[CHILD] -> <%d>\n", getpid());
        write(fd, buffer, size*sizeof(char));
        sem_post(semaphore);
    }

    // parent waits for all the children proccesses
    if(getpid() == parentPid){
        for(int i=0; i<proccesses; i++){
            waitpid(childPids[i], NULL, 0);
        }
    }
    
    close(fd);
    free(childPids);
    sem_close(semaphore);
    sem_unlink("sem");

    return 0;
}
