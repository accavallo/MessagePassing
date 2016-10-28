//  oss.c
//  Proj3
//
//  Created by Tony on 10/12/16.
//  Copyright © 2016 Anthony Cavallo. All rights reserved.
//

#include "Proj3.h"
unsigned int *seconds;
unsigned int *nano_seconds;
unsigned int *turn;
unsigned int *flag;
int memory;
int snd_id;
int rcv_id;

void printHelp();

int main(int argc, const char * argv[]) {
    signal(SIGSEGV, faultHandler);
    signal(SIGINT, interruptHandler);
    
    int spawns = 5, duration = 20, option, status, time_increment = 1400;
    char *fileName = "logfile.txt";
    
    while((option = getopt(argc, (char**)argv, "hs:l:m:t:")) != -1) {
        switch (option) {
            case 'h':
                printHelp();
                break;
            case 's':
                if (isdigit(*optarg))
                    spawns = atoi(optarg);
                else
                    fprintf(stderr, "Expected positive integer, found %s instead\n", optarg);
                break;
            case 'l':
                fileName = optarg;
                break;
            case 'm':
                if(isdigit(*optarg) && atoi(optarg) > 0)
                    time_increment = atoi(optarg);
                else
                    fprintf(stderr, "Expected positive integer, found %s instead\n", optarg);
                break;
            case 't':
                if(isdigit(*optarg))
                    duration = atoi(optarg);
                else
                    fprintf(stderr, "Expected positive integer, found %s instead\n", optarg);
                break;
            case '?':
                if (optopt == 's' || optopt == 'l' || optopt == 't' || optopt == 'm')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf(stderr, "Unknown option '-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character '%s'.\n", optarg);
                break;
            default:
                printHelp();
                break;
        }
    }
    
    //Print out any additional errors not caught by getopt.
    int index, processCount = 0;
    for (index = optind; index < argc; index++)
        printf("Non-option argument %s.\n", argv[index]);
    /***** End of getopt and initial setup *****/
    
    //Just a confirmation of what will be printed to, how many spawns will be created and how long it will run for.
    printf("Number of spawns: %i\nDuration oss will run: %i\nName of file to write to: %s\nModulo for clock increase: %i\n", spawns, duration, fileName, time_increment);
    printf("Press enter to continue...");
    getchar();
        
    /* Do shared memory and time setup */
    memory = shmget(KEY, (sizeof(int) * 103), IPC_CREAT | 0777);
    if (memory == -1) {
        fprintf(stderr, "Oss failed to create shared memory, exiting program.\n");
        return -1;
    }

    if ((seconds = shmat(memory, NULL, 0)) == (unsigned int *)(-1)) {
        fprintf(stderr, "Oss failed to attach to memory, exiting program.\n");
        return -1;
    }
    
    nano_seconds = seconds + 1;
    turn = seconds + 2;
    flag = seconds + 3;
    
    /* Set up the message passing stuff */
    int msgflg = IPC_CREAT | 0666;
    msg_buf sndbuf, rcvbuf;
    key_t snd_key = 1632;
    key_t rcv_key = 2361;
    /* Create the message queue that will be used to send from oss. */
    if ((snd_id = msgget(snd_key, msgflg)) < 0) {
        perror("msgsnd");
        exit(1);
    }
    
    /* Create the message queue that will be used to receive */
    if ((rcv_id = msgget(rcv_key, msgflg)) < 0) {
        perror("msgrcv");
        exit(1);
    }
  
    /* Do signal set setup here */
    sigset_t sig_set;
    sigemptyset(&sig_set);
    sigaddset(&sig_set, SIGALRM);
    sigaddset(&sig_set, SIGINT);

    /* Do fork and exec calls */
    char buff[100], memSize[10], numSpawns[5];
    sprintf(memSize, "%lu", (sizeof(int) * 103));
    sprintf(numSpawns, "%i", spawns);
    pid_t pid = 0, wpid;
    index = 0;
    srand((unsigned)time(NULL));
    
    signal(SIGALRM, alarmHandler);
    alarm(duration);
    int keepGoing = 1, processes_left = 100;
    while (keepGoing) {
        //Check to ensure 2 seconds haven't passed in oss time.
        if (*seconds > 1){
            fprintf(stderr, "Allotted time has passed.\t%u.%u\n", *seconds, *nano_seconds);
            otherInterrupt();
            break;
        }
        //Check to ensure we are continually generating processes, no more than the number of spawns at one time, until we reach 100
        else if (index < spawns && processCount < 100) {
            pid = fork();
            index++;
            processCount++;
            if (pid == 0) {
                sprintf(buff, "%i", processCount);
                execl("./user", buff, memSize, numSpawns, (char *)NULL);
                processCount--;
                index--;
                _exit(EXIT_FAILURE);
            }
        } else {
            if (!(msgrcv(rcv_id, &rcvbuf, MSGSZ, 1, IPC_NOWAIT) < 0)) {
                if (processCount < 100)
                    index--;
                
                FILE *file;
                file=fopen(fileName, "a");
                if (file == NULL) {
                    printf("Failed to open file, exiting program.\n");
                    errno = ENOENT;
                    faultHandler();
                    return -1;
                }
                /* Block signal set here */
                sigprocmask(SIG_BLOCK, &sig_set, NULL);
                fprintf(file, "Child %3li is terminating at my time %i.%i because it reached %s\n", rcvbuf.procID, *seconds, *nano_seconds, rcvbuf.mtext);
                fclose(file);
                /* Unblock signal set here */
                sigprocmask(SIG_UNBLOCK, &sig_set, NULL);
                
                /* Send message back to child to terminate */
                sndbuf.mtype = rcvbuf.procID;
                sprintf(sndbuf.mtext, "Message received %ld!\n", rcvbuf.procID);
                
                if (msgsnd(snd_id, &sndbuf, MSGSZ, IPC_NOWAIT) < 0)
                    printf("Message not sent from oss :(\n");
                else 
                    processes_left--;
                if (rcvbuf.procID == 100 && processes_left == 0)
                    keepGoing = 0;
            }
        }
        *nano_seconds += rand() % time_increment;
        if (*nano_seconds >= 1000000000) {
            (*seconds)++;
            *nano_seconds = *nano_seconds % 1000000000;
        }
//        waitpid(0, NULL, WNOHANG);
    }

    /* Wait for all children to finish if exiting normally. */
    while ((wpid = wait(&status)) > 0);
    
    /* Release all shared memory */
    detachEverything();
    printf("Program exiting normally\n");
    return 0;
}

void printHelp() {
    printf("-h displays this help message\n");
    printf("-l filename sets the filename to be used for the log file\n");
    printf("-m y sets the Integer argument 'y' to the variable 'm' for the amount the \"clock\"\n   will potentially increase by every time the loop iterates.\n");
    printf("-s x sets the Integer argument 'x' to the variable 's' for the number of user\n   processes spawned. By default x is set to 5.\n");
    printf("-t z sets the Integer argument 'z' to the variable 't' for the amount of time,\n   in seconds, when oss will terminate itself. By default z is set to 20.\n");
}

void faultHandler() {
    printf("\nYOU DONE MESSED UP A-A-RON! YOU CREATED A SEGMENTATION FAULT!\n");
    pid_t pid = getpid(), wpid, groupID = getpgrp(); int status = 0;
    killpg(groupID, SIGINT);
    while ((wpid = wait(&status)) > 0); //Wait for all children to exit before continuing execution.

    detachEverything();
    fprintf(stderr, "OSS process %d dying due to a segmentation fault.\n", pid);
    exit(0);
}

void interruptHandler() {
    printf("\nAnd here, we.......go. OSS got an interrupt boss!\n");
    pid_t pid = getpid(), groupID = getpgrp(), wpid; int status = 0;
    killpg(groupID, SIGINT);
    while ((wpid = wait(&status)) > 0);	//Wait for all children to exit before continuing execution.

    detachEverything();
    fprintf(stderr, "OSS process %d dying due to an interrupt.\n", pid);
    exit(0);
}

void otherInterrupt() {
    printf("So, since we didn't want to complete everything...\n");
    pid_t groupID = getpgrp();
    killpg(groupID, SIGINT);
}

void alarmHandler() {
    printf("\nTime's up! Three bucks off!\n");
    pid_t pid = getpid(), groupID = getpgrp(), wpid; int status = 0;
    killpg(groupID, SIGALRM);
    while ((wpid = wait(&status)) > 0);

    detachEverything();
    fprintf(stderr, "Wise man say, \"Forgiveness is divine, but never wait full time for late process.\"\nOSS process %d dying due to time being up.\n", pid);
    exit(0);
}

void detachEverything() {
    shmdt(seconds);
    shmdt(nano_seconds);
    shmdt(turn);
    shmdt(flag);
    msgctl(snd_id, IPC_RMID, NULL);
    msgctl(rcv_id, IPC_RMID, NULL);
    shmctl(memory, IPC_RMID, NULL);    //Remove shared memory
}
