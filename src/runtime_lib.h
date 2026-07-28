#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//debug define
#define DEBUG if(errno){ fprinf(stderr, "%s: %d: PID=%5d: Error %d (%s)\n", __FILE__, __LINE__,getpid(), errno, strerror(errno));}

void simulate(int m_id, pid_t* pids, int semid, counter* seat_ptr[]);
pid_t* add_usr(pid_t* pids, struct mymsg message, int m_id, int semid);
void send_signals(pid_t* pids, int SIG);
void initialize_procs(int m_id, int semid, pid_t* pids);


