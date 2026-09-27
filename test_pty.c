#include <pty.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
int main(void){alarm(8);
 int master;pid_t p=forkpty(&master,NULL,NULL,NULL);if(p<0)return 1;
 if(p==0){execl("./cs","./cs",(char*)NULL);_exit(127);}
 char buf[8192]={0};size_t n=0;
 const char *cmd="ech\t\"pty-ok\"\nexit\n";
 usleep(200000);write(master,cmd,strlen(cmd));
 for(;;){ssize_t r=read(master,buf+n,sizeof(buf)-1-n);if(r<=0)break;n+=(size_t)r;buf[n]=0;if(strstr(buf,"pty-ok"))break;}
 int st;waitpid(p,&st,0);return WIFEXITED(st)&&WEXITSTATUS(st)==0&&strstr(buf,"pty-ok")?0:1;
}
