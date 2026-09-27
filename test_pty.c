#define _POSIX_C_SOURCE 200809L
#include <pty.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
int main(void){
 int master;pid_t p=forkpty(&master,NULL,NULL,NULL);if(p<0)return 1;
 if(p==0){execl("./cs","./cs",(char*)NULL);_exit(127);}
 const char *cmd="ech\t\"pty-ok\"\nexit\n";
 struct timespec ts={0,300000000};nanosleep(&ts,NULL);write(master,cmd,strlen(cmd));
 char buf[8192]={0};size_t n=0;bool ok=false;
 for(int i=0;i<50&&!ok;i++){struct pollfd q={master,POLLIN,0};int pr=poll(&q,1,100);if(pr>0&&q.revents&POLLIN){ssize_t r=read(master,buf+n,sizeof(buf)-1-n);if(r<=0)break;n+=(size_t)r;buf[n]=0;if(strstr(buf,"pty-ok"))ok=true;}}
 int st;kill(p,SIGTERM);waitpid(p,&st,0);close(master);
 if(!ok){fprintf(stderr,"PTY transcript:\n%s\n",buf);return 1;}
 return WIFEXITED(st)||WIFSIGNALED(st)?0:1;
}
