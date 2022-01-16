#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if(argc < 1){
        fprintf(2, "Usage: sleep [seconds]...\n");
        exit(1);
    }

    int sleep_seconds = atoi(argv[1]);
    fprintf(2, "sleep second %d\n", sleep_seconds);
    if(sleep_seconds <= 0){
        fprintf(2, "sleep param invalid...\n");
        exit(1);
    }

    sleep(sleep_seconds);
    exit(0);
}
