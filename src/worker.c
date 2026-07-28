
#include "library.h"
int nof_worker_seats=0;
int n_nano_secs=1;
int semid;
union semun 
{
    int val;
    struct semid_ds* buf;
    unsigned short* array;
    #if defined(__linux__)
    struct seminfo* __buf;
    #endif
};

typedef struct daily_stats_op
{
    int service;
    bool has_worked;
    size_t npauses;
    size_t erogated_services;
    long double total_service_time;
    size_t nusernotserved;//user not served at the end of the day
}Dstats_op;


bool day_finished=false;
bool simulate=true;
void sigs_handler(int sig)
{
    if(sig==SIGUSR1)
    {
        day_finished=true;
    }
    
    else if (sig==SIGQUIT)
    {
        day_finished=true;
        simulate=false;
    }

}

int initialize(int m_id, int sem_init, int semid, struct mymsg message, counter* desks[], int serv)
{
    int m_id_users=-1;
    //critic section
    if((reserveSem(sem_init,0)==-1))
    {
        if(simulate)
        {
            perror("reserve sem failed in initialize");
            exit(EXIT_FAILURE);
        }
        else
        {
            return -1;
        }
    }
    //initialize itself: create message queue between worker and the users
    for(int i=0; i<nof_worker_seats; i++)
    {
        if(desks[i]->service==serv)
        {
            desks[i]->is_occupied=true;
            
            if((m_id_users=msgget(FIRST_KEY+serv,0666|IPC_CREAT))==-1)
                {
                    perror("msgget failed in initialize");
                    exit(EXIT_FAILURE);
                }
            break;

        }
    }

    releaseSem(sem_init, 0);//end of critic section

    releaseSem(semid, 1);//end of initialize
    return m_id_users;
}

counter** create_desks(int nof_worker_seats)
{
    //attach mem shared with director for the counters, saving it in an array like the director
    counter** desks=malloc(nof_worker_seats*sizeof(counter));

    for(int i=0;i<nof_worker_seats;i++)
    {
        int counter_id=shmget(SHMKEY+i,sizeof(counter),0666);
        if(counter_id==-1)
        {
            perror("shmget failed");
            exit(EXIT_FAILURE);
        }
        desks[i]=shmat(counter_id,NULL,0);
        if(desks[i]==(void*)-1)
        {
            perror("shmat failed");
            exit(EXIT_FAILURE);
        }
    }
    return desks;
}

Dstats_op* create_stats(int serv)
{
    Dstats_op* daily_stats=(Dstats_op*)malloc(sizeof(Dstats_op));
    if(daily_stats==NULL)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    daily_stats->service=serv;
    daily_stats->has_worked=0;
    daily_stats->npauses=0;
    daily_stats->erogated_services=0;
    daily_stats->total_service_time=0;
    return daily_stats;
}
void send_stats(Dstats_op* daily_stats,int serv)
{
    //get queue for the stats
    int m_id_stats=msgget(STATS_KEY,0666|IPC_CREAT);
    if(m_id_stats==-1)
    {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }
    struct mymsg message;
    message.mtype=1;
    sprintf(message.mtext,"w %d %d %ld %ld %LF %ld",daily_stats->service, daily_stats->has_worked, daily_stats->npauses, daily_stats->erogated_services, daily_stats->total_service_time,daily_stats->nusernotserved);
    put_sig_mask();
    if(msgsnd(m_id_stats, &message, TEXTLEN + sizeof(struct timespec), 0)==-1)
    {
        perror("msgsnd Failed");
        exit(EXIT_FAILURE);
    }
    remove_sig_mask();
}
void clearServiceQueue(int workplace, Dstats_op* daily_stats)
{
    if(workplace==-1)
    {
        return;
    }
    struct mymsg message;
    message.mtype=1;
    while(msgrcv(workplace, &message, TEXTLEN + sizeof(struct timespec), 1, IPC_NOWAIT)!=-1)
    {
        daily_stats->nusernotserved++;
    }
}
int main(int argc, char *argv[])
{
    semid=atoi(argv[1]);
    int m_id=atoi(argv[2]);
    //set signal handlers
    signal(SIGUSR1,sigs_handler);
    signal(SIGQUIT,sigs_handler);
    srand(time(NULL)+getpid());

    //create the array for the mean time of the services
    int mean_time[6]={10,8,6,8,20,20};

    //send pid to director, with a signal mask to not get it interruped 
      
    struct mymsg message;
    message.mtype=1;
    sprintf(message.mtext,"%d",getpid());
    
    put_sig_mask();
    if(msgsnd(m_id, &message, TEXTLEN + sizeof(struct timespec), 0)==-1)
    {
        perror("msgsnd Failed");
        exit(EXIT_FAILURE);
    }
    remove_sig_mask();
    nof_worker_seats=atoi(getenv("NOF_WORKER_SEATS"));

    counter** desks= create_desks(nof_worker_seats);
    
    //choose a random service to provide
    int serv=rand()%6;
    //number of total pauses possible
    int pauses=atoi(getenv("N_PAUSES"));
    int nano_secs=atoi(getenv("N_NANO_SEC"));
    int n_nano_secs=atoi(getenv("N_NANO_SEC"));
    int semid_workers=semget(WAITING_KEY,6,IPC_CREAT|0666);
    if(semid_workers==-1)
    {
        perror("semget failed");
        exit(EXIT_FAILURE);
    }
    //create struct for the daily stats
    Dstats_op* daily_stats= create_stats(serv);
    int sem_init=semget(INIT_KEY, 1, IPC_CREAT|0666);
    if(sem_init==-1)
    {
        perror("semget failed");
        exit(EXIT_FAILURE);
    }
    int workplace=-1;
    while(simulate)
    {
        workplace=-1;
        day_finished=false;

        daily_stats->service=serv;
        daily_stats->has_worked=0;
        daily_stats->npauses=0;
        daily_stats->erogated_services=0;
        daily_stats->total_service_time=0;
        daily_stats->nusernotserved=0;
        //initialize the semaphore for the worker
        workplace=initialize(m_id, sem_init,semid, message, desks, serv);
        //semid to wait for the start of the day.
        if(reserveSem(semid, 0)==-1)
        {
            if(simulate)
            {
                perror("semaphore reserve failed in worker");
                exit(EXIT_FAILURE);
            }
            else
            {
                break;
            }
        }

        if(workplace!=-1)
        {
            //decrease only the "serv" semaphore in semaphore set
            if(day_finished||reserveSem(semid_workers, serv)!=-1)
            {
                daily_stats->has_worked=1;
                int time_before_pause=rand()%4+1;
                while(true)
                {
                    //receive requests from users, with in it the pid of the user for the message back
                    struct mymsg message;
                    message.mtype=1;
                    if(day_finished||msgrcv(workplace, &message, TEXTLEN + sizeof(struct timespec), 1, 0)==-1)
                    {
                        break;
                    }
                    //the message is the pid of the user
                    char user_pid[10];
                    strcpy(user_pid,strtok(message.mtext," "));
                    int min=(mean_time[serv]/2);
                    int max=(mean_time[serv]+min);
                    long double actual_time=min+rand()%(max-min+1);
                    struct timespec endtime;
                    clock_gettime(CLOCK_MONOTONIC, &endtime);
                    int slept=nanosleep((struct timespec[]){{0,actual_time*n_nano_secs}},NULL);
                    if(slept==-1)
                    {
                        break;
                    }
                    else
                    {
                        struct timespec finishtime;
                        clock_gettime(CLOCK_MONOTONIC, &finishtime);
                        //calculate the actual time spent on the service
                        actual_time=((finishtime.tv_sec-endtime.tv_sec)*10e9 + (finishtime.tv_nsec-endtime.tv_nsec))/n_nano_secs;
                        daily_stats->erogated_services++;
                        daily_stats->total_service_time+=actual_time;

                        //send message to user that the service is done
                        message.mtype=atoi(user_pid);

                        sprintf(message.mtext,"%d %s",getpid(), "done"); 
                        message.time.tv_sec=endtime.tv_sec-message.time.tv_sec;
                        message.time.tv_nsec=endtime.tv_nsec-message.time.tv_nsec; 
                        if(msgsnd(workplace, &message, TEXTLEN + sizeof(struct timespec), 0)==-1)
                        {
                            printf("Failed while sending message to user %d(worker)\n",getpid());
                            perror("msgsnd failed");
                            exit(EXIT_FAILURE);
                        }
                        if(pauses>0)
                        {
                            if(time_before_pause==0)
                            {
                                //release the desk for the other workers
                                daily_stats->npauses++;
                                if(releaseSem(semid_workers, serv)==-1)
                                {
                                    perror("semRelease failed while releasing semaphore");
                                    exit(EXIT_FAILURE);
                                }
                                pauses--;
                                break;
                            }
                            else
                            {
                                time_before_pause--;
                            }
                        }         
                    }


                }
            }
            

        }
        //wait for the director to increment the semaphore to end the day
        if(reserveSem(semid, 2)==-1)
        {
            if(!simulate)
            {
                break;
            }
        }
        //send the stats to the director
        clearServiceQueue(workplace,daily_stats);
        send_stats(daily_stats,serv);
    }
    free(desks);
    free(daily_stats);
    return 0;
}