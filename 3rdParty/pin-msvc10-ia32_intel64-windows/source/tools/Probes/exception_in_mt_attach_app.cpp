/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2011 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#include <assert.h>
#include <stdio.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <syscall.h>
#include <sched.h>


using namespace std;

bool allThreadsCanceled = false;
void CancelAllThreads();
void BlockSignal(int sigNo);
void UnblockSignal(int sigNo);

/* 
 * The total number of threads that should run in this process
 * The number may be changed in command line with -th_num
 */
unsigned int numOfSecondaryThreads = 4;


/*
 * Get thread Id
 */
pid_t GetTid()
{
     return syscall(__NR_gettid);
}

void UnblockAllSignals()
{
     sigset_t mask;
     sigemptyset(&mask);
     sigprocmask(SIG_SETMASK, &mask, 0);
}

/*
 * A signal handler for canceling all threads
 */
void SigUsr1Handler(int sig)
{
    if (!allThreadsCanceled)
    {
        fprintf(stderr, "Cancel all threads\n");
        CancelAllThreads();
        allThreadsCanceled = true;
    }
}

extern "C" void foo()
{
    throw (0);
}
/*
 * An endless-loop function for secondary threads
 */

void * ThreadEndlessLoopFunc(void * arg)
{
    int x = 0;
    while (1) 
    {
        x++;
        if (x > 10) 
        {
            x = 0;
        }
    }

    return 0;
}


void * ThreadExitFunc(void * arg)
{
    int p;
    pthread_exit(&p);
    return 0;
}


#define DECSTR(buf, num) { buf = (char *)malloc(10); sprintf(buf, "%d", num); }

inline void PrintArguments(char **inArgv)
{
    fprintf(stderr, "Going to run: ");
    for(unsigned int i=0; inArgv[i] != 0; ++i)
    {
        fprintf(stderr, "%s ", inArgv[i]);
    }
    fprintf(stderr, "\n");
}


/* AttachAndInstrument()
 * a special routine that runs $PIN
 */
pid_t AttachAndInstrument(list <string > * pinArgs)
{
    list <string >::iterator pinArgIt = pinArgs->begin();

    string pinBinary = *pinArgIt;
    pinArgIt++;

    pid_t parent_pid = getpid();
    
    pid_t child = fork();

    if (child) 
    {
        fprintf(stderr, "Pin injector pid %d\n", child);
        // inside parent
        return child;  
    }
    else
    {
        // inside child

        UnblockAllSignals();
        char **inArgv = new char*[pinArgs->size()+10];

        unsigned int idx = 0;
        inArgv[idx++] = (char *)pinBinary.c_str(); 
        inArgv[idx++] = (char*)"-pid"; 
        inArgv[idx] = (char *)malloc(10);
        sprintf(inArgv[idx++], "%d", parent_pid);

        for (; pinArgIt != pinArgs->end(); pinArgIt++)
        {
            inArgv[idx++]= (char *)pinArgIt->c_str();
        }
        inArgv[idx] = 0;
        
        PrintArguments(inArgv);

        execvp(inArgv[0], inArgv);
        fprintf(stderr, "ERROR: execv %s failed\n", inArgv[0]);
        kill(parent_pid, 9);
        return 0; 
    }
}


/*
 * Expected command line: <this exe> [-th_num NUM] -pin $PIN -pinarg <pin args > -t tool <tool args>
 */

void ParseCommandLine(int argc, char *argv[], list < string>* pinArgs)
{
    string pinBinary;
    for (int i=1; i<argc; i++)
    {
        string arg = string(argv[i]);
        if (arg == "-th_num")
        {
            numOfSecondaryThreads = atoi(argv[++i]) - 1;
        }
        else if (arg == "-pin")
        {
            pinBinary = argv[++i];
        }
        else if (arg == "-pinarg")
        {
            for (int parg = ++i; parg < argc; parg++)
            {
                pinArgs->push_back(string(argv[parg]));
                ++i;
            }
        }
    }
    assert(!pinBinary.empty());
    pinArgs->push_front(pinBinary);
}

pthread_t *thHandle;

extern "C" int ThreadsReady(unsigned int numOfThreads)
{
    assert(numOfThreads == numOfSecondaryThreads+1);
    return 0;
}
    
int main(int argc, char *argv[])
{
   list <string> pinArgs;
   ParseCommandLine(argc, argv, &pinArgs);
    
    signal(SIGUSR1, SigUsr1Handler);
    
    thHandle = new pthread_t[numOfSecondaryThreads];

    // start all secondary threads
    // in the secondary threads SIGUSR1 should be blocked
    BlockSignal(SIGUSR1);
    for (unsigned int i = 0; i < numOfSecondaryThreads; i++)
    {
        pthread_create(&thHandle[i], 0, ThreadEndlessLoopFunc, (void *)i);
    }
    UnblockSignal(SIGUSR1);

    AttachAndInstrument(&pinArgs);
    
    // Give enough time for all threads to get started 
    while (!ThreadsReady(numOfSecondaryThreads+1))
    {
        sched_yield();
    }        
        
    try {
        foo();
    }
    catch(...)
    {
        printf("Exception caught successfully\n");
    }
    
    for (unsigned int i = 0; i < numOfSecondaryThreads; i++)
    {
        pthread_t th;
        pthread_create(&th, 0, ThreadExitFunc, (void *)i);
    }
        
    fprintf(stderr, "Sending SIGUSR1\n");
    kill(getpid(), SIGUSR1);
    
    while(!allThreadsCanceled)
    {
        sched_yield();
    }
    fprintf(stderr, "All threads are canceled after SIGUSR1\n");

    return 0;
}

void CancelAllThreads()
{
    for (unsigned int i = 0; i < numOfSecondaryThreads; i++)
    {
        pthread_cancel(thHandle[i]);
    }
}

void BlockSignal(int sigNo)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, sigNo);   
    sigprocmask(SIG_BLOCK, &mask, 0);
}
void UnblockSignal(int sigNo)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, sigNo);
    sigprocmask(SIG_UNBLOCK, &mask, 0);
}