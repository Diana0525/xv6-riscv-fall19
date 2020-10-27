#include "kernel/types.h"
#include "user/user.h"

void func(int *input, int num){
    if (num == 1)
    {
        printf("prime %d\n",*input);
        return;
    }
    int p[2],i;
    int prime = *input;
    int temp;
    printf("prime %d\n",prime);
    pipe(p);//p[1]写入管道，p[0]读出管道
    if (fork() == 0)//子进程,将输入写入管道
    {
        for ( i = 0; i < num; i++)
        {
            temp = *(input+i);//指向下一个数字
            write(p[1],(char*)(&temp),4);
        }
        exit();
    }
    close(p[1]);
    if (fork() == 0)
    {
        int counter = 0;
        char buffer[4];
        while (read(p[0],buffer,4) != 0)
        {
            temp = *((int*)buffer);//强转为int型
            if (temp % prime != 0)
            {
                *input = temp;
                input++;
                counter++;
            }
        }
        func(input - counter,counter);//递归寻找素数
        exit();
    }
    wait();
    wait();//分别等两个子进程
}

int main(){
    int input[34];
    int i=0;
    for (;i < 34;i ++ ){
        input[i] = i+2;
    }
    func(input , 34);
    exit();
}