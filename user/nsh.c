#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"


#define MAXARGS 20
#define MAXWORD 30
char whitespace[] = " \t\r\n\v";
char args[MAXARGS][MAXWORD];
void runcmd(char *argv[],int argc);
//参考自sh.c文件中的panic、fork1和getcmd函数
//panic函数负责输出错误信息
void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(-1);
}
//fork1函数负责生成子进程，并在返回错误时打印错误信息
int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}
//getcmd函数负责打印"@"符号，让用户在该符号之后输入命令
int
getcmd(char *buf, int nbuf)
{
  fprintf(2, "@ ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

void
clearcmd(char* cmd, char* argv[],int* argc){
    //让argv的每一个元素都指向args的每一行
    for(int i=0; i<MAXARGS; i++){
        argv[i] = &args[i][0];
    }
    int i;//第i个word
    int j;
    for(i = 0,j = 0; cmd[j] != '\n' && cmd[j] != '\0'; j++){
        //每一轮循环都是寻找一行命令中的一个word
        //跳过之前的空格
        //strchr(char *str,int c),功能为在参数str所指向的字符串中搜索第一次出现字符c的位置
        //返回一个指向该字符串中第一次出现的字符的指针
        //若字符串中不包含该字符则返回NULL空指针
        while(strchr(whitespace,cmd[j])){
            j++;
        }
        //空格之后的命令存入argv
        argv[i++] = cmd+j;
        //寻找下一个空格
        //strchr函数返回0表示当前j指向的不是空格
        while(strchr(whitespace,cmd[j])==0){
            j++;
        }
        cmd[j] = '\0';//则argv能准确读取被两个空格围住的一个字符
    }
    argv[i] = 0;
    *argc = i;
}

void
myPipe(char *argv[],int argc){
  int i=0;
  //首先找到命令中的"|",然后把它换成"\0"
  //从前到后，找到第一个就停止，后面都递归调用
  for(;i<argc;i++){
    if(!strcmp(argv[i],"|")){
      argv[i]=0;
      break;
    }
  }
  //首先考虑最简单的情况:cat file | wc
  int fd[2];
  pipe(fd);
  if(fork1()==0){
    //子进程，执行左边的命令,关闭自己的标准输出，输出将用于之后的输入
    close(1);//先关闭标准输出文件显示器，以便后面调用
    dup(fd[1]);//获取文件描述符1,指向管道写入端
    close(fd[0]);//关闭管道读取端
    close(fd[1]);//关闭管道写入端
    runcmd(argv,i);
  }
  else{
    //父进程，执行右边的命令，关闭自己的标准输入，输入来自子进程的命令
    close(0);
    dup(fd[0]);//获取文件描述符0，即管道读入端指向输入
    close(fd[0]);
    close(fd[1]);
    runcmd(argv+i+1,argc-i-1);
  }
}

void 
runcmd(char *argv[],int argc){
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"|")){
      //如果遇到"|"即表示pipe命令，至少说明后面还有一个命令要执行
      myPipe(argv, argc);
    }
  }
  for(int i=1;i<argc;i++){
    //如果遇到">"，说明需要执行输出重定向，首先需要关闭stdout
    if(!strcmp(argv[i],">")){
      if(argc != i){
        close(1);//关闭stdout，即标准输出文件——输出至显示器
        //此时要把输出重定向到后面给出的文件名对应的文件里
        open(argv[i+1], O_CREATE|O_WRONLY);
        argv[i]=0;
      }else{
        fprintf(2,"%s\n","redirect error");
      }
    }
    if(!strcmp(argv[i],"<")){
      if(argc != i){
        //如果遇到"<",说明需要执行输入重定向，需要关闭stdin
        close(0);//关闭stdin，即标准输入文件——从键盘输入
        open(argv[i+1], O_RDONLY);
        argv[i]=0;
      }else{
        fprintf(2,"%s\n","redirect error");
      }

    }
  }
  exec(argv[0], argv);
}

//参考自sh.c中的main函数
int
main(void)
{
  static char buf[100];

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(fork1() == 0){
        char* argv[MAXARGS];
        int argc = -1;
        //重组参数格式
        clearcmd(buf, argv, &argc);
        runcmd(argv, argc);
    } 
    wait(0);
  }
  exit(0);
}