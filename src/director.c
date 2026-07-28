#include "library.h"
#include "runtime_lib.h"

int nof_users=0;
int nof_workers=0;
int nof_worker_seats=0;
int n_services;
int sim_duration;

extern char **environ;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};





void printstats()
{
    FILE* fstats=fopen("data/stats.csv","a");
    if(!fstats)
    {
        perror("error in fopen");
        exit(EXIT_FAILURE);
    }
    //write stats on file
}
void configreader(char* config)
{
    FILE* fconfig=fopen(config,"r");
    if(!fconfig)
    {
        perror("config file not found");
        exit(EXIT_FAILURE);
    }
    char string[128];
    while(fgets(string,128,fconfig)!=NULL)
    {
        if(string[0]=='#')
            continue;
        char* tokenword=strtok(string,"=");//prima del =
        char* tokenvalue=strtok(NULL,"=");//dopo il =
        setenv(tokenword,tokenvalue,0);
    }
    fclose(fconfig);
}

void sigalarm_handler(int sig)
{
    printf("alarm ringed\n");
}

int create_procs(char* semid_str, char* m_id_str,char* argv[])
{

    signal(SIGALRM,sigalarm_handler);



    pid_t pid=fork();
    if (pid == 0) 
    {
        // Create ticket issuer process
        if(execlp("./bin/ticket_dispenser","./ticket_dispenser", semid_str,m_id_str,environ, NULL)==-1)
        {
            perror("ticket issuer failed");
            exit(EXIT_FAILURE);
        }
    } 
    else if (pid > 0) 
    {
        
        // Create worker processes
        for (int i = 0; i < nof_workers; i++) 
        {
            pid_t worker_pid = fork();
            if (worker_pid == 0) 
            {
                if(execlp("./bin/worker","./worker",semid_str,m_id_str, environ,NULL)==-1)
                {
                    perror("worker proc failed");
                    exit(EXIT_FAILURE);
                }
            } 
            else if (worker_pid < 0) 
            {
                perror("fork failed");
                exit(EXIT_FAILURE);
            }
        }

        // Create user processes
        for (int i = 0; i < nof_users; i++) 
        {
            pid_t user_pid = fork();
            if (user_pid == 0) 
            {
                execlp("./bin/user","./user",semid_str,m_id_str, environ, NULL);
                perror("user proc failed");
                exit(EXIT_FAILURE);
            } 
            else if (user_pid < 0) 
            {
                perror("fork failed");
                exit(EXIT_FAILURE);
            }
        }

    }
    else 
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}
void cleanup(int m_id, int semid, int sem_workers, counter* seat_ptr[])
{
    printf("-------------------entered cleanup----------------\n");
    //closing mesage queue
    if(msgctl(m_id, IPC_RMID, NULL)==-1)
    {
        perror("Closing queue failed");
        exit(EXIT_FAILURE);
    }
    int m_id_add_users=msgget(ADD_USERS_KEY,0666|IPC_CREAT);
    if(msgctl(m_id_add_users, IPC_RMID, NULL)==-1)
    {
        perror("Closing queue failed");
        exit(EXIT_FAILURE);
    }
    int m_id_stats=msgget(STATS_KEY,0666|IPC_CREAT);
    if(msgctl(m_id_stats, IPC_RMID, NULL)==-1)
    {
        perror("Closing queue failed");
        exit(EXIT_FAILURE);
    }
    int m_id_tickets=msgget(TICKETS_KEY,0666|IPC_CREAT);
    if(msgctl(m_id_tickets, IPC_RMID, NULL)==-1)
    {
        perror("Closing queue failed");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < 6; i++)
    {
        int m_id_first_key=msgget(FIRST_KEY+i,0666|IPC_CREAT);
        if(msgctl(m_id_first_key, IPC_RMID, NULL)==-1)
        {
            perror("Closing queue failed");
            exit(EXIT_FAILURE);
        }
    }
    printf("-------------------Closed queues----------------\n");
    if(semctl(semid,0,IPC_RMID,NULL)==-1)
    {
        perror("semctl failed");
        exit(EXIT_FAILURE);
    }
    int sem_init=semget(INIT_KEY, 1, IPC_CREAT|0666);
    if(semctl(sem_init,0,IPC_RMID,NULL)==-1)
    {
        perror("semctl failed");
        exit(EXIT_FAILURE);
    }
    if(semctl(sem_workers,0,IPC_RMID,NULL)==-1)
    {
        perror("semctl failed");
        exit(EXIT_FAILURE);
    }
    printf("-----------------------semaphores removed----------------------\n");
    //remove the shared memory for the stats
    for (int i = 0; i < nof_worker_seats; i++)
    {
        int shm_id_seat=shmget(SHMKEY+i, sizeof(counter), IPC_CREAT|0666);
        if(shmctl(shm_id_seat, IPC_RMID, NULL)==-1)
        {
            perror("shmctl failed");
            exit(EXIT_FAILURE);
        }
    }
    free(seat_ptr);
    printf("-----------------------shared memory removed---------------------\n");
    printf("-----------------------CLEANUP COMPLETED ---------------------\n");
}


int main(char argc, char* argv[])
{ 
    //check if the config file is passed
    if(argc!=2)
    {
        printf("Usage: %s <config_file>\n",argv[0]);
        exit(EXIT_FAILURE);
    }


    // create semaphore for synchronization
    int semid=semget(SEMKEY,4,IPC_CREAT|0666);
    if(semid==-1)
    {
        perror("semget failed");
        exit(EXIT_FAILURE);
    }
    //create 6 semaphores for workers waiting the desk
    int semid_workers=semget(WAITING_KEY,6,IPC_CREAT|0666);
    if(semid_workers==-1)
    {
        perror("semget failed");
        exit(EXIT_FAILURE);
    }

    union semun arg;
    unsigned short semarray[3]={0,0,0};//first position is the old semid, second is for the start of the day, third is for the end of the day
    arg.array=semarray;
    if(semctl(semid,0,SETALL,arg)==-1)
    {
        perror("semctl failed");
        exit(EXIT_FAILURE);
    }
    char* semid_str=(char*)malloc(10);
    sprintf(semid_str,"%d",semid);

    //initialize the semaphores for the workers
    for(int i=0;i<6;i++)
    {
        arg.val=1;
        if(semctl(semid_workers,i,SETVAL,arg)==-1)
        {
            perror("semctl failed");
            exit(EXIT_FAILURE);
        }
    }
    
    //create message queue
    int m_id;
    if((m_id=msgget(MSGKEY,0666|IPC_CREAT))==-1)
    {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }
    char* m_id_str=(char*)malloc(10);
    sprintf(m_id_str,"%d",m_id);

    //read config file
    configreader(argv[1]);
    nof_worker_seats=atoi(getenv("NOF_WORKER_SEATS"));
    nof_workers=atoi(getenv("NOF_WORKERS"));
    nof_users=atoi(getenv("NOF_USERS"));
    n_services=atoi(getenv("N_SERVICES"));//da vedere se serve
    sim_duration=atoi(getenv("SIM_DURATION"));


    // Create worker seats
    counter** seat_ptr=(counter**)malloc(nof_worker_seats*sizeof(counter));
    for (int i = 0; i < nof_worker_seats; i++) 
    {
        //worker seat is a shmem of counter memory
        int seat=shmget(SHMKEY+i,sizeof(counter),IPC_CREAT|0666);
        if(seat==-1)
        {
            perror("shmget failed");
            exit(EXIT_FAILURE);
        }
        seat_ptr[i]=shmat(seat,NULL,0);
        if(seat_ptr[i]==(void*)-1)
        {
            perror("shmat failed");
            exit(EXIT_FAILURE);
        }
        seat_ptr[i]->service=0;
        //seat_ptr[i]->mean_time_required=0;
        seat_ptr[i]->is_occupied=0;
    }
    //create an array of pids to save the pids of the workers and users and the ticket issuer
    pid_t* pids=(pid_t*)malloc((nof_workers+nof_users+1)*sizeof(pid_t));
   
    //create processes
    create_procs(semid_str,m_id_str,argv);

    //receive pid of all workers and users and ticket issuer to make a two way communication
    struct mymsg message;
    message.mtype=1;
    for(int i=0;i<nof_workers + nof_users+1;i++)
    {
        if(msgrcv(m_id, &message, TEXTLEN + sizeof(struct timespec), message.mtype, 0)==-1)
        {
            perror("msgrcv Failed");
            exit(EXIT_FAILURE);
        }
        pids[i]=atoi(message.mtext);
    }


    //start the simulation
    simulate(m_id,pids, semid, seat_ptr);
    printf("simulation ended\n");
    //clean up
    cleanup(m_id,semid,semid_workers, seat_ptr);
    free(semid_str);
    free(m_id_str);
    return 0;
    
}