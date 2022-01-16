#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

char *readline()
{
    char *buf = malloc(100);
    char *p = buf;
    while (read(0, p, 1) != 0)
    {
        if (*p == '\n' || *p == '\0')
        {
            *p = '\0';
            return buf;
        }
        p++;
    }
    if (p != buf)
        return buf;
    free(buf);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: xargs [command]\n");
        exit(-1);
    }

    // 先复制原有的参数，argv 要 +1 是因为要跳过第一个参数 xargs
    char* line;
    char* nargv[MAXARG];
    char** pna = nargv;
    char** pa = ++argv;
    while(*pa != 0){
        *pna = *pa;
        pna++;
        pa++;
    }

    while ((line = readline()) != 0)
    {
        printf("read .. %s\n", line);
        char *pline = line;
        char *buf = malloc(36);
        char *pbuf = buf;
        int nargc = argc - 1;
        // 遍历该行每个字符
        while (*pline != 0)
        {
            // 遍历完一个参数
            if (*pline == ' ' && buf != pbuf)
            {
                *pbuf = 0;
                nargv[nargc] = buf;
                buf = malloc(36);
                pbuf = buf;
                nargc++;
            }
            // 单字符复制
            else if(*pline != ' ')
            {
                *pbuf = *pline;
                pbuf++;
            }
            pline++;
        }
        if (buf != pbuf)
        {
            nargv[nargc] = buf;
            nargc++;
        }
        nargv[nargc] = 0;
        free(line);
        int pid = fork();
        if (pid == 0)
        {
            printf("exec %s %s\n", nargv[0], nargv[1]);
            exec(nargv[0], nargv);
        }
        else
        {
            wait(0);
        }
    }
    exit(0);
}