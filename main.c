#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>

int indexOf(int v, int *array, int size);

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

    int *childPids = (int*)malloc(proccesses * sizeof(int));
    const int parentPid = getpid();
    sem_t *semaphore = sem_open("sem", O_CREAT, 0777, 0);
    const int fd = open(filename, O_CREAT | O_WRONLY, 0666);
    char fileBuffer[100]; memset(fileBuffer, '\0', sizeof(fileBuffer));
    int **fds = (int**)malloc(proccesses * sizeof(int*));
    for(int i=0; i<proccesses; i++){
        fds[i] = (int*)malloc(sizeof(int)*2);
    }

    // creating the pipes
    for(int i=0; i<proccesses; i++){
        if(pipe(fds[i]) < 0){
            printf("COUND NOT CREATE PIPES");
            exit(1);
        }
    }

    // if parent is running then fork and save the child pid
    for(int i=0; i<proccesses; i++){
        if(getpid() == parentPid){
            childPids[i] = fork();
            if(childPids[i] == 0) childPids[i] = getpid();
        }
    }

    // parent will always run first because of semaphore
    if(getpid() == parentPid){
        
        // for(int i=0; i<proccesses; i++){
        //     printf("%d\n", childPids[i]);
        // }

        printf("Parent proccess: %d\n", getpid());
        int s = sprintf(fileBuffer, "[PARENT] -> <%d>\n", parentPid);
        write(fd, fileBuffer, s*sizeof(char));
        memset(fileBuffer, '\0', sizeof(fileBuffer));

        char pipeBuffer[100]; memset(pipeBuffer, '\0', sizeof(pipeBuffer));
        for(int i=0; i<proccesses; i++){
            sprintf(pipeBuffer, "child%d", i+1);
            write(fds[i][1], pipeBuffer, sizeof(pipeBuffer));
            memset(pipeBuffer, '\0', sizeof(pipeBuffer));
            sem_post(semaphore);
        }

        // parent waits for all the child proccesses to finish
        if(getpid() == parentPid){
            for(int i=0; i<proccesses; i++){
                waitpid(childPids[i], NULL, 0);
            }
        }
        // parent reads "done"
        for(int i=0; i<proccesses; i++){
            read(fds[i][0], pipeBuffer, sizeof("done"));
            printf("%s\n", pipeBuffer);
            memset(pipeBuffer, '\0', sizeof(pipeBuffer));
        }

    }
    else{

        sem_wait(semaphore);
        sem_post(semaphore);

        char pipeBuffer[100]; memset(pipeBuffer, '\0', sizeof(pipeBuffer));
        int thisChildPipe = indexOf(getpid(), childPids, proccesses);
        
        read(fds[thisChildPipe][0], pipeBuffer, sizeof(pipeBuffer));
        printf("<%d> -> <%s>\n", childPids[thisChildPipe], pipeBuffer);
        
        int s = sprintf(fileBuffer, "<%d> -> <%s>\n", childPids[thisChildPipe], pipeBuffer);
        write(fd, fileBuffer, s*sizeof(char));
        memset(fileBuffer, '\0', sizeof(fileBuffer));
        
        write(fds[thisChildPipe][1], "done", sizeof("done"));
        
    }

    // // parent waits for all the child proccesses
    // if(getpid() == parentPid){
    //     for(int i=0; i<proccesses; i++){
    //         waitpid(childPids[i], NULL, 0);
    //     }
    // }
    

    // free memory ----------------------------------------
    close(fd);
    free(childPids);
    sem_close(semaphore);
	sem_unlink("sem");
    for(int i=0; i<proccesses; i++){
        free(fds[i]);
    }
    free(fds);

    return 0;
}

int indexOf(int v, int *array, int size){
    for(int i=0; i<size; i++){
        if(array[i] == v){
            return i;
        }
    }
    return -1;
}
