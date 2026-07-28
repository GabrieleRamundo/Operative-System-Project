#include "library.h"
#include "runtime_lib.h"
#define NANO_SEC_DURATION 720//duarata della giornata in minuti

int nof_users_runtime;
int nof_workers_runtime;
int nof_workers_seats_runtime;
int expl_threshold=0;
int n_nano_secs=1;
extern char **environ;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
typedef struct simulation_statistic
{
    size_t nusers_total_served;
    size_t nservices_total_erogated;
    size_t nservices_total_not_erogated;
    size_t total_active_ops;
    size_t daily_active_ops;
    long double total_wait_time;//il tempo medio di attesa degli utenti nella simulazione
    long double daily_wait_time;//il tempo medio di attesa degli utenti nella giornata
    long double total_erogation_time;//il tempo medio di erogazione dei servizi nella simulazione
    long double daily_erogation_time;//il tempo medio di erogazione dei servizi nella giornata
    size_t total_npauses;//il numero medio di pause effettuate nella giornata e il totale di pause effettuate durante la simulazione;
    float daily_mean_npauses;
}Sstats;

typedef struct all_stats
{
    Sstats service_stats[6];
    Sstats general_stats;
} Dstats;

void initialize_procs(int m_id, int semid, pid_t* pids)
{
    

    //initialize the semaphore for the workers to get the desk
    int sem_init=semget(INIT_KEY, 1, IPC_CREAT|0666);
    union semun arg;
    arg.val=1;
    if(semctl(sem_init,0,SETVAL,arg)==-1)
    {
        perror("semctl failed in initialize");
        exit(EXIT_FAILURE);
    }

    //decrease the semaphore for all processes that are initializing
    struct sembuf sop;
    sop.sem_op=-(nof_workers_runtime + nof_users_runtime +1);//decrement by the number of other processes
    sop.sem_flg=0;
    sop.sem_num=1;
    if(semop(semid,&sop,1)==-1)
    {
        perror("semop failed in initialize");
        exit(EXIT_FAILURE);
    }
    printf("all processes initialized\n");
    arg.val=0;
    if(semctl(sem_init,0,SETVAL,arg)==-1)
    {
        perror("semctl failed in initialize");
        exit(EXIT_FAILURE);
    }
}
void send_signals(pid_t* pids, int SIG)
{
    //send SIGNAL to all workers, all users and the ticket issuer 
    for(int i=0;i<nof_workers_runtime+nof_users_runtime+1;i++)
    {
        kill(pids[i],SIG);
    }
}

pid_t* add_usr(pid_t pids[], struct mymsg message, int m_id, int semid)
{
    //add the users in message to the simulation
    //add the new users to the array of pids
    int new_users=atoi(message.mtext);


    //create a new array of pids with the new users
    pid_t* copy_of_pids=(pid_t*)malloc((nof_users_runtime+nof_workers_runtime+1)*sizeof(pid_t));
    if(copy_of_pids==NULL)
    {
        perror("---------------------------malloc failed------------------");
        exit(EXIT_FAILURE);
    }
    //copy the old pids to the new array
    for(int i=0;i<nof_users_runtime+nof_workers_runtime+1;i++)
    {
        copy_of_pids[i]=pids[i];
    }
    pid_t* new_pids=reallocarray(pids, (nof_users_runtime+nof_workers_runtime+1+new_users), sizeof(pid_t));
    if(new_pids==NULL)
    {
        perror("--------------------------reallocarray failed----------------------");
        exit(EXIT_FAILURE);
    }
    //recopy the pids to the new array
    for(int i=0;i<nof_users_runtime+nof_workers_runtime+1;i++)
    {
        new_pids[i]=copy_of_pids[i];
    }
    //create the new users
    char*m_id_str=(char*)malloc(10);
    sprintf(m_id_str,"%d",m_id);
    char*semid_str=(char*)malloc(10);
    sprintf(semid_str,"%d",semid);
    for(int i=0;i<new_users;i++)
    {
        printf("%d user added\n",i+1);    

        pid_t user_pid = fork();
        if (user_pid == 0) 
        {
            if(execlp("./bin/user","./user", semid_str, m_id_str,environ, NULL)==-1)
            {
                perror("user proc failed");
                exit(EXIT_FAILURE);
            }
        } 
        else if (user_pid < 0) 
        {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        //receive pid of the new user
        if(msgrcv(m_id, &message, TEXTLEN + sizeof(struct timespec), message.mtype, 0)==-1)
        {
            perror("msgrcv Failed");
            exit(EXIT_FAILURE);
        }
        new_pids[nof_users_runtime+nof_workers_runtime+1+i]=atoi(message.mtext);//nuovi user aggiunti
        printf("new user pid: %d\n",new_pids[nof_users_runtime+nof_workers_runtime+1+i]);
    }
    nof_users_runtime+=new_users;
    //change the env variable to the new number of users
    char* new_nof_users=(char*)malloc(10);
    sprintf(new_nof_users,"%d",nof_users_runtime);
    if(setenv("NOF_USERS", new_nof_users, 1) == -1)
    {
        perror("setenv failed");
        exit(EXIT_FAILURE);
    }
    
    free(new_nof_users);
    free(copy_of_pids);
    free(m_id_str);
    free(semid_str);
    printf("users added\n");
    return new_pids;
} 

void initialize_desks(counter* seat_ptr[],int sem_workers)
{
    //put the count of how any desks with that service there are inside of the semaphore
    //initialize the worker semaphores
    union semun arg;
    for(int i=0; i<6; i++)
    {
        arg.val=0;
        if(semctl(sem_workers,i,SETVAL,arg)==-1)
        {
            perror("semctl failed in initialize");
            exit(EXIT_FAILURE);
        }
    }
    arg.val=1;
    for(int i=0;i<nof_workers_seats_runtime;i++)
    {
        seat_ptr[i]->service=rand()%6;
        //seat_ptr[i]->mean_time_required=rand()%10;
        seat_ptr[i]->is_occupied=false;
        struct sembuf sop;
        sop.sem_num=seat_ptr[i]->service;
        sop.sem_op=1;//increment
        sop.sem_flg=0;
        if(semop(sem_workers, &sop,1)==-1)
        {
            perror("semop failed in initialize desks");
            exit(EXIT_FAILURE);
        }
    }


}
int receive_stats(Dstats* stats_ptr)
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
    //receive the statistics from all workers and users
    int nusernotserved=0;
    for(int i=0;i<nof_users_runtime+nof_workers_runtime;i++)
    {
        if(msgrcv(m_id_stats, &message, TEXTLEN + sizeof(struct timespec), 1, 0)==-1)
        {
            perror("msgrcv Failed");
            exit(EXIT_FAILURE);
        }
        char* tipo=strtok(message.mtext," ");
        if(*tipo=='u')
        {
            //user statistics
            for (size_t i = 0; i < 6; i++)
            {
                int not_served=atoi(strtok(NULL," "));
                long double wait_time=atof(strtok(NULL," "));
                bool is_served=atoi(strtok(NULL," "));
                stats_ptr->service_stats[i].nservices_total_not_erogated+=not_served;
                stats_ptr->service_stats[i].daily_wait_time+=wait_time; 
                stats_ptr->service_stats[i].nusers_total_served+=is_served;
            }
        }
        else if(*tipo=='w')
        {
            //worker statistics
            int serv=atoi(strtok(NULL," "));
            int has_worked=atoi(strtok(NULL," "));
            int npauses=atoi(strtok(NULL," "));
            int erogated_services=atoi(strtok(NULL," "));
            double total_service_time=atof(strtok(NULL," "));
            nusernotserved+=atoi(strtok(NULL," "));

            stats_ptr->service_stats[serv].nservices_total_erogated +=erogated_services;
            stats_ptr->service_stats[serv].daily_erogation_time+=total_service_time;
            stats_ptr->service_stats[serv].total_npauses+=npauses;
            stats_ptr->service_stats[serv].daily_mean_npauses+=npauses;
            stats_ptr->service_stats[serv].daily_active_ops+=has_worked;
        }
        else
        {
            printf("Unknown type of message received: %s\n", message.mtext);
            exit(EXIT_FAILURE);
        }
    }
    printf("--------------------------------Finished receiving stats\n");
    return nusernotserved;
}
//create a functione to stamp the  daily statistics of the simulation on the screen 
void print_DailyStats(Dstats* stats_ptr, FILE* file)
{
    printf("Daily statistics:\n");
    fprintf(file,"Daily statistics:\n");
    printf("Service:     active operators:     Wait time:     Erogation time:     Mean number of pauses:\n");
    fprintf(file,"Service:     Number of active operators:     Wait time:     Erogation time:     Mean number of pauses:\n");
    for(int i=0;i<6;i++)
    {   
        //general total simulation stats
        stats_ptr->general_stats.total_active_ops+=stats_ptr->service_stats[i].daily_active_ops;
        stats_ptr->general_stats.total_wait_time+=stats_ptr->service_stats[i].daily_wait_time;
        stats_ptr->general_stats.total_erogation_time+=stats_ptr->service_stats[i].daily_erogation_time;

        //general daily stats
        stats_ptr->general_stats.daily_active_ops+=stats_ptr->service_stats[i].daily_active_ops;
        stats_ptr->general_stats.daily_wait_time+=stats_ptr->service_stats[i].daily_wait_time;
        stats_ptr->general_stats.daily_erogation_time+=stats_ptr->service_stats[i].daily_erogation_time;
        stats_ptr->general_stats.daily_mean_npauses+=stats_ptr->service_stats[i].daily_mean_npauses;

        //stats for each service
        stats_ptr->service_stats[i].total_active_ops+=stats_ptr->service_stats[i].daily_active_ops;
        stats_ptr->service_stats[i].total_wait_time+=stats_ptr->service_stats[i].daily_wait_time;
        stats_ptr->service_stats[i].total_erogation_time+=stats_ptr->service_stats[i].daily_erogation_time;

        //calculate the mean number of pauses
        if(stats_ptr->service_stats[i].daily_active_ops>0)
        {
            stats_ptr->service_stats[i].daily_mean_npauses=stats_ptr->service_stats[i].daily_mean_npauses/stats_ptr->service_stats[i].daily_active_ops;
        }
        printf("%d              %zu                   %LF         %LF         %f\n",i+1, stats_ptr->service_stats[i].daily_active_ops, stats_ptr->service_stats[i].daily_wait_time , stats_ptr->service_stats[i].daily_erogation_time , stats_ptr->service_stats[i].daily_mean_npauses);
        fprintf(file,"%d,%zu,%LF,%LF,%f\n",i+1, stats_ptr->service_stats[i].daily_active_ops, stats_ptr->service_stats[i].daily_wait_time , stats_ptr->service_stats[i].daily_erogation_time, stats_ptr->service_stats[i].daily_mean_npauses);
    }
    if(stats_ptr->general_stats.daily_active_ops>0)
    {
        stats_ptr->general_stats.daily_mean_npauses=stats_ptr->general_stats.daily_mean_npauses/stats_ptr->general_stats.daily_active_ops;
    }
    printf("Total          %zu                  %LF          %LF          %f\n", stats_ptr->general_stats.daily_active_ops, stats_ptr->general_stats.daily_wait_time , stats_ptr->general_stats.daily_erogation_time, stats_ptr->general_stats.daily_mean_npauses);
    fprintf(file,"Total,%zu,%LF,%LF,%f\n", stats_ptr->general_stats.daily_active_ops, stats_ptr->general_stats.daily_wait_time , stats_ptr->general_stats.daily_erogation_time, stats_ptr->general_stats.daily_mean_npauses);

    fprintf(file,"\n\n");
    fflush(file);
}
void print_SimulationStats(Dstats* stats_ptr)
{
    FILE* file=fopen("./data/tmp/simulation_stats.txt","w+");
    if(file==NULL)
    {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }
    int sim_duration=atoi(getenv("SIM_DURATION"));
    for (size_t i = 0; i < 6; i++)
    {
        fprintf(file, "Service, services erogated, services not erogated, active operators, Wait time, Wait service time, Mean number of pauses, user served for service, services erogated daily for service, services not erogated daily for service\n");
        fprintf(file, "%zu, %zu, %zu, %zu, %LF, %LF, %f, %zu, %zu, %zu\n", i+1, stats_ptr->service_stats[i].nservices_total_erogated, 
        stats_ptr->service_stats[i].nservices_total_not_erogated, stats_ptr->service_stats[i].total_active_ops, stats_ptr->service_stats[i].total_wait_time , 
        stats_ptr->service_stats[i]. total_erogation_time , stats_ptr->service_stats[i].daily_mean_npauses, stats_ptr->service_stats[i].nusers_total_served/sim_duration, 
        stats_ptr->service_stats[i].nservices_total_erogated/sim_duration, stats_ptr->service_stats[i].nservices_total_not_erogated/sim_duration);



        stats_ptr->general_stats.nservices_total_erogated+=stats_ptr->service_stats[i].nservices_total_erogated;
        stats_ptr->general_stats.nservices_total_not_erogated+=stats_ptr->service_stats[i].nservices_total_not_erogated;
        stats_ptr->general_stats.nusers_total_served+=stats_ptr->service_stats[i].nusers_total_served;
        stats_ptr->general_stats.total_npauses+=stats_ptr->service_stats[i].total_npauses;
    }
    
    printf("\n\nSimulation statistics:\n");
    printf("Total number of services erogated: %zu\n", stats_ptr->general_stats.nservices_total_erogated);
    printf("Total number of services not erogated: %zu\n", stats_ptr->general_stats.nservices_total_not_erogated);
    printf("Total number of users served: %zu\n", stats_ptr->general_stats.nusers_total_served);
    printf("Total number of active operators: %zu\n", stats_ptr->general_stats.total_active_ops);
    printf("Total wait time: %LF\n", stats_ptr->general_stats.total_wait_time );
    printf("Total wait erogation time: %LF\n", stats_ptr->general_stats. total_erogation_time);
    printf("Total number of pauses: %zu\n", stats_ptr->general_stats.total_npauses);
    printf("Mean number of users served daily: %zu\n", stats_ptr->general_stats.nusers_total_served/sim_duration);
    printf("Mean number of services erogated daily %zu\n",stats_ptr->general_stats.nservices_total_erogated/sim_duration);
    printf("Mean number of services not erogated daily %zu\n",stats_ptr->general_stats.nservices_total_not_erogated/sim_duration);
    
    fprintf(file,"Total, %zu, %zu, %zu, %LF, %LF, %f, %zu, %zu, %zu\n", stats_ptr->general_stats.nservices_total_erogated,
    stats_ptr->general_stats.nservices_total_not_erogated, stats_ptr->general_stats.total_active_ops, stats_ptr->general_stats.total_wait_time ,
    stats_ptr->general_stats.total_erogation_time , stats_ptr->general_stats.daily_mean_npauses, stats_ptr->general_stats.nusers_total_served/sim_duration,
    stats_ptr->general_stats.nservices_total_erogated/sim_duration, stats_ptr->general_stats.nservices_total_not_erogated/sim_duration);
    
}
void reset_stats(Dstats* stats_ptr)
{
    //reset the statistics to 0 for the new day
    printf("Resetting the stats\n");
    for(int i=0;i<6;i++)
    {
        stats_ptr->service_stats[i].daily_active_ops=0;
        stats_ptr->service_stats[i].daily_wait_time=0;
        stats_ptr->service_stats[i].daily_erogation_time=0;
        stats_ptr->service_stats[i].daily_mean_npauses=0;
    }
    //reset general stats
    stats_ptr->general_stats.daily_active_ops=0;
    stats_ptr->general_stats.daily_wait_time=0;
    stats_ptr->general_stats.daily_erogation_time=0;
    stats_ptr->general_stats.daily_mean_npauses=0;
}
void simulate(int m_id, pid_t pids[], int semid, counter* seat_ptr[])
{
    int current_duration=0;
    srand(time(NULL)+getpid());
    //create message queue to see if users have been added(uses IPC_NOWAIT)
    int m_id_add_users;
    if((m_id_add_users=msgget(ADD_USERS_KEY,0666|IPC_CREAT))==-1)
    {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }
    //create shared memory for stats
    Dstats* stats_ptr=malloc(sizeof(Dstats));
    if(stats_ptr==NULL)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    int sim_duration=atoi(getenv("SIM_DURATION"));
    nof_users_runtime=atoi(getenv("NOF_USERS"));
    nof_workers_runtime=atoi(getenv("NOF_WORKERS"));
    nof_workers_seats_runtime=atoi(getenv("NOF_WORKER_SEATS"));
    expl_threshold=atoi(getenv("EXPLODE_THRESHOLD"));
    n_nano_secs=atoi(getenv("N_NANO_SEC"));
    FILE* file=fopen("./data/tmp/daily_stats.txt","w+");
    if(file==NULL)
    {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }
    int sem_workers=semget(WAITING_KEY, 6, IPC_CREAT|0666);
    if(sem_workers==-1)
    {
        perror("semget failed");
        exit(EXIT_FAILURE);
    }
    //inizialize simulation stats
    for (size_t i = 0; i < 6; i++)
    {
        stats_ptr->service_stats[i].nservices_total_erogated=0;
        stats_ptr->service_stats[i].nservices_total_not_erogated=0;
        stats_ptr->service_stats[i].nusers_total_served=0;
        stats_ptr->service_stats[i].total_active_ops=0;
        stats_ptr->service_stats[i].total_wait_time=0;
        stats_ptr->service_stats[i].total_erogation_time=0;
        stats_ptr->service_stats[i].total_npauses=0;
    }
    stats_ptr->general_stats.nservices_total_erogated=0;
    stats_ptr->general_stats.nservices_total_not_erogated=0;
    stats_ptr->general_stats.nusers_total_served=0;
    stats_ptr->general_stats.total_active_ops=0;
    stats_ptr->general_stats.total_wait_time=0;
    stats_ptr->general_stats.total_erogation_time=0;
    stats_ptr->general_stats.total_npauses=0;

    //simulate the days
    while(current_duration<sim_duration)
    {
        fflush(stdout);
        printf("GIORNATA %d\n\n\n", current_duration+1);

        //reset the statistics for the new day
        reset_stats(stats_ptr);
        //checks if users have been added
        struct mymsg message;
        message.mtype=1;
        if(msgrcv(m_id_add_users, &message, TEXTLEN + sizeof(struct timespec), message.mtype, IPC_NOWAIT)!=-1)
        {
            for(int i=0;i<nof_users_runtime+nof_workers_runtime+1;i++)
            {
                printf("New pids[%d]: %d\n",i,pids[i]);
            }
            pids=add_usr(pids,message,m_id, semid);
            for(int i=0;i<nof_users_runtime+nof_workers_runtime+1;i++)
            {
                printf("------------------%d--------------\n",pids[i]);
            }
        }

        initialize_desks(seat_ptr,sem_workers);

        //initialize all workers, users and the ticket issuer
        printf("initializing processes\n");
        initialize_procs(m_id,semid,pids);
        //reset the semaphore for the end of the day to 0
        union semun arg;
        arg.val=0;
        if(semctl(semid,2,SETVAL,arg)==-1)
        {
            perror("semctl failed in runtime");
            exit(EXIT_FAILURE);
        }
        //Now the day starts, set an alarm(time of simulation) and increment the semaphore to start the day
        struct sembuf sop;
        sop.sem_num=0;
        sop.sem_op=nof_users_runtime+nof_workers_runtime+1;//increment by the number of other processes
        sop.sem_flg=0;
        if(semop(semid,&sop,1)==-1)
        {
            perror("semop failed in runtime");
            exit(EXIT_FAILURE);
        }
        if(n_nano_secs<20000)
        {
            printf("WARNING: execution might have undefined behaviour \n");
        }
        long time_nano_seconds=NANO_SEC_DURATION*n_nano_secs;
        if(time_nano_seconds<0)
        {
            perror("time_nano_seconds is negative");
            exit(EXIT_FAILURE);
        }
        int time_seconds=floor(time_nano_seconds/1000000000);
        time_nano_seconds=time_nano_seconds%1000000000;
        struct timespec time={time_seconds,time_nano_seconds};
        nanosleep(&time,NULL);
        arg.val=0;
        if(semctl(semid,0,SETVAL,arg)==-1)
        {
            perror("semctl failed in runtime");
            exit(EXIT_FAILURE);
        }

        

        //increment the semaphore to end the day for the number of processes, and also send the signal to all workers and users to end the day
        send_signals(pids,SIGUSR1);
        sop.sem_num=2;
        if(semop(semid,&sop,1)==-1)
        {
            perror("semop failed in runtime");
            exit(EXIT_FAILURE);
        }
        //reset the semaphores for the different services to zero
        for(int i=0;i<6;i++)
        {
            arg.val=0;
            if(semctl(sem_workers,i,SETVAL,arg)==-1)
            {
                perror("semctl failed in runtime");
                exit(EXIT_FAILURE);
            }
        }
        printf("semaphores removed\n");
        printf("day ended\n");
        
        

        printf("Receiving stats now\n");
        if(receive_stats(stats_ptr)>expl_threshold)
        {
            printf("TOO MUCH USERS NOT SERVED, EXPLODING\n");
            //send signals to all workers, all users and the ticket issuer to stop themselves

            break;
        }
        //print the daily statistics
        print_DailyStats(stats_ptr, file);
        //receive statistics from all workers and users
        current_duration++;
    }
    fclose(file);
    //print the simulation statistics
    send_signals(pids,SIGQUIT);
    print_SimulationStats(stats_ptr);
    free(stats_ptr);
    free(pids);
}