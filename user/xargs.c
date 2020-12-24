#include "kernel/types.h"
#include "user/user.h"

//int型的argc，为整型，用来统计程序运行时发送给main函数的命令行参数的个数，在VS中默认值为1。
//char*型的argv[]，为字符串数组，用来存放指向的字符串参数的指针数组，每一个元素指向一个参数。各成员含义如下：
// argv[0]指向程序运行的全路径名
// argv[1]指向在DOS命令行中执行程序名后的第一个字符串
// argv[2]指向执行程序名后的第二个字符串
// argv[3]指向执行程序名后的第三个字符串
// argv[argc]为NULL
int main(int argc, char *argv[])
{
    int i, j, k, l, m;
    char block[32];
    char buf[32];
    char *p = buf;
    char *lineSplit[32]; //lineSplit是一个数组，数组长度为32位，每个位置存放的是一个字符型指针，指向一个字符串的首地址
    j = 0;
    for (i = 1; i < argc; i++)
    {
        lineSplit[j++] = argv[i]; //j++,先使用j值再+1
    }
    m = 0;
    while ((k = read(0, block, sizeof(block))) > 0)
    { //int read(int, void*, int),读取命令行输入内容
        for (l = 0; l < k; l++)
        { //k为命令行输入内容长度
            if (block[l] == '\n')
            {
                buf[m] = 0;
                m = 0;
                lineSplit[j++] = p;
                p = buf;
                lineSplit[j] = 0;
                j = argc - 1;
                if (fork() == 0)
                {
                    exec(argv[1], lineSplit); //exec功能：装入并运行其它程序的函数
                }
                wait();
            }
            else if (block[l] == ' ')
            {
                buf[m++] = 0;
                lineSplit[j++] = p;
                p = &buf[m];
            }
            else
            {
                buf[m++] = block[l]; //不是换行符也不是空字符，则加入buf字符数组
            }
        }
    }
    exit();
}