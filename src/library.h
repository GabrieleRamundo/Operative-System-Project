#include<sys/types.h>
#include<stdbool.h>
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/shm.h>
#include<string.h>
#include<sys/sem.h>
#include<time.h>
#include<signal.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<errno.h>
#include<unistd.h>
#include<math.h>
#define _GNU_SOURCE

//Keys for the semaphores
#define INIT_KEY 54888
#define WAITING_KEY 0x4440
#define SEMKEY 0x2222

//KEY for the shared memory
#define SHMKEY 0x1234

//KEYS for the message queues
#define TICKETS_KEY 54881
#define FIRST_KEY 54882
#define MSGKEY 0x3333
#define ADD_USERS_KEY 54880
#define STATS_KEY 54890

typedef struct Services
{
    int service;//da 0 a 5 
    //size_t mean_time_required; 
    bool is_occupied;
}counter;

#define TEXTLEN 128
struct mymsg {
    long mtype; /* Message type */
    char mtext[TEXTLEN]; /* Message body */
    struct timespec time; 
    };

void sigs_handler(int sig);
int reserveSem(int semid, int semnum);
int releaseSem(int semid, int semnum);
int put_sig_mask();
int remove_sig_mask();