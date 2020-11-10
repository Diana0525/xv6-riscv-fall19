#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

//参考自sh.c文件中的panic、fork1和getcmd函数
void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(-1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

int
getcmd(char *buf, int nbuf)
{
  fprintf(2, "$ ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

void clearcmd(char* cmd, char* argv[],int* argc){
    for(int i=0; i<MAXARGS; i++){
        argv[i] = args[i];
    }
    int i;
    int j;
    for(i = 0,j = 0; cmd[j] != '\n' && cmd[j] != '\0'; j++){
        //跳过之前的空格
        while(strchr(whitespace,cmd[j])){
            j++;
        }
        //空格之后的命令存入argv
        argv[i++] = cmd+j;
        //寻找下一个空格
        while(strchr(whitespace,cmd[j])){
            j++;
        }
        cmd[j] = '\0';
    }
    argv[i] = 0;
    *argc = i;
}

void runcmd()
//参考自sh.c中的main函数
int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }
  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(fork1() == 0){
        char* argv[MAXARGS];
        int argc = -1;
        //重组参数格式
        clearcmd(buf, argv, &argc);
        runcmd(parsecmd(buf));
    } 
    wait(0);
  }
  exit(0);
}