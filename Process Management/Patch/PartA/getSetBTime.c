#include "types.h"
#include "user.h"

int getBurstTimeInput() {
    printf(1, "Please enter a new burst time [expected range 1-20]: ");
    
    char buf[18];
    int amt = read(0, buf, 18);
    
    if (amt > 5) 
        return -1;
    
    buf[amt] = 0;
    for (int i=0; buf[i]&&buf[i]!='\n'; i++)
        if (buf[i]<'0' || buf[i]>'9')
            return -1;

    int bTime = 0;
    for (int i=0; buf[i]&&buf[i]!='\n'; i++)
        bTime = bTime*10 + (int)(buf[i]-'0');

    return bTime;
}

int main(int argc, char *argv[]){
    // Show original burst time
    printf(1, "Original burst time of this process: %d\n", get_burst_time());
    
    // Read and set new burst time
    int bTime = getBurstTimeInput();
    if (bTime < 0) {
        printf(1, "[-] Invalid burst time set. Aborting!!\n");
        exit();
    }
    set_burst_time(bTime);
    
    // Print new burst time
    printf(1, "New burst time of this process: %d\n", get_burst_time());
    exit();
}