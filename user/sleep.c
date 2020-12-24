#include "kernel/types.h"
#include "user/user.h"

int main(int argn, char * argv[]) {
    if (argn != 2) {
        fprintf(2, "must 1 argument for sleep\n");
        exit(); //异常退出
    }
    int sleepNum = atoi(argv[1]); //将传入字符型参数转换为int类型
    fprintf(2, "(nothing happens for a little while)\n");
    sleep(sleepNum);
    exit(); //正常退出
}