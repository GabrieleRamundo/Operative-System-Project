#include "library.h"


int main(int argc, char* argv[])
{
    
    if(argc!=2)
    {
        printf("Usage: %s <number of users>\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    int NEW_USERS=atoi(argv[1]);
    if(NEW_USERS<=0)
    {
        printf("Number of users must be greater than 0\n");
        exit(EXIT_FAILURE);
    }
    //create message
    struct mymsg message;
    //create message queue
    int m_id;
    if((m_id=msgget(ADD_USERS_KEY,0666|IPC_CREAT))==-1)
    {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }
    //send number of new users to director
    message.mtype=1;
    sprintf(message.mtext,"%d",NEW_USERS);

    if(msgsnd(m_id, &message, sizeof(message.mtext), 0) == -1)
    {
        perror("msgsnd failed");
        exit(EXIT_FAILURE);
    }

    
}