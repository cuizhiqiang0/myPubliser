#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>  
#include <sys/time.h> // 包含setitimer()函数
#include <sys/resource.h>
#include <signal.h>  //包含signal()函数
#include <fstream> 
#include <iostream>
using namespace std;

#define PORT    9255
#define BACKLOG   100
#define BUF_EV_LEN 150
#define BUFFER_SIZE  512      
#define MAX_EPOLL_FD 8000
#define HEART_INTERVAL 60
#define HEART_BUFFER_SIZE 11
#define HEART_PROTOCOL "live"
#define BASH_RESULT_SIZE 1000
#define BASH_REPLY_SIZE  1500

#if 0
static char *policyXML="<cross-domain-policy><allow-access-from domain=/"*/" to-ports=/"*/"/></cross-domain-policy>";
static char *policeRequestStr="<policy-file-request/>";
#endif
bool firsttime = true;
char md5[33];
unsigned char filehead[512];
char filename[BUFFER_SIZE];
long filesize;
unsigned char *file;
int CreateTcpListenSocket();
int InitEpollFd();
void UseConnectFd(int sockfd);
void setnonblocking(int sock);
int sendMsg(int fd,char *msg, int len);
int run();

static int listenfd;
int gport;

void timerfunc(int param)
{
    static int count = 0;

    printf("count is %d\n", count++);
}
#if 0
void init_sigaction()
{
    struct sigaction act;
          
    act.sa_handler = timerfunc; //设置处理信号的函数
    act.sa_flags  = 0;

    sigemptyset(&act.sa_mask);
    sigaction(SIGPROF, &act, NULL);//时间到发送SIGROF信号
}

void init_time()
{
    struct itimerval val;
         
    val.it_value.tv_sec = HEART_INTERVAL; //1秒后启用定时器
    val.it_value.tv_usec = 0;

    val.it_interval = val.it_value; //定时器间隔为1s

    setitimer(ITIMER_PROF, &val, NULL);
}
#endif
static struct itimerval oldtv;
void set_timer()  
{  
    struct itimerval itv;  
    itv.it_interval.tv_sec = HEART_INTERVAL;  //设置为1秒
    itv.it_interval.tv_usec = 0;  
    itv.it_value.tv_sec = 1;  
    itv.it_value.tv_usec = 0;  
    setitimer(ITIMER_REAL, &itv, &oldtv);  //此函数为linux的api,不是c的标准库函数
}  

void signal_handler(int param)  
{  
    struct sockaddr_in server_addr;
    struct sockaddr_in connAddr;
    socklen_t len = sizeof(connAddr);
    int client_socketfd = 0;
    int ret = 0;
    unsigned char heartbuf[HEART_BUFFER_SIZE]; 

    memset(&server_addr, 0, sizeof(server_addr));

    client_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socketfd < 0)
    {
        printf("Create socket fd failed！\n"); 
    }

    /*fix-me通过L5获得一个最佳的ip*/
    server_addr.sin_family  = AF_INET;
    //inet_pton(AF_INET, IP, (void *)server_addr.sin_addr.s_addr);
    server_addr.sin_addr.s_addr = inet_addr("10.242.170.126");
    server_addr.sin_port = htons(9248);

    if (connect(client_socketfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Connect to server failed! \n"); 
    }

    heartbuf[0] = 0xFF;
    heartbuf[1] = 0xEE;
    heartbuf[2] = 0x11;
    heartbuf[3] = (unsigned int)((gport >> 8) & 0xFF);
    heartbuf[4] = (unsigned int)(gport & 0xFF);
    memcpy(heartbuf + 5, HEART_PROTOCOL, strlen(HEART_PROTOCOL));
    if (0 >= send(client_socketfd,heartbuf, BUFFER_SIZE, 0))
    {
        /*fix-me,这里如果发送失败了，就代表服务端挂掉了，这里只需要重启就行了*/
        printf("sendfailed:\n");
    }

    close(client_socketfd);
}  
#if 0
bool getAvaliablePort(unsigned short &port)
{
    bool result = true;

    // 1. 创建一个socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    // 2. 创建一个sockaddr，并将它的端口号设为0
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ADDR_ANY);
    addr.sin_port = 0;        // 若port指定为0,则调用bind时，系统会为其指定一个可用的端口号

    // 3. 绑定
    int ret = bind(sock, (SOCKADDR*)&addr, sizeof addr);

    if (0 != ret)
    {
        result = false;
        goto END;
    }

    // 4. 利用getsockname获取
    struct sockaddr_in connAddr;
    int len = sizeof connAddr;
    ret = getsockname(sock, (SOCKADDR*)&connAddr, &len);
    if (0 != ret)
    {
        result = false;
        goto END;
    }

    port = ntohs(connAddr.sin_port); // 获取端口号

END:
    if ( 0 != closesocket(sock) )
        result = false;
    return result;
}
#endif
int main()
{
    signal(SIGPIPE, SIG_IGN);
    /**/pid_t pid;

    signal(SIGALRM, signal_handler);  //注册当接收到SIGALRM时会发生是么函数；
    set_timer();  //启动定时器

    if((pid = fork()) < 0){
        printf("End at: %d",__LINE__);
        exit(-1);
    }
 
    if (pid){
        printf("End at: %d",__LINE__);
        //exit(0);
    }

    run();
}

int run()
{
    int epoll_fd;
    int nfds;
    int i;
    struct epoll_event events[BUF_EV_LEN];
    struct epoll_event tempEvent;
    int sockConnect;
    struct sockaddr_in remoteAddr;
    socklen_t addrLen;    

    addrLen = sizeof(struct sockaddr_in);
    
    epoll_fd = InitEpollFd();
    if (epoll_fd == -1)
    {
        perror("init epoll fd error.");
        printf("End at: %d\n",__LINE__);
        exit(1);
    }

    printf("begin in loop.\n");
    while (1)
    {
        nfds = epoll_wait(epoll_fd, events, BUF_EV_LEN, 1000);
        //sleep(3);
        if(nfds>5) printf("connect num: %d\n", nfds);
        if (nfds == -1)
        {
            if (errno = EINTR)
            {
                continue;
            }
            printf("End at: %d\n",__LINE__);
            perror("epoll_wait error.");
            continue;
        }
        for (i = 0; i < nfds; i++)
        {
            if (listenfd == events[i].data.fd)
            {
                //printf("connected success/n");
                sockConnect = accept(events[i].data.fd, (struct sockaddr*)&remoteAddr, &addrLen);
                if (sockConnect == -1)
                {
                    printf("End at: %d\n",__LINE__);
                    perror("accept error.");
                    continue;
                }
                setnonblocking(sockConnect);
                tempEvent.events = EPOLLIN | EPOLLET;
                tempEvent.data.fd = sockConnect;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockConnect, &tempEvent) < 0)
                {
                    perror("epoll ctl error.");
                    printf("End at: %d\n",__LINE__);
                    return -1;
                }
            }
            else
            {
                UseConnectFd(events[i].data.fd);
            }
        }
    }

    printf("---------------------------------/n/n");

}

int CreateTcpListenSocket()
{
    int sockfd;
    int port;
    int ret = 0;
    struct sockaddr_in localAddr;
    struct sockaddr_in connAddr;
    socklen_t len = sizeof(connAddr);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("create socket fail");
        printf("End at: %d\n",__LINE__);
        return -1;
    }

    setnonblocking(sockfd);
    
    bzero(&localAddr, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    //localAddr.sin_port = htons(PORT);
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    unsigned int optval;    
    //设置SO_REUSEADDR选项(服务器快速重起)
    optval = 0x1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));
    int nRecvBuf=320*1024;//设置为320K
    setsockopt(sockfd ,SOL_SOCKET, SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));
    /*
    //设置SO_LINGER选项(防范CLOSE_WAIT挂住所有套接字)
    optval1.l_onoff = 0;
    optval1.l_linger = 1;
    setsockopt(listener, SOL_SOCKET, SO_LINGER, &optval1, sizeof(struct linger));

    int nRecvBuf=320*1024;//设置为320K
    setsockopt(listener ,SOL_SOCKET, SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));

    int nSendBuf=1024*1024;//设置为640K
    setsockopt(listener ,SOL_SOCKET, SO_SNDBUF,(const char*)&nSendBuf,sizeof(int));
    */
    if (bind(sockfd,  (struct sockaddr*)&localAddr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind error");
        printf("End at: %d\n",__LINE__);
        return -1;
    }
    
    ret = getsockname(sockfd, (struct sockaddr*)&connAddr, &len);
    port = ntohs(connAddr.sin_port); // 获取端口号
    printf("global port:<%d>\n", port);
    gport = port;

    
    
    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen error");
        printf("End at: %d\n",__LINE__);
        return -1;
    }
    
    return sockfd;
}

int InitEpollFd()
{
    struct rlimit rt;
    rt.rlim_max = rt.rlim_cur = MAX_EPOLL_FD;

    if (setrlimit(RLIMIT_NOFILE, &rt) == -1)
    {
        //perror("setrlimit");
        printf("<br/>     RLIMIT_NOFILE set FAILED: %s     <br/>",strerror(errno));
        //exit(1);
    }
    else 
    {
        printf("设置系统资源参数成功！\n");
    }
    
    //epoll descriptor
    int s_epfd;
    struct epoll_event ev;
    
    
    listenfd = CreateTcpListenSocket();
    
    if (listenfd == -1)
    {
        perror("create tcp listen socket error");
        printf("End at: %d\n",__LINE__);
        return -1;
    }
    
    s_epfd = epoll_create(MAX_EPOLL_FD);
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    if (epoll_ctl(s_epfd, EPOLL_CTL_ADD, listenfd, &ev) < 0)
    {
        perror("epol    l ctl error");
        printf("End at: %d\n",__LINE__);
        return -1;
    }
    
    return s_epfd;
}
void execute_bash(char *buf, char *resultbuf)
{
    char *line = NULL;
    FILE *fp;
    int i = 0;
    int len = 0;
    int count  = 0;

    line = new char[BASH_RESULT_SIZE];
    
    if ((fp = popen(buf, "r")) == NULL) {
        cout << "error" << endl;
        return;
    }
    memset(line, 0, sizeof(line));
    while (fgets(line, sizeof(line)-1, fp) != NULL){
        i = strlen(line);
        len = ((BASH_RESULT_SIZE - count - i) > i) ? i : (BASH_RESULT_SIZE - count -i);
        if (len <= 0)
        {
            pclose(fp);
            delete []line;
            return;
        } 
        memcpy(resultbuf + count, line, i);
        count = count + i ;
        memset(line, 0, sizeof(line));
    }
    pclose(fp);
    delete []line;
}

void bash_stringproc(int sockfd, unsigned char *recvBuff)    
{
    char *resultbuf = NULL;
    char          *buf = NULL;
    char          *replybuf = NULL;
    int bash_len, result_len;
    
    buf = new char [BUFFER_SIZE];
    resultbuf = new char [BASH_RESULT_SIZE];
    replybuf = new char[BASH_REPLY_SIZE];

    memset(buf, 0, BUFFER_SIZE);
    memset(resultbuf, 0, BASH_RESULT_SIZE);
    memset(replybuf, 0, BASH_REPLY_SIZE);
    
    bash_len = (int)(recvBuff[3] & 0xFF);
    memcpy(buf, recvBuff + 4, bash_len);
    printf("bash_stringproc: execute bash:<%s>\n", buf);

    execute_bash(buf, resultbuf);
    result_len = strlen(resultbuf);
    printf("result:<%s>\n", resultbuf);
    replybuf[0] = 0xFF;
    replybuf[1] = 0xEE;
    replybuf[2] = 0x14;
    replybuf[3] = (int)((result_len >> 8) & 0xFF);
    replybuf[4] = (int)(result_len & 0xFF);
    memcpy(replybuf + 5, resultbuf, result_len);
    
    if (0 >= sendMsg(sockfd,replybuf, 5 + result_len))
    {
        /*fix-me,这里如果发送失败了，就代表服务端挂掉了，这里只需要重启就行了*/
        printf("sendfailed:\n");
    }
    delete []buf;
    delete []resultbuf;
    delete []replybuf;
    return;
}

void UseConnectFd(int sockfd)
{
    unsigned char recvBuff[512];
    char md5file[33] = {0};
    char md5string[50] = {0};
    char rmstr[50] = {0};
    int recvNum = 0;
    int filenamelen, i, type;
    bool continuerecv = false;
    char replybuf[60] = {0};  
    long  recvtotal = 0;
    int total = 0;
    bool headcon = false;
    ofstream log;
    //log.open("dou.txt", ios::app);
    bzero(recvBuff, sizeof(recvBuff));  

    while(1)
    {
        if (firsttime)
        {
            //printf("head recv\n");
            memset(recvBuff,'0', 512);
            recvNum = recv(sockfd, recvBuff, 512, MSG_DONTWAIT); 
            if ( recvNum < 0) 
            {
                //printf("errno:<%d>", errno);
                if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
                {//ETIMEDOUT可能导致SIGPIPE
                    continue;
                }
                else
                {
                    printf("errno<%d>\n", errno);
                    break;
                }
            } 
            else if (recvNum == 0) 
            {
                close(sockfd);
                return;
            }
         
           
            printf("firstime recvnum:<%d>\n", recvNum);
            
            recvtotal = 0;

            type = (int)(recvBuff[2] & 0x0F);
            
            /*执行bash命令*/
            if (type == 0xB)
            {
                bash_stringproc(sockfd, recvBuff);
                return ;
            }
            
            /*下发文件的一种，这几种他们的头部都是一样的*/
            filenamelen = (int)(recvBuff[3] & 0xFF);
            printf("head type<%x>, namelen<%d>\n", type, filenamelen);
            for(i = 0; i<15; i++)printf("<%x>", recvBuff[i]);
            
            memset(filename, 0, BUFFER_SIZE);
            memcpy(filename, recvBuff + 4, filenamelen);
            sprintf(rmstr, "%s %s", "rm", filename);
            system(rmstr);
            /*文件下发*/
            if (type == 0xA || type == 0x8 || type == 0x9)
            {
                /*头部*/
                printf("file head, filename<%s>", filename);
                filesize = (long)(recvBuff[4 + filenamelen] << 24) + (long)(recvBuff[5 + filenamelen] << 16) + (long)(recvBuff[6 + filenamelen] << 8) + (long)(recvBuff[7+filenamelen] & 0xFF);
                memset(md5, 0, sizeof(md5));
                memcpy(md5, recvBuff + 8 + filenamelen, 32);
                printf("filesize<%ld>, md5<%s>\n", filesize, md5);
                file = new unsigned char [filesize + BUFFER_SIZE];
                firsttime = false;
                
            }
            else
            {
                printf("UseConnectFd type error<%d>\n", type);
                return;
            }
            break;
        }
        else
        {
            printf("body recv, file<%x>\n", file);
            //memset(file,'0',filesize + 512);
            memset(file, 0, filesize + BUFFER_SIZE);
            
            recvNum = recv(sockfd, file, filesize + BUFFER_SIZE, MSG_DONTWAIT);
            if ( recvNum < 0) 
            {
                printf("num < 0, errno<%d>\n", errno);
                if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
                {//ETIMEDOUT可能导致SIGPIPE
                
                    continue;
                }
                else
                {
                    printf("errno<%d>\n", errno);
                    break;
                }
            } 
            else if (recvNum == 0) 
            {
                printf("recv num = 0\n");
                close(sockfd);
                break;
            }

            recvtotal += recvNum;
            printf("recv num<%d>, total<%d>\n", recvNum, recvtotal);
            
            if (!continuerecv)
            {
                type = (int)(file[2] & 0x0F);
                filenamelen = (int)(file[3] & 0xFF);
                printf("body type<%d>, namelen<%d>", type, filenamelen);
                for(i = 0; i < 15; i++)
                {
                    printf("<%x>", file[i]);
                }
                memset(filename, 0, BUFFER_SIZE);
                memcpy(filename, file + 4, filenamelen);
                if (type == 0xE || type == 0xD || type == 0xC)
                {
                    /*头部*/
                    printf("file head, filename<%s>", filename);
                    filesize = (long)(file[4 + filenamelen] << 24) + (long)(file[5 + filenamelen] << 16) + (long)(file[6 + filenamelen] << 8) + (long)(file[7+filenamelen] & 0xFF);
                    printf("filesize<%ld>\n", filesize);
                    char *tmp = new char [filesize];
                    memcpy(tmp, file + 8 + filenamelen, filesize);
                    log.open(filename, ios::app);
                    log << tmp;
                    log.close();
                    delete []tmp;
                    if (recvtotal < filesize + BUFFER_SIZE)
                    {
                        continuerecv = true;
                        firsttime = false;
                        continue;
                    }
                    /*一次接受完成*/
                    else
                    {
                        firsttime = true;
                    }
                }
                else
                {
                    printf("UseConnectFd recv type error<%d>\n", type);
                    return;
                }
            }
            else
            {
                log.open(filename, ios::app);
                log << file;
                log.close();
                if (recvtotal < filesize + BUFFER_SIZE)
                {
                    continuerecv = true;
                    firsttime = false;
                    continue;
                }
                else 
                { 
                    continuerecv = false;
                    firsttime = true;
                }
            }
            delete []file;
            break;
        }
    }

    sprintf(md5string, "%s %s", "md5sum", filename);
    execute_bash(md5string, md5file);
    printf("md5:<%s>, md5file<%s>\n", md5, md5file);
    /*文件下发,比对md5*/
    if (type == 0xE)
    {
        replybuf[0] = 0xFF;
        replybuf[1] = 0xEE;
        replybuf[2] = 0x15;
        replybuf[3] = (int)(filenamelen & 0xFF);
        memcpy(replybuf + 4, filename, filenamelen);
        if (0 == memcmp(md5, md5file, 32))
        {
            replybuf[4 + filenamelen] = 0x1; 
            printf("FILE PUBLISH SUCCESS!\n");
        }
        else
        {
            replybuf[4 + filenamelen] = 0x2; 
            printf("FILE PUBLISH FAILED!\n");
        }
       
        sendMsg(sockfd, replybuf, 60);
    } 
    /*配置更新，比对md5，更新*/
    else if (type == 0xD)
    {
        replybuf[0] = 0xFF;
        replybuf[1] = 0xEE;
        replybuf[2] = 0x16;
        replybuf[3] = (int)(filenamelen & 0xFF);
        memcpy(replybuf + 4, filename, filenamelen);
        if (0 == memcmp(md5, md5file, 32))
        {
            replybuf[4 + filenamelen] = 0x1; 
            printf("CONFIG PUBLISH UPDATE SUCCESS!\n");
        }
        else
        {
            replybuf[4 + filenamelen] = 0x2;
            printf("CONFIG PUBLISH UPDATE FAILED!\n");
        }
       
        sendMsg(sockfd, replybuf, 60);
        printf("send success\n");
    }
    /*脚本执行，比对md5，执行*/
    else if (type == 0xC)
    {
        replybuf[0] = 0xFF;
        replybuf[1] = 0xEE;
        replybuf[2] = 0x17;
        replybuf[3] = (int)(filenamelen & 0xFF);
        memcpy(replybuf + 4, filename, filenamelen);
        if (0 == memcmp(md5, md5file, 32))
        {
            replybuf[4 + filenamelen] = 0x1; 
            printf("CONFIG PUBLISH UPDATE SUCCESS!\n");
        }
        else
        {
            replybuf[4 + filenamelen] = 0x2;
            printf("CONFIG PUBLISH UPDATE FAILED!\n");
        }
       
        sendMsg(sockfd, replybuf, 60);
    }

    //free(buff);
    //printf("message: %s /n", recvBuff);
}

void setnonblocking(int sock)
{
    int opts;    
    opts=fcntl(sock,F_GETFL);
    if(opts<0)    
    {    
        perror("fcntl(sock,GETFL)");
        printf("End at: %d\n",__LINE__);
        exit(1);
    }
    
    opts = opts|O_NONBLOCK;    
    if(fcntl(sock,F_SETFL,opts)<0)    
    {    
        perror("fcntl(sock,SETFL,opts)");
        printf("End at: %d\n",__LINE__);
        exit(1);    
    }    

}

//发送消息给某个连接
int sendMsg(int fd,char *msg, int len)
{
    if(fd<1) return 0;
    while(1){
        int l=send(fd,msg,len,MSG_DONTWAIT); 

        if(l<0){
            if(errno == EPIPE){
                printf(">Send pipe error: %d\n",errno);
                printf("%d will close and removed at line %d!\n",fd,__LINE__);
                printf(">Send pipe error, %d closed!\n",fd);
                return -1;
            }
            break;
        }
        if (l <= len) {
            //printf("消息'%s'发送失败！错误代码是%d，错误信息是'%s'/n", msg, errno, strerror(errno));
            //return -1;
            break;
        }
    }


    return 1;
}

