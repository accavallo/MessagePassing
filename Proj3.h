//
//  Proj3.h
//  Proj3
//
//  Created by Tony on 10/12/16.
//  Copyright © 2016 Anthony Cavallo. All rights reserved.
//

#ifndef Proj3_h
#define Proj3_h

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <errno.h>

#define KEY 18137644
#define MSGSZ 128

void interruptHandler();
void faultHandler();
void otherInterrupt();
void alarmHandler();
void detachEverything();

enum State {idle = 0, want_in, in_cs};

typedef struct messagebuffer {
    long mtype;
    long procID;
    char mtext[MSGSZ];
} msg_buf;

#endif /* Proj3_h */
