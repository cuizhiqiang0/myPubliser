#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>  
#include <sys/time.h> // 包含setitimer()函数
#include <sys/resource.h>
#include <signal.h>  //包含signal()函数
#include <pthread.h>
#include "execinfo.h"
#include <fstream> 
#include <iostream>


#include "qos_client.h"

using namespace std;
using namespace cl5;

#define PORT    9255
#define BACKLOG   100
#define BUF_EV_LEN 2
#define BUFFER_SIZE  512
#define FILE_NAME_SIZE 40
#define MAX_EPOLL_FD 2
#define HEART_INTERVAL 60
#define HEART_BUFFER_SIZE 11
#define HEART_PROTOCOL "live"
#define BASH_RESULT_SIZE 1000
#define BASH_REPLY_SIZE  1500
#define L5_MODID 64489857
#define L5_CMDID 65536
#define L5_TM_OUT 0.5;
#define SO_RCVBUF_SIZE 327680

bool firsttime = true;
unsigned char filehead[512];
char filename[FILE_NAME_SIZE];
unsigned char file[SO_RCVBUF_SIZE];
char serverip[20] ={0};
static int listenfd;
int gport;
static struct itimerval oldtv;

int CreateTcpListenSocket();
int InitEpollFd();
void UseConnectFd(int sockfd);
void setnonblocking(int sock);
int sendMsg(int fd,char *msg, int len);
int run();

void fun_dump( int no)
{
        char _signal[64][32] = {
"1: SIGHUP", "2: SIGINT", "3: SIGQUIT", "4: SIGILL",
"5: SIGTRAP", "6: SIGABRT", "7: SIGBUS", "8: SIGFPE",
"9: SIGKILL", "10: SIGUSR1", "11: SIGSEGV", "12: SIGUSR2",
"13: SIGPIPE", "14: SIGALRM", "15: SIGTERM", "16: SIGSTKFLT",
"17: SIGCHLD", "18: SIGCONT", "19: SIGSTOP", "20: SIGTSTP",
"21: SIGTTIN", "22: SIGTTOU", "23: SIGURG", "24: SIGXCPU",
"25: SIGXFSZ", "26: SIGVTALRM", "27: SIGPROF", "28: SIGWINCH",
"29: SIGIO", "30: SIGPWR", "31: SIGSYS", "34: SIGRTMIN",
"35: SIGRTMIN+1", "36: SIGRTMIN+2", "37: SIGRTMIN+3", "38: SIGRTMIN+4",
"39: SIGRTMIN+5", "40: SIGRTMIN+6", "41: SIGRTMIN+7", "42: SIGRTMIN+8",
"43: SIGRTMIN+9", "44: SIGRTMIN+10", "45: SIGRTMIN+11", "46: SIGRTMIN+12",
"47: SIGRTMIN+13", "48: SIGRTMIN+14", "49: SIGRTMIN+15", "50: SIGRTMAX-14",
"51: SIGRTMAX-13", "52: SIGRTMAX-12", "53: SIGRTMAX-11", "54: SIGRTMAX-10",
"55: SIGRTMAX-9", "56: SIGRTMAX-8", "57: SIGRTMAX-7", "58: SIGRTMAX-6",
"59: SIGRTMAX-5", "60: SIGRTMAX-4", "61: SIGRTMAX-3", "62: SIGRTMAX-2",
"63: SIGRTMAX-1", "64: SIGRTMAX" };

        void *stack_p[10];
        char **stack_info;
        int size;

        size = backtrace( stack_p, sizeof(stack_p));
        stack_info = backtrace_symbols( stack_p, size);

        if( no >= 1 && no <= 64)   
                printf("[%s] %d stack frames.\n", _signal[no-1], size);
        else
                printf("[No infomation %d] %d stack frames.\n", no, size);

        int i = 0;
        for( ; i < size; i++)
                printf("%s\n", stack_info[i]);

        free( stack_info);

        //free anything
        fflush(NULL);
        exit(0);
}

void myexit()
{
    printf("pid is existing : gport<%d>\n", gport);
}
    
void move_to_newdir()
{
    int status = 0;
    char buffer[10] = {0}; 
    
    sprintf(buffer, "%d", gport);
    status = mkdir(buffer, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (-1 == status)
    {
        cout << "mkdir failed! i:" << "errno:" << errno << endl;
        return;
    }
    status = chdir(buffer);
    if (-1 == status)
    {
        cout << "chdir failed" << endl;
        return;
    }
}
void set_timer()  
{  
    struct itimerval itv;  
    itv.it_interval.tv_sec = HEART_INTERVAL;  //设置为60秒
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
    QOSREQUEST qos_req;
    qos_req._modid = L5_MODID;
    qos_req._cmd = L5_CMDID;
    float tm_out = L5_TM_OUT;
    std::string err_msg;

    memset(&server_addr, 0, sizeof(server_addr));
    client_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socketfd < 0)
    {
        printf("Create socket fd failed！\n"); 
    }
    if (serverip[0] == '\0')
    {
        int iRet = ApiGetRoute(qos_req, tm_out, err_msg);
        if(iRet < 0)
        {
            cout << "iRet: " << iRet << endl;
            cout << "err msg: " << err_msg << endl;
            return;
        }
        strncpy(serverip, qos_req._host_ip.c_str(), strlen(qos_req._host_ip.c_str()));
    }
    Tryconnect:
    /*fix-me通过L5获得一个最佳的ip*/
    server_addr.sin_family  = AF_INET;
    //inet_pton(AF_INET, IP, (void *)server_addr.sin_addr.s_addr);
    server_addr.sin_addr.s_addr = inet_addr(serverip);
    server_addr.sin_port = htons(9248);

    if (connect(client_socketfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        ret = ApiRouteResultUpdate(qos_req, -1, tm_out, err_msg); 
        goto loop;
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
    
    ret = ApiRouteResultUpdate(qos_req, 0, tm_out,err_msg); 
    close(client_socketfd);
    return;
    
    loop:
    int iRet = ApiGetRoute(qos_req, tm_out, err_msg);
    if(iRet < 0)
    {
        cout << "iRet: " << iRet << endl;
        cout << "err msg: " << err_msg << endl;
        return;
    }
    strncpy(serverip, qos_req._host_ip.c_str(), strlen(qos_req._host_ip.c_str()));
    goto Tryconnect;
}  
void *hello(void *ptr)
{
    signal(SIGALRM, signal_handler);  //注册当接收到SIGALRM时会发生是么函数；
    set_timer();  //启动定时器
    while(1);
}
int main()
{
    signal(SIGPIPE, SIG_IGN);
    /**/pid_t pid;
    pthread_t t0;
    atexit(myexit);
    signal( SIGSEGV, fun_dump);
    
    if(pthread_create(&t0, NULL, hello, NULL) == -1){
            puts("fail to create pthread t0");
            exit(1);
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
    int nRecvBuf = SO_RCVBUF_SIZE;//设置为320K
    setsockopt(sockfd ,SOL_SOCKET, SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));
    
    int  flag = 1;
    ret = setsockopt( sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) );
    if (ret == -1) {
      printf("Couldn't setsockopt(TCP_NODELAY)\n");
      return -1;
    }
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
    move_to_newdir();
    return sockfd;
}

int InitEpollFd()
{
    #if 0
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
    #endif
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
    buf[bash_len] = '\0';
    //printf("bash_stringproc: execute bash:<%s>\n", buf);

    execute_bash(buf, resultbuf);
    result_len = strlen(resultbuf);
    //printf("result:<%s>\n", resultbuf);
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
void configupdate(char * filename)
{
    ifstream configFile;
    char mvhere[60] = {0};
    char local_File[60]  = {0};
    char rmdirbuf[60] = {0};
    string file_last;
    string str_line;
    string line_d;
    string zs("#");
    int filesize;
    char *filecon = NULL;
    sprintf(mvhere, "%s%s%s%s", "sudo cp /etc/", filename, " ./ETC_" , filename);
    system(mvhere);
    sprintf(local_File, "%s%s", "./ETC_", filename);
    configFile.open(filename);
  
    ifstream localfile(local_File, ios::in|ios::binary|ios::ate);
    filesize = localfile.tellg();
    filecon = new char[filesize];
    localfile.seekg(0, ios::beg); 
    localfile.read(filecon, filesize); 
    localfile.close(); 
    size_t total =0;
    string strfile(filecon);
    delete filecon;

    if (configFile.is_open())
    {
        while (!configFile.eof())
        {
            getline(configFile, str_line);
            
            if (0 != str_line.size())
            {
                if ( str_line.compare(0, 1, "#") == 0 ) //注释
                {
                    line_d.assign(str_line, 1, str_line.size() - 1);
                    size_t pos = line_d.find(' ');
                    string str_key = line_d.substr(0, pos);
                    string str_value = line_d.substr(pos + 1);
                    total = 0;
                    while (size_t q = strfile.find(str_key + " " + str_value, total))
                    {
                        if( q != string::npos )
                        {
                            total = q+1;
                            if (strfile[q-1] == '\n')
                            {
                                strfile.insert(q, zs);
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    
                    }
                    
                }
                else/*增加或者修改*/
                {
                    size_t pos = str_line.find(' ');
                    string str_key = str_line.substr(0, pos);
                    string str_value = str_line.substr(pos + 1);

                    total = 0;
                    while (size_t q = strfile.find(str_key + " ",total))
                    {
                        /*找到修改，找不到添加*/
                        if( q != string::npos )
                        {
                            total = q+1;
                            if (strfile[q-1] == '\n')
                            {
                                file_last.assign(strfile, q, strfile.size() - q);
                                
                                size_t f = file_last.find("\n");
                                string value = file_last.substr(str_key.size() ,f- str_key.size() -1);
                                
                                strfile.erase(q+str_key.size()+1, value.size());
                             
                                strfile.insert(q+str_key.size()+1, str_value);
                                break;
                            }
                            
                        }
                        else
                        {
                            
                            strfile += str_key + " " + str_value + "\n";
                            break;
                        }
                    }

                    
                }
            }   
        }
    }
    else
    {    
        cout << "Cannot open config file setting.ini, path: " << endl;

    }
    strfile += '\0';
    sprintf(rmdirbuf, "%s%s", "rm -f ", local_File);
    system(rmdirbuf);
    filesize = strfile.size();
    filecon = new char[filesize];
    strncpy(filecon, strfile.c_str(), filesize);
    ofstream local(local_File, ios::app | ios::binary);
    local << filecon;
    local.close();
    delete filecon;
}
#if 0
void UseConnectFd(int sockfd)
{
    unsigned char recvBuff[512];
    char md5file[33] = {0};
    char md5string[50] = {0};
    char rmstr[50] = {0};
    char codetran[70] = {0};
    int recvNum = 0;
    int filenamelen, i, type;
    bool continuerecv = false;
    char replybuf[60] = {0}; 
    char *errorbuf = NULL;
    char chmodbuf[60] = {0};
    char *resultbuf = NULL;
    char *result = NULL;
    long  recvtotal = 0;
    int total = 0;
    int result_len = 0;
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
         
           
            //printf("firstime recvnum:<%d>\n", recvNum);
            
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
            //printf("head type<%x>, namelen<%d>\n", type, filenamelen);
            //for(i = 0; i<15; i++)printf("<%x>", recvBuff[i]);
            
            memset(filename, 0, BUFFER_SIZE);
            memcpy(filename, recvBuff + 4, filenamelen);
            filename[filenamelen] = '\0';
            sprintf(rmstr, "%s %s", "rm -f ", filename);
            system(rmstr);
            /*文件下发*/
            if (type == 0xA || type == 0x8 || type == 0x9)
            {
                /*头部*/
                //printf("file head, filename<%s>", filename);
                filesize = (long)(recvBuff[4 + filenamelen] << 24) + (long)(recvBuff[5 + filenamelen] << 16) + (long)(recvBuff[6 + filenamelen] << 8) + (long)(recvBuff[7+filenamelen] & 0xFF);
                memset(md5, 0, sizeof(md5));
                memcpy(md5, recvBuff + 8 + filenamelen, 32);
                //printf("filesize<%ld>, md5<%s>\n", filesize, md5);
                newbufsize = (filesize + BUFFER_SIZE) > SO_RCVBUF_SIZE ? SO_RCVBUF_SIZE : (filesize + BUFFER_SIZE);
                //printf("newbufsize:<%d>\n", newbufsize);
                file = new unsigned char [newbufsize];
                firsttime = false;
            }
            else
            {
                errorbuf = new char[60];
                printf("UseConnectFd type error<%d>, gport<%d>\n", type, gport);
                printf("filenamelen<%d>, <filename<%s>>\n", filenamelen, filename);
                //for(i = 0; i<30; i++)printf("<%x>", recvBuff[i]);
                errorbuf[0] = 0xFF;
                errorbuf[1] = 0xEE;
                errorbuf[2] = 0x15;
                errorbuf[3] = (int)(filenamelen & 0xFF);
               
                memcpy(replybuf + 4, filename, filenamelen);
                errorbuf[4 + filenamelen] = 0x2;           
                sendMsg(sockfd, errorbuf, 60);
                printf("send success\n");
                delete []errorbuf;
                firsttime = true;
            }
            return;
        }
        else
        {
            //printf("body recv, file<%x>\n", file);
            //memset(file,'0',filesize + 512);
            memset(file, 0, newbufsize);
            
            recvNum = recv(sockfd, file, newbufsize, MSG_DONTWAIT);
            if ( recvNum < 0) 
            {
                //printf("num < 0, errno<%d>\n", errno);
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
    
            if (!continuerecv)
            {
                type = (int)(file[2] & 0x0F);
                filenamelen = (int)(file[3] & 0xFF);
                //printf("body type<%d>, namelen<%d>", type, filenamelen);
                #if 0
                for(i = 0; i < 30; i++)
                {
                    printf("<%x>", file[i]);
                }
                #endif
                memset(filename, 0, BUFFER_SIZE);
                memcpy(filename, file + 4, filenamelen);
                if (type == 0xE || type == 0xD || type == 0xC)
                {
                    /*头部*/
                    //printf("file body, filename<%s>", filename);
                    filesize = (long)(file[4 + filenamelen] << 24) + (long)(file[5 + filenamelen] << 16) + (long)(file[6 + filenamelen] << 8) + (long)(file[7+filenamelen] & 0xFF);
                    //printf("filesize<%ld>\n", filesize);
                    char *tmp = new char [newbufsize];
                    memset(tmp, 0, newbufsize);
                    memcpy(tmp, file + 8 + filenamelen, recvNum - 8 -filenamelen);
                    tmp[recvNum - 8 -filenamelen] = '\0';
                    //printf("tem<%d>, <%s>\n", strlen(tmp), tmp);
                    log.open(filename, ios::app | ios::binary);
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
                log.open(filename, ios::app | ios::binary);
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
    //printf("md5:<%s>, md5file<%s>\n", md5, md5file);
    /*文件下发,比对md5*/
    if (type == 0xE)
    {
        //printf("file publish\n");
        replybuf[0] = 0xFF;
        replybuf[1] = 0xEE;
        replybuf[2] = 0x15;
        replybuf[3] = (int)(filenamelen & 0xFF);
        //memcpy(replybuf + 4, filename, filenamelen);
        if (0 == memcmp(md5, md5file, 32))
        {   
            replybuf[4 + filenamelen] = 0x1; 
        }
        else
        {  
           resultbuf[4 + filenamelen] = 0x2;
           printf("FILE PUBLISH  FAILED!<%d>\n", gport);       
        }
   
        sendMsg(sockfd, replybuf, 60);
        return;
    } 
    /*配置更新，比对md5，更新*/
    else if (type == 0xD)
    {
        printf("config\n");
        replybuf[0] = 0xFF;
        replybuf[1] = 0xEE;
        replybuf[2] = 0x16;
        replybuf[3] = (int)(filenamelen & 0xFF);
        memcpy(replybuf + 4, filename, filenamelen);
        if (0 == memcmp(md5, md5file, 32))
        {
            replybuf[4 + filenamelen] = 0x1; 
        }
        else
        {
            resultbuf[4 + filenamelen] = 0x2;
            printf("CONFIG PUBLISH UPDATE FAILED!<%d>\n", gport);   
        }
       
        sendMsg(sockfd, replybuf, 60);
        return;
    }
    /*比对md5，脚本执行,shouji jieguo*/
    else if (type == 0xC)
    {
        printf("shell file\n");
        resultbuf = new char [1060];
        result = new char[1000];
        resultbuf[0] = 0xFF;
        resultbuf[1] = 0xEE;
        resultbuf[2] = 0x17;
        resultbuf[3] = (int)(filenamelen & 0xFF);
        memcpy(resultbuf + 4, filename, filenamelen);
        if (0 == memcmp(md5, md5file, 32))
        {
            resultbuf[4 + filenamelen] = 0x1; 
        }
        else
        {
            resultbuf[4 + filenamelen] = 0x2;
            printf("SHELL EXE FAILED!<%d>\n", gport);    
        }
       
        sprintf(chmodbuf, "%s%s", "chmod +x ./", filename);
        system(chmodbuf);
        memset(chmodbuf, 0 , sizeof(chmodbuf));
        sprintf(chmodbuf, "%s%s", "./", filename);
        execute_bash(chmodbuf, result);
        result_len = strlen(result);
        resultbuf[5 + filenamelen] = (int) ((result_len >> 8) & 0xFF);
        resultbuf[6 + filenamelen] = result_len & 0xFF;
        memcpy(resultbuf+7+filenamelen, result, result_len);
        sendMsg(sockfd, resultbuf, 7 + filenamelen + result_len);
        delete []resultbuf;
        delete []result;
    }

    //free(buff);
    //printf("message: %s /n", recvBuff);
}
#endif
void UseConnectFd(int sockfd)
{
    char md5file[33] = {0};
    char md5[33];
    char md5string[50] = {0};
    char rmstr[50] = {0};
    int recvNum = 0;
    int filenamelen, i, type;
    bool continuerecv = false;
    char replybuf[60] = {0}; 
    char *errorbuf = NULL;
    char chmodbuf[60] = {0};
    char *resultbuf = NULL;
    char *result = NULL;
    long  recvtotal = 0;
    long filesize = 0;
    long newbufsize = 0;
    int total = 0;
    int result_len = 0;
    bool headcon = false;
    char tmp[SO_RCVBUF_SIZE] = {0};
    ofstream log;
  
    while(1)
    {
        
        memset(file, 0, SO_RCVBUF_SIZE);
        recvNum = recv(sockfd, file, SO_RCVBUF_SIZE, MSG_DONTWAIT);
        if ( recvNum < 0) 
        {
            //printf("num < 0, errno<%d>\n", errno);
            if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
            {//ETIMEDOUT可能导致SIGPIPE
                continue;
            }
            else
            {
                printf("errno<%d>\n", errno);
                return;
            }
        } 
        else if (recvNum == 0) 
        {
            close(sockfd);
            return;
        }
        recvtotal += recvNum;
        
        if (!continuerecv)
        {
            type = (int)(file[2] & 0x0F);
            filenamelen = (int)(file[3] & 0xFF);
            //printf("body type<%d>, namelen<%d>", type, filenamelen);
            #if 0
            for(i = 0; i < 30; i++)
            {
                printf("<%x>", file[i]);
            }
            #endif
            if (type == 0xB)
            {
                bash_stringproc(sockfd, file);
                return ;
            }
            
           
            memset(filename, 0, FILE_NAME_SIZE);
            memcpy(filename, file + 4, filenamelen);
            file[filenamelen] = '\0';
            sprintf(rmstr, "%s %s", "rm -f ", filename);
            system(rmstr);
            if (type == 0xE || type == 0xD || type == 0xC)
            {
                /*头部*/
                //printf("tem<%d>, <%s>\n", strlen(tmp), tmp);
                memset(md5, 0, 33);
                memcpy(md5, file + 8 + filenamelen, 32);
                md5[32] = '\0';
                
                filesize = (long)(file[4 + filenamelen] << 24) + (long)(file[5 + filenamelen] << 16) + (long)(file[6 + filenamelen] << 8) + (long)(file[7+filenamelen] & 0xFF);
                //printf("filesize<%ld>\n", filesize);              
                //newbufsize = (filesize + BUFFER_SIZE) > SO_RCVBUF_SIZE ? SO_RCVBUF_SIZE : (filesize + BUFFER_SIZE);
                #if 0
                char *tmp = new char [newbufsize];
                memset(tmp, 0, newbufsize);
                memcpy(tmp, file + 40 + filenamelen, recvNum - 40 -filenamelen);
                tmp[recvNum - 40 - filenamelen] = '\0';
                #endif
                memcpy(tmp, file + 40 + filenamelen, recvNum - 40 -filenamelen);
                tmp[recvNum - 40 - filenamelen] = '\0';
                log.open(filename, ios::app | ios::binary);
                log << tmp;
                log.close();
                //delete []tmp;
                if (recvtotal < filesize + 40 + filenamelen)
                {
                    continuerecv = true;
                    firsttime = false;
                    continue;
                }
                /*一次接受完成*/
                else
                {
                    break;
                    firsttime = true;
                }
            }
            else
            {
                errorbuf = new char[60];
                for(i = 0; i < 30; i++)
                {
                    printf("<%x>", file[i]);
                }
                printf("UseConnectFd type error<%d>, gport<%d>\n", type, gport);
                printf("filenamelen<%d>, <filename<%s>>\n", filenamelen, filename);
                //for(i = 0; i<30; i++)printf("<%x>", recvBuff[i]);
                errorbuf[0] = 0xFF;
                errorbuf[1] = 0xEE;
                errorbuf[2] = 0x15;
                errorbuf[3] = (int)(filenamelen & 0xFF);
               
                memcpy(replybuf + 4, filename, filenamelen);
                errorbuf[4 + filenamelen] = 0x2;           
                sendMsg(sockfd, errorbuf, 60);
                printf("send success\n");
                delete []errorbuf;
                firsttime = true;
                return;
            }
        }
        else
        {
            memcpy(tmp, file, recvNum);
            tmp[recvNum] = '\0';
            log.open(filename, ios::app | ios::binary);
            log << tmp;
            log.close();
            if (recvtotal < filesize + 40 + filenamelen)
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
        //delete []file;
        break;
    }

    sprintf(md5string, "%s %s", "md5sum", filename);
    execute_bash(md5string, md5file);
    //printf("md5:<%s>, md5file<%s>\n", md5, md5file);
    /*文件下发,比对md5*/
    if (type == 0xE)
    {
        //printf("file publish\n");
        replybuf[0] = 0xFF;
        replybuf[1] = 0xEE;
        replybuf[2] = 0x15;
        replybuf[3] = (int)(filenamelen & 0xFF);
        memcpy(replybuf + 4, filename, filenamelen);
        if (0 == memcmp(md5, md5file, 32))
        {   
            replybuf[4 + filenamelen] = 0x1; 
            printf("FILE PUBLISH  SUCCESS!<%d>\n", gport);
        }
        else
        {  
           replybuf[4 + filenamelen] = 0x2;
           printf("FILE PUBLISH  FAILED!<%d>, recvtotal<%d>, filesize<%d>\n", gport, recvtotal, filesize);       
        }
   
        sendMsg(sockfd, replybuf, 60);
        return;
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
            configupdate(filename); 
            replybuf[4 + filenamelen] = 0x1;
            printf("CONFIG PUBLISH UPDATE SUCCESS!<%d>\n", gport);
        }
        else
        {
            replybuf[4 + filenamelen] = 0x2;
            printf("CONFIG PUBLISH UPDATE FAILED!<%d>\n", gport);   
        }
       
        sendMsg(sockfd, replybuf, 60);
        return;
    }
    /*比对md5，脚本执行,shouji jieguo*/
    else if (type == 0xC)
    {
        resultbuf = new char [1060];
        result = new char[1000];
        resultbuf[0] = 0xFF;
        resultbuf[1] = 0xEE;
        resultbuf[2] = 0x17;
        resultbuf[3] = (int)(filenamelen & 0xFF);
        memcpy(resultbuf + 4, filename, filenamelen);
        if (0 == memcmp(md5, md5file, 32))
        {
            resultbuf[4 + filenamelen] = 0x1; 
            printf("SHELL EXE SUCCESS!<%d>\n", gport);
        }
        else
        {
            resultbuf[4 + filenamelen] = 0x2;
            printf("SHELL EXE FAILED!<%d>\n", gport);    
        }
       
        sprintf(chmodbuf, "%s%s", "chmod +x ./", filename);
        system(chmodbuf);
        memset(chmodbuf, 0 , sizeof(chmodbuf));
        sprintf(chmodbuf, "%s%s", "./", filename);
        execute_bash(chmodbuf, result);
        result_len = strlen(result);
        printf("result<%s>result_len<%d>\n", result, result_len);
        resultbuf[5 + filenamelen] = (int) ((result_len >> 8) & 0xFF);
        resultbuf[6 + filenamelen] = result_len & 0xFF;
        memcpy(resultbuf+7+filenamelen, result, result_len);
        sendMsg(sockfd, resultbuf, 7 + filenamelen + result_len);
        delete []resultbuf;
        delete []result;
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

