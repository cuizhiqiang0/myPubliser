#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<iostream>
#include<signal.h>
#include <unistd.h>
#include <time.h>  
#include <sys/time.h> // 包含setitimer()函数

using namespace std;
static struct itimerval oldtv;
void signal_handler(int param)  
{
    system("sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
}

void set_timer()  
{  
    struct itimerval itv;  
    itv.it_interval.tv_sec = 100;  //设置为60秒
    itv.it_interval.tv_usec = 0;  
    itv.it_value.tv_sec = 1;  
    itv.it_value.tv_usec = 0;  
    setitimer(ITIMER_REAL, &itv, &oldtv);  //此函数为linux的api,不是c的标准库函数
}  

int main(int argc,char* argv[])
{
    int pidNum = 0;
    int i = 0;
    if (argc != 2)
    {
        cout << "input error" << endl;
        return 0;
    }
    sscanf(argv[1], "%d", &pidNum);

    struct sigaction sigchld_action;
    sigchld_action.sa_handler = SIG_DFL;
    sigchld_action.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sigchld_action, NULL);
    
    for (i = 0; i < pidNum; i++)
    {
        int m_subProcessID=fork();
        if (m_subProcessID < 0)
        {
            cout << "fork failed i:" << i << endl;
        } 
        else if (m_subProcessID == 0)
        {
            int ret=execl("./client", NULL);
            if(0!=ret)
            {
                printf("execl fails.\n");
                return -1;
            }
            return 0;
        }
    }

    signal(SIGALRM, signal_handler);  //注册当接收到SIGALRM时会发生是么函数；
    set_timer();  //启动定时器

    while(1);
    
    return 0;
}
