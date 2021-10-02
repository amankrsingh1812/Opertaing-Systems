#include "types.h"
#include "user.h"

int arrivalTime[3];

// Loop "loopfac" number of times
void looper(int bt, int loopfac, int idx) {
  set_burst_time(bt);  
  int tick_start = uptime();  

  for (volatile int l=0; l<loopfac; l++)
    for (volatile int i=0; i<100000000; i++)
      ;

  int tick_end = uptime();
  printf(1, "Exiting PID: %d\tArrivalUptime: %d\tCompletionUptime: %d\tTurnaroundTime: %d\tResponseUptime: %d\n"
  , getpid()
  , arrivalTime[idx]
  , tick_end
  , tick_end - arrivalTime[idx]
  , tick_start - arrivalTime[idx]
  );
  exit();
}

int main(int argc, char *argv[])
{

  int pid[3];
  int rpid[3];

  char exitInfoGatherer[3][70];

  arrivalTime[0] = uptime();
  if ( !(pid[0] = fork()) )  looper(4,4,0);
  arrivalTime[1] = uptime();
  if ( !(pid[1] = fork()) )  looper(8,8,2);
  arrivalTime[2] = uptime();
  if ( !(pid[2] = fork()) )  looper(2,2,4); 


  for (int i=0; i<3; i++)
    rpid[i] = wait();

  strcpy(exitInfoGatherer[0], "BurstTime: 4   - Empty loop running 4e8 times\n");
  strcpy(exitInfoGatherer[1], "BurstTime: 8   - Empty loop running 8e8 times\n");
  strcpy(exitInfoGatherer[2], "BurstTime: 2   - Empty loop running 2e8 times\n");

  printf(1, "\n******** CHILDREN EXIT ORDER SUMMARY ********\n");
  for (int i=0; i<3; i++) {
    for (int j=0; j<3; j++) {
      if (rpid[i] == pid[j])
        printf(1, exitInfoGatherer[j]);
    }
  }
  printf(1, "\n****** Summary ends, completing parent *******\n\n");
  exit();
}


