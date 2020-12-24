#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

/*
    将路径格式化为文件名
*/
char * fmt_name(char * path) {
    static char buf[DIRSIZ + 1];
    char * p;
    // Find first character after last slash. 找到斜杠后的第一个单词
    for (p = path + strlen(path); p >= path && * p != '/'; p--) 
    ;
    p++;
    memmove(buf, p, strlen(p) + 1); //void* memmove(void *vdst, const void *vsrc, int n)p字符串move给buf
    return buf;
}
/*
    系统文件名与要查找的文件名，若一致，则打印系统文件完整路径
*/
void eq_print(char * fileName, char * findName) {
    if (strcmp(fmt_name(fileName), findName) == 0) { //字符串匹配，返回0表示匹配成功
        printf("%s\n", fileName);
    }
}
/*
    在某路径查找某文件
*/
void find(char * path, char * findName) {
    int fd;
    struct stat st;

    //参考自ls.c fd为打开文件的返回值 主要处理文件打开时的异常情况
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if (fstat(fd, & st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    char buf[512], * p;
    struct dirent de;

    switch (st.type) {
            //是一个文件
        case T_FILE:
            eq_print(path, findName);
            break;
            //是一个目录
        case T_DIR:
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path); //将路径复制进buf
            p = buf + strlen(buf);
            *p++ = '/';
            while (read(fd, & de, sizeof(de)) == sizeof(de)) {
                if (de.inum == 0 || de.inum == 1 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) // 避开文件名为.和..的文件
                    continue;
                memmove(p, de.name, strlen(de.name));
                p[strlen(de.name)] = 0;
                find(buf, findName); // 递归调用，顺着目录一级一级找下去
            }
            break;
    }
    close(fd);
}

int main(int argc, char * argv[]) {
    if (argc < 3) {
        printf("find:find <path> <fileName>\n");
        exit();
    }
    find(argv[1], argv[2]);
    exit();
}
