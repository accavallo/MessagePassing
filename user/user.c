//  user.c
//  user
//
//  Created by Tony on 10/12/16.
//  Copyright © 2016 Anthony Cavallo. All rights reserved.
//

#include "Proj3.h"
unsigned int *seconds;
unsigned int *nano_seconds;
unsigned int *turn;
unsigned int *flag;
int rcv_id;
int snd_id;
int memory;

int main(int argc, const char * argv[]) {
    signal(SIGSEGV, faultHandler);
    signal(SIGINT, interruptHandler);
    signal(SIGALRM, alarmHandler);
    
    /****** NOTE: ******
     argv[0] is the process id (1 through max slaves)
     argv[1] is the shared memory size in bytes
     argv[2] is the number of spawns
     *******************/
    
    const int ID = atoi(argv[0]);
    /* Set up shared memory */
    memory = shmget(KEY, atoi(argv[1]), 0777);
    if(memory < 0) {
        fprintf(stderr, "User %i failed to access shared memory; child terminating\n", ID);
        return -1;
    }

    if((seconds = shmat(memory, NULL, 0)) == (unsigned int *)(-1)) {
        fprintf(stderr, "User %i failed to attach to shared memory; child terminating\n", ID);
        return -1;
    }

    nano_seconds = seconds + 1;
    turn = seconds + 2;
    flag = seconds + 3;

    /* Set up message passing */

    msg_buf sndbuf, rcvbuf;
    key_t rcv_key = 1632;
    key_t snd_key = 2361;
    size_t buf_length;
    fflush(stdout);
    if((rcv_id = msgget(rcv_key, 0666)) < 0) {
        fprintf(stderr, "Error: User %s failed to access message receive queue. Exiting...\n", argv[0]);
        exit(1);
    }

//No longer needed, but this was the original message passed to the process from main.
/*    if (msgrcv(rcv_id, &rcvbuf, MSGSZ, ID, 0) < 0) {
        fprintf(stderr, "Error: User %s failed to receive message. Exiting...\n", argv[0]);
        exit(1);
    } else {
//        printf("%i received message \"%s\"\n", ID, rcvbuf.mtext);
    }
*/
    
    if((snd_id = msgget(snd_key, 0666)) < 0) {
        fprintf(stderr, "Error: User %s failed to access message send queue. Exiting...\n", argv[0]);
        exit(1);
    }
    
    long seed = snd_id % (ID * 3);
    srand((unsigned)seed);
    int wait_time = 1 + rand() % 100000, terminate_rcvd = 1, j = 0;
    
    do {
        do {
            flag[ID-1] = want_in;
            j = *turn;
            while (j != ID-1)
                j = (flag[j] != idle) ? *turn : (j + 1) % 100;
            
            flag[ID-1] = in_cs;
            for (j = 0; j < 100; j++)
                if ((j != ID-1) && (flag[j] == in_cs))
                    break;
            
        } while ((j < 100) || (*turn != ID-1 && flag[*turn] != idle));
        *turn = ID-1;
        /******Critical Section!******/
        /* Check the "clock" to see if enough time has passed. */
        long int currentTime = *seconds * 1000000000 + *nano_seconds;
        if ((*seconds * 1000000000 + *nano_seconds + wait_time) >= currentTime) {
            /* Send message to main to ask for permission to terminate. */
            printf("Child %3i's time has come! Asking for permission to terminate %i\n", ID, ID);
            sndbuf.mtype = 1;
            sndbuf.procID = ID;
            sprintf(sndbuf.mtext, "%i", wait_time);
            buf_length = strlen(sndbuf.mtext) + 1;
            
            if (msgsnd(snd_id, &sndbuf, MSGSZ, IPC_NOWAIT) < 0)
                printf("Message not sent from %i :(\n", ID);
            else{
//                printf("%i sent the message for termination\n", ID);
		//Keep the process here until it recieces a message back from oss to terminate.
                while (msgrcv(rcv_id, &rcvbuf, MSGSZ, ID, 0) < 0);
                terminate_rcvd = 0;
            }
        }
        /******End Critical Section!******/
        //Code to exit critical section
        j = (*turn + 1) % 100;
        while (flag[j] == idle)
            j = (j + 1) % 100;
        
        *turn = j;
        flag[ID-1] = idle;

    } while (terminate_rcvd);
    flag[ID-1] = idle;  //This should already be done, but it won't hurt to ensure this process' flag is set to idle.
    
    detachEverything();
    return 0;
}

void faultHandler() {
    detachEverything();

    pid_t pid = getpid();
    printf("\nYOU DONE MESSED UP A-A-RON! YOU CREATED A SEGMENTATION FAULT!\n");
    fprintf(stderr, "USER process %d dying due to a segmentation fault.\n", pid);
    
    exit(0);
}

void interruptHandler() {
    detachEverything();

    pid_t pid = getpid();
    printf("Uh, oh spaghetti-o's! USER %d got an interrupt boss!\n", pid);
    fprintf(stderr, "USER process %d dying from an interrupt\n", pid);
    
    exit(0);
}

void alarmHandler() {
    detachEverything();

    pid_t pid = getpid();
    printf("Time waits for no process... that means you number %d!\n", pid);
    fprintf(stderr, "USER process %d dying due to time being up.\n", pid);
    
    exit(0);
}

void detachEverything() {
    shmdt(seconds);
    shmdt(nano_seconds);
    shmdt(turn);
    shmdt(flag);
}
