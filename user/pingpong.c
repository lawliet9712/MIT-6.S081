#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int fd[2];
    char buf[8] = "hello\n";

    if (pipe(fd) == -1)
    {
        fprintf(2, "pipe failed ...\n");
        exit(1);
    }

    int pid = fork();
    if (pid < 0)
    {
        fprintf(2, "fork failed ... \n");
        exit(1);
    }
    // child process
    else if (pid == 0)
    {
        if (read(fd[0], buf, sizeof(buf)))
        {
            write(fd[1], buf, sizeof(buf));
            fprintf(2, "%d: received ping\n", getpid());
        }
    }
    // parent process
    else{
        write(fd[1], buf, sizeof(buf));
        sleep(10);
        if (read(fd[0], buf, sizeof(buf)))
        {
            fprintf(2, "%d: received pong\n", getpid());
        }
    }

    exit(0);
}
