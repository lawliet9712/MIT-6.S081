#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int fd[2];
    if (pipe(fd) == -1)
    {
        fprintf(2, "pipe failed ...\n");
        exit(1);
    }

    int pid = 0;
    int i = 0;
    int cnt = 10;
    int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31};

    fprintf(2, "prime %d\n", primes[i]);

repeat:
    pid = fork();
    if (pid == 0)
    {
        if (read(fd[0], &i, sizeof(int)))
        {
            fprintf(2, "prime %d\n", primes[i]);
            if (i == cnt)
            {
                close(fd[1]);
                exit(0);
            }
            else
                goto repeat;
        }
    }
    else
    {
        i += 1;
        write(fd[1], &i, sizeof(int));
        wait(0);
    }

    exit(0);
}
