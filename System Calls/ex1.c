//
// Simple inline assembly example
// 

#include <stdio.h>

int main()
{
    int x = 1;
    printf("Hello x = %d\n", x);


    asm ("incl %0\n\t":"+r"(x));


    printf("Hello x = %d after increment\n", x);

    if(x == 2){
        printf("OK\n");
    }
    else{
        printf("ERROR\n");
    }
}