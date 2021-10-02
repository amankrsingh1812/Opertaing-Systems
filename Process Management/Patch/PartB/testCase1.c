#include "types.h"
#include "user.h"

// Loop "loopfac" number of times
void looper(int bt, int loopfac) {
  set_burst_time(bt);        

  if (loopfac>5) 
    loopfac = 5;

  for (volatile int l=0; l<loopfac; l++)
    for (volatile int i=0; i<100000000; i++)
      ;

  printf(1, "Exiting PID: %d\n", getpid());
  exit();
}

// User IO
void userIO(int bt)
{
  set_burst_time(bt);

  char buf[256];
  printf(1, "Enter for user IO:\n");
  int amt = read(0, buf, 256);
  buf[amt] = 0;
  // printf(1, "So you entered: %s", buf);

  printf(1, "Exiting PID: %d\n", getpid());
  exit();
}

// File IO
void fileIO(int bt, int readBytes, char filename[])
{
  set_burst_time(bt);

  int fd = open(filename, 0);
  char buf[readBytes];
  int _read = read(fd, buf, readBytes);
  if (_read == readBytes) _read--;
  buf[_read] = 0;
  // printf(1, "1500 Words of README: \n%s\n---------\n", buf);

  printf(1, "Exiting PID: %d\n", getpid());
  exit();
}


int main(int argc, char *argv[])
{
  int pid[6];
  int rpid[6];
  char exitInfoGatherer[6][70];

  if ( !(pid[0] = fork()) )  looper(8,2);
  if ( !(pid[1] = fork()) )  userIO(1);
  if ( !(pid[2] = fork()) )  looper(10,4);
  if ( !(pid[3] = fork()) )  fileIO(6, 1500, "README");
  if ( !(pid[4] = fork()) )  looper(5,1);  
  if ( !(pid[5] = fork()) )  fileIO(3, 500, "README2");

  for (int i=0; i<6; i++)
    rpid[i] = wait();

  strcpy(exitInfoGatherer[0], "BurstTime: 8  - Empty loop running 2e8 times\t\t");
  strcpy(exitInfoGatherer[1], "BurstTime: 1  - Taking user IO and printing it\t\t");
  strcpy(exitInfoGatherer[2], "BurstTime: 10 - Empty loop running 4e8 times\t\t");
  strcpy(exitInfoGatherer[3], "BurstTime: 6  - Reading 1500 Bytes from \"README\"\t");
  strcpy(exitInfoGatherer[4], "BurstTime: 5  - Empty loop running 1e8 times\t\t");
  strcpy(exitInfoGatherer[5], "BurstTime: 3  - Reading 500 Bytes from \"README2\"\t");

  int outfd = 1;
  printf(outfd, "\n******** CHILDREN EXIT ORDER SUMMARY ********\n");
  for (int i=0; i<6; i++) {
    for (int j=0; j<6; j++) {
      if (rpid[i] == pid[j]) {
        printf(outfd, exitInfoGatherer[j]);
        printf(outfd, "PID: %d\n", rpid[i]);
      }
    }
  }
  printf(outfd, "\n****** Summary ends, completing parent *******\n\n");

	exit();
}

