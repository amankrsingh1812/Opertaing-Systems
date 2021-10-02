#include "types.h"
#include "user.h"

int arrivalTime[2];

void childProcess(int idx,int bt) {
  set_burst_time(bt); 
  int fib[2] = {0, 1};

  for (volatile int i=0; i<300000000; i++) {
    if (i%2 == 0) {
        fib[0] = (fib[0] + fib[1]) % 1000000007;
    } else {
        fib[1] = (fib[0] + fib[1]) % 1000000007;
    }
  }

  int tick_end = uptime();
  printf(1, "Exiting PID: %d\tArrivalUptime: %d\tCompletionUptime: %d\tTurnaroundTime: %d\n"
  , getpid()
  , arrivalTime[idx]
  , tick_end
  , tick_end - arrivalTime[idx]
  );
  exit();
}


int main(int argc, char *argv[])
{
  int pid[2];
  int rpid[2];
  char exitInfoGatherer[2][70];

  arrivalTime[0] = uptime();
  if ( !(pid[0] = fork()) )  childProcess(0,20);
  arrivalTime[1] = uptime();
  if ( !(pid[1] = fork()) )  childProcess(1,5);

  for (int i=0; i<2; i++)
    rpid[i] = wait();

  strcpy(exitInfoGatherer[0], "BurstTime: 20  - Fibonacci loop running\t");
  strcpy(exitInfoGatherer[1], "BurstTime: 5   - Fibonacci loop running\t");

  int outfd = 1;
  printf(outfd, "\n******** CHILDREN EXIT ORDER SUMMARY ********\n");
  for (int i=0; i<2; i++) {
    for (int j=0; j<2; j++) {
      if (rpid[i] == pid[j]) {
        printf(outfd, exitInfoGatherer[j]);
        printf(outfd, "PID: %d\n", rpid[i]);
      }
    }
  }
  printf(outfd, "\n****** Summary ends, completing parent *******\n\n");

  exit();
}

