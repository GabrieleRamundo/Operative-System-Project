#include "library.h"
#define NANO_SEC_DURATION 720//duarat della giornata in minuti
int n_request;
int p_serv_min;
int p_serv_max;

typedef struct daily_stats_user
{
    int not_served[6];
    long double  wait_time[6];
    bool is_served[6];
}Dstats;


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

void send_stats(Dstats* daily_stats)
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
    //send the statistics to the director
    char statistics[128];
    sprintf(statistics,"u ");
    for(size_t i=0;i<6;i++)
    {
        sprintf(message.mtext,"%d %LF %d ", daily_stats->not_served[i], daily_stats->wait_time[i],daily_stats->is_served[i]);
        strcat(statistics,message.mtext);
    }
    strcpy(message.mtext,statistics);
    if(msgsnd(m_id_stats, &message, TEXTLEN + sizeof(struct timespec), 0)==-1)
    {
        perror("msgsnd Failed");
        exit(EXIT_FAILURE);
    }
}
void reset_daily_stats(Dstats* daily_stats)
{
    //reset the statistics to 0 for the new day
    for(int i=0;i<6;i++)
    {
        daily_stats->not_served[i]=0;
        daily_stats->wait_time[i]=0;
        daily_stats->is_served[i]=false;
    }
}
int the_office(int requests[], struct mymsg message, Dstats* stats)
{
    //position 0 is the number of requests resolved, position 1 is the number of requests not resolved
    int n_nano_secs=atoi(getenv("N_NANO_SEC"));
    int resolved=0;
    int ticket_queue= msgget(TICKETS_KEY,0666);
    if(ticket_queue==-1)
    {
        perror("msgget failed in the_office");
        exit(EXIT_FAILURE);
    }
    for(int i=0; i<n_request&&requests[i]!=-1&&!day_finished;i++)
    {
        //go to the ticket dispenser to see if there's a desk available for service wanted
        message.mtype=1;
        sprintf(message.mtext,"%d %d",requests[i],getpid());
        if(msgsnd(ticket_queue, &message, TEXTLEN + sizeof(struct timespec), 0)==-1)
        {
            if(day_finished)
            {
                return resolved;
            }
            else
            {
                perror("msgsnd failed in the_office");
                exit(EXIT_FAILURE);
            }
        }
        //wait for the ticket dispenser to send a message back
        message.mtype=getpid();
        if(day_finished||msgrcv(ticket_queue, &message, TEXTLEN + sizeof(struct timespec), getpid(), 0)==-1)
        {
            if(day_finished)
            {
                return resolved;
            }
            else
            {
                perror("msgrcv failed in the_office");
                exit(EXIT_FAILURE);
            }
            
        }
        //check if the service is available
        if(strcmp(message.mtext,"Service not available")!=0)
        {
            //get message queue with key as the request service
            int m_id_users;
            if((m_id_users=msgget(FIRST_KEY+requests[i],0666|IPC_CREAT))==-1)
            {
                perror("msgget failed in the_office");
                exit(EXIT_FAILURE);
            }
            //send message to the worker with the pid of the user
            message.mtype=1;
            sprintf(message.mtext,"%d %s",getpid(), "request");
            //wait for the worker to send a message back
            struct timespec start_time;
            clock_gettime(CLOCK_MONOTONIC, &start_time);
            message.time.tv_nsec=start_time.tv_nsec;
            message.time.tv_sec=start_time.tv_sec;
            if(msgsnd(m_id_users, &message, TEXTLEN + sizeof(struct timespec), 0)==-1)
            {
                if(day_finished)
                {
                    break;
                }
                else
                {
                    perror("msgsnd failed in the_office");
                    exit(EXIT_FAILURE);
                }
            }
            
            message.mtype=getpid();
            if(day_finished||msgrcv(m_id_users, &message, TEXTLEN + sizeof(struct timespec), getpid(), 0)==-1)
            {
                break;
            }
            //get the end_time of the service from the message from worker(third parameter)
            stats->wait_time[requests[i]]+=(message.time.tv_sec*10e9+message.time.tv_nsec)/n_nano_secs;
            resolved++;
        }
        else
        {
            break;
        }
    }
    return resolved;
}

int main(int argc, char *argv[])
{
    //initialize signal handlers
    signal(SIGUSR1,sigs_handler);
    signal(SIGQUIT,sigs_handler);

    srand(time(NULL)+getpid());
    int semid=atoi(argv[1]);
    int m_id=atoi(argv[2]);
    bool request_service=false;

    Dstats* daily_stats=(Dstats*)malloc(sizeof(Dstats));
    if(daily_stats==NULL)
    {
        perror("malloc failed in user");
        exit(EXIT_FAILURE);
    }
    //initialize the daily stats
    reset_daily_stats(daily_stats);

    //send pid to director
    struct mymsg message;
    message.mtype=1;
    sprintf(message.mtext,"%d",getpid());
    if(msgsnd(m_id, &message, TEXTLEN + sizeof(struct timespec), 0)==-1)
    {
        perror("msgsnd Failed");
        exit(EXIT_FAILURE);
    }

    int p_serv=0;
    int choice=0; 
    struct timespec arrival;

    n_request=atoi(getenv("N_REQUESTS"));

    int* requests=(int*)malloc(n_request*sizeof(int));
    p_serv_min=atoi(getenv("P_SERV_MIN"));
    p_serv_max=atoi(getenv("P_SERV_MAX"));

    //initialize the requests array
    for(int i=0;i<n_request;i++)
    {
        requests[i]=-1;
    }
    int n_req_services=0;
    int n_nano_secs=atoi(getenv("N_NANO_SEC"));
    while(simulate)
    {
        day_finished=false;
        reset_daily_stats(daily_stats);
        
        if(requests[0]==-1)
        {
            p_serv=(rand()%p_serv_max)+p_serv_min;
            choice=rand()%100;
        
            if(choice<p_serv)
            {
                n_req_services=rand()%n_request;
                if(n_req_services!=0)
                {
                    request_service=true;
                }
                for(int i=0;i<n_req_services;i++)
                {
                    requests[i]=rand()%6;
                }
            }
            else
            {
                request_service=false;
            }
        }
        if(releaseSem(semid,1)==-1)
        {
            perror("release sem failed in user");
            exit(EXIT_FAILURE);
        }
        //wait for director to send a signal to start the simulation
        if((reserveSem(semid,0)==-1))
        {
            if(simulate)
            {
                perror("semaphore reserve failed in user");
                exit(EXIT_FAILURE);
            }
            else
            {
                break;
            }
        }

        if(request_service)
        {
            arrival.tv_nsec=(rand()%(int)(NANO_SEC_DURATION))*n_nano_secs;
            arrival.tv_sec=0;
            nanosleep(&arrival, NULL);

            //go to the office to resolve the requests
            int resolved=the_office(requests, message, daily_stats);
            //remove the elaborated requests, replacing them with those not yet done: the elaborated requests are at the beginning of the array
            for(int i=resolved;i<n_req_services;i++)
            {
                daily_stats->not_served[requests[i]]+=1;
            }
            n_req_services-=resolved;
            for(int i=0;i<resolved;i++)
            {
                daily_stats->is_served[requests[i]]=true;
                for(int j=0;j<n_request-1;j++)
                {
                    requests[j]=requests[j+1];
                }
                requests[n_request-1]=-1;
            }

            if(requests[0]==-1)
            {
                request_service=false;
            }

        }
        
        
        //wait for director to send a signal to end the simulation
        if(day_finished||reserveSem(semid,2)==-1)
        {
            if(!simulate)
            {
                break;
            }
        }
        //send the stats to the director
        send_stats(daily_stats);

    }
    free(requests);
    free(daily_stats);
    return 0;
}