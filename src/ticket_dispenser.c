#include "library.h"

int nof_worker_seats=0;

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



bool is_service(int service, counter* desks[])
{
    //checks the desks if the service is available
    for(int i=0;i<nof_worker_seats;i++)
    {
        if(desks[i]->service==service&&desks[i]->is_occupied==true)
        {
            return true;
        }
    }
    return false;
}

void dispense(int m_id_users, struct mymsg message, counter* desks[])
{
    //wait for messages on the queue from users
    while(true)
    {
        message.mtype=1;
        if(day_finished||msgrcv(m_id_users, &message, TEXTLEN + sizeof(struct timespec), message.mtype, 0)==-1)
        {
            break;
        }
        char serv_request[10];
        //parse the message: first part is the service requested, second part is the pid of the user
        strcpy(serv_request,strtok(message.mtext," "));
        char user_pid[10];
        strcpy(user_pid,strtok(NULL," "));
        if(is_service(atoi(serv_request), desks))
        {
            //send a message to the user with the key of the desk that will serve him
            message.mtype=atoi(user_pid);
            sprintf(message.mtext,"Someone will serve you");
            if(msgsnd(m_id_users, &message, TEXTLEN + sizeof(struct timespec), 0)==-1)
            {
                break;
            }
        }
        else
        {
            //send a message to the user that the service is not available
            message.mtype=atoi(user_pid);
            sprintf(message.mtext,"Service not available");
            if(msgsnd(m_id_users, &message, TEXTLEN + sizeof(struct timespec), 0)==-1)
            {
                break;
            }            
        }
        //check if the end of the day has been reached
        if(day_finished)
        {
            break;
        }
    }
}
void clearServiceQueue(int m_id_users)
{
    //clear the queue of the users
    struct mymsg message;
    message.mtype=1;
    while(msgrcv(m_id_users, &message, TEXTLEN + sizeof(struct timespec), message.mtype, IPC_NOWAIT)!=-1);
}

int main(int argc, char *argv[])
{
    //initialize signal handlers
    signal(SIGUSR1,sigs_handler);
    signal(SIGUSR2,sigs_handler);
    signal(SIGQUIT,sigs_handler);
    int semid=atoi(argv[1]);
    int m_id=atoi(argv[2]);
    struct mymsg message;
    //send pid to director
    message.mtype=1;
    sprintf(message.mtext,"%d",getpid());
    if(msgsnd(m_id, &message, TEXTLEN + sizeof(struct timespec), 0)==-1)
    {
        perror("msgsnd Failed");
        exit(EXIT_FAILURE);
    }
    
    
    nof_worker_seats=atoi(getenv("NOF_WORKER_SEATS"));

    counter** desks=malloc(nof_worker_seats*sizeof(counter));
    if(desks==NULL)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

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
    //initialize itself: create message queue between ticket dispenser and the users
    int m_id_users=0;
    if((m_id_users=msgget(TICKETS_KEY,0666|IPC_CREAT))==-1)
    {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }    
    while(simulate)
    {
        day_finished=false;
        if(releaseSem(semid,1)==-1)
        {
            perror("reserve sem failed in ticket dispenser");
            exit(EXIT_FAILURE);
        }
        //wait for director to increment semaphore to start the simulation
        if((reserveSem(semid, 0)==-1))
        {
            if(simulate)
            {
                perror("semaphore reserve failed in ticket dispenser");
                exit(EXIT_FAILURE);
            }
            else
            {
                break;
            }
        }
        
        dispense(m_id_users,message,desks);
        if(reserveSem(semid, 2)==-1)
        {
            if(day_finished)
            {
                printf("Ticket dispenser %d day finished\n", getpid());
                break;
            }
            else
            {
                perror("semaphore reserve failed in ticket dispenser");
                exit(EXIT_FAILURE);
            }
        }
        clearServiceQueue(m_id_users);
    }
    free(desks);
}