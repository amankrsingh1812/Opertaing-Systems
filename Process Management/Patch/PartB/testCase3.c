#include "types.h"
#include "user.h"

int arrivalTime[5];

void fib(int idx,int bt,int N) {
  set_burst_time(bt); 
  int tick_start = uptime();       
  int fib[2] = {0, 1};

  for (volatile int i=0; i<N; i++) {
    if (i%2 == 0) {
        fib[0] = (fib[0] + fib[1]) % 1000000007;
    } else {
        fib[1] = (fib[0] + fib[1]) % 1000000007;
    }
  }

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

void looper2(int idx,int bt){
    set_burst_time(bt);
    int tick_start = uptime();       

    int N=1000;
    for (volatile int i = 0; i < N; i++) 
    { 
        for (volatile int j = 0; j < N; j++) 
        { 
            int res = 0; 
            for (volatile int k = 0; k < N; k++) 
                res += i*k+k*j;
        } 
    } 
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

void looper(int idx,int bt, int loopfac,int N) {
  set_burst_time(bt);        
  int tick_start = uptime();       

  if (loopfac>5) 
    loopfac = 5;

  for (volatile int l=0; l<loopfac; l++)
    for (volatile int i=0; i<N; i++)
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
  int pid[5];
  int rpid[5];
  char exitInfoGatherer[5][70];

  arrivalTime[0] = uptime();
  if ( !(pid[0] = fork()) )  fib(0,20,1000000000);
  arrivalTime [1] = uptime();
  if ( !(pid[1] = fork()) )  looper(1,10,8,10000000);
  arrivalTime [2] = uptime();
  if ( !(pid[2] = fork()) )  looper2(2,18);  
  arrivalTime [3] = uptime();
  if ( !(pid[3] = fork()) )  fib(3,12,500000000);
  arrivalTime [4] = uptime();
  if ( !(pid[4] = fork()) )  looper(4,19,2,500000000);

  for (int i=0; i<5; i++)
    rpid[i] = wait();

  strcpy(exitInfoGatherer[0], "BurstTime: 20  - Fibonacci loop running 1000000000 times  \t");
  strcpy(exitInfoGatherer[1], "BurstTime: 10   - 2 D loop running 8 X 10000000 times      \t");
  strcpy(exitInfoGatherer[2], "BurstTime: 18  - 3 D loop running 1000 X 1000 X 1000 times\t");
  strcpy(exitInfoGatherer[3], "BurstTime: 15   - Fibonacci loop running 500000000  times  \t");
  strcpy(exitInfoGatherer[4], "BurstTime: 19   -2 D loop running 2 X 500000000 times      \t");

  int outfd = 1;
  printf(outfd, "\n******** CHILDREN EXIT ORDER SUMMARY ********\n");
  for (int i=0; i<5; i++) {
    for (int j=0; j<5; j++) {
      if (rpid[i] == pid[j]) {
        printf(outfd, exitInfoGatherer[j]);
        printf(outfd, "PID: %d\n", rpid[i]);
      }
    }
  }
  printf(outfd, "\n****** Summary ends, completing parent *******\n\n");

  exit();
}

