#include "kernel/types.h"
#include "user/user.h"

int main(){
    int p1[2],p2[2];
    char buffer[] = {'X'};//来回传输的字符数组
    long length = sizeof(buffer);//传输字符数组的长度
    pipe(p1);//父进程写，子进程读
    pipe(p2);//父进程读，子进程写
    //子进程
    if (fork() == 0)
    {
        //关掉父进程写p1[1]和父进程读p2[0]
        close(p1[1]);
        close(p2[0]);
        //子进程从pipe1的读端，读出字符数组
        if (read(p1[0],buffer,length) != length)
        {
            printf("a--->b read error!");
            exit();
        }
        //打印读取到的字符数组
        printf("%d: received ping\n",getpid());
        //子进程向pipe2的写端，写入字符数组
        if (write(p2[1],buffer,length) != length)
        {
            printf("a<---b writh error!");
            exit();
        }
        exit();
    }
    //关掉子进程读p1[0]和子进程写p2[1]
    close(p1[0]);
    close(p2[1]);
    
    //父进程从pipe1的写端，写入字符数组
    if (write(p1[1],buffer,length) != length)
    {
        printf("a--->b write error!");
        exit();
    }
    //父进程从pipe2的读端，读出字符数组
    if (read(p2[0],buffer,length) != length)
    {
        printf("a<---b read error!");
        exit();
    }
    //打印读取到的字符数组
    printf("%d: received pong\n",getpid());
    //等待子进程退出
    wait();
    exit();

}