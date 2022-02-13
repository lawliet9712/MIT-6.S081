#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find_file(char *buf, char *p, char *search_path, char *search_file)
{
    int fd;
    struct stat st;
    struct dirent de;
    if ((fd = open(search_path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", search_path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", search_path);
        close(fd);
        return;
    }

    strcpy(buf, search_path);
    p = buf + strlen(buf);
    *p++ = '/';

    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        // 校验目录项是文件还是目录
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0)
        {
            fprintf(2, "ls: cannot stat %s\n", buf);
            continue;
        }

        // 如果是文件且名称符合搜索的文件
        if (st.type == T_FILE && !strcmp(de.name, search_file))
        {
            printf("%s\n", buf);
            *(p + 1) = 0;
            continue;
        }
        // 如果是目录，则继续递归下去
        else if(st.type == T_DIR)
        {
            if (de.inum == 0)
            {
                continue;
            }

            if (!strcmp(de.name, ".") || !strcmp(de.name, ".."))
            {
                continue;
            }

            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            find_file(buf, p, buf, search_file);
        }
    }
    close(fd);
}

void find(char *search_path, char *search_file)
{
    char buf[512], *p;
    p = buf;
    find_file(p, buf, search_path, search_file);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "find param invalid, format find [search path] [search file] \n");
        exit(0);
    }
    find(argv[1], argv[2]);
    exit(0);
}
