#include "library.h"

int reserveSem(int semid, int semnum)
{
    struct sembuf sop;
    sop.sem_num=semnum;
    sop.sem_op=-1;//reserve
    sop.sem_flg=0;
    return semop(semid,&sop,1);
}
int releaseSem(int semid, int semnum)
{
    struct sembuf sop;
    sop.sem_num=semnum;
    sop.sem_op=1;//release
    sop.sem_flg=0;
    return semop(semid,&sop,1);
}
int put_sig_mask()
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGQUIT);
    if (sigprocmask(SIG_SETMASK, &mask, &oldmask) == -1) {
        perror("sigprocmask failed");
        return -1;
    }
    return 0;
}

int remove_sig_mask()
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGQUIT);
    if (sigprocmask(SIG_UNBLOCK, &mask, &oldmask) == -1) {
        perror("sigprocmask failed");
        return -1;
    }
    return 0;
}