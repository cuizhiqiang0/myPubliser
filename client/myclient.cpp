#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <netinet/in.h>                         // for sockaddr_in  
#include <sys/types.h>                          // for socket  
#include <sys/stat.h>
#include <sys/wait.h> 
#include <sys/socket.h>                         // for socket    
#include <stdlib.h>    
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include <iomanip>
#include <errno.h>

using namespace std;

#define PTHREAD_NUM 1000    
#define PID_NUM PTHREAD_NUM
#define IP "10.123.5.46"
#define SERVER_PORT 9248

#define FILE_NAME_MAX_SIZE  512
#define BUFFER_SIZE  512
#define TYPE_BUFFER_SIZE 4
#define FILE_NAME_SIZE 4

int gfilename = 0;
int gconfig = 0;

enum COMMUNICATE_TYPE
{
    FILE_DOWN = 1,
    CONFIG_UPDATE,
    BASH_CMD,
    TYPE_MAX,
};  


void fileDownloadProc(int client_socketfd)
{
    char buffer[BUFFER_SIZE];
    char filename[FILE_NAME_SIZE];
    buffer[0] = '\0';
    filename[0] = '\0';
    int length = 0;
    int writelength = 0;
    FILE *fp = NULL;

    sprintf(filename, "%d", gfilename);
    fp = fopen(filename, "w");
    if (NULL == fp)
    {
        cout <<"filename:" << gfilename << "open failed! i: " << getpid() << endl;
        
        return;
    }
    while (length = recv(client_socketfd, buffer, BUFFER_SIZE, 0))
    {
        if (length < 0)
        {
            cout << "recv data failed! i:" << getpid() << endl;
            break; 
        }
        else
        {
            cout << buffer << endl;
        }
        writelength = fwrite(buffer, sizeof(char), length, fp);
        if (writelength < length)
        {
            cout << "write failed ! i:" << getpid() << "filename:" << gfilename << endl;
            break;
        }
           
        memset(buffer, 0, BUFFER_SIZE);
    }
    cout << "file recv finish! filename:" << filename << "i:" << getpid() << endl;

    return;
}

void configUpdateProc(int client_socketfd)
{
    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';
    int length = 0;

    while (length = recv(client_socketfd, buffer, BUFFER_SIZE, 0))
    {
        if (length < 0)
        {
            cout << "recv data failed! i:" << getpid() << endl;
            break; 
        }
        else
        {
            cout << buffer << endl;
        }
        
        sscanf(buffer, "%d", &gconfig);
        cout << "config update, gconfig:" << gconfig << endl;
           
        memset(buffer, 0, BUFFER_SIZE);
    }
    cout << "config update finish:" << getpid() << endl;

    return;

}

void bash_cmdProc(int client_socketfd)
{
    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';
    int length = 0;

    while (length = recv(client_socketfd, buffer, BUFFER_SIZE, 0))
    {
        if (length < 0)
        {
            cout << "recv data failed! i:" << getpid() << endl;
            break; 
        }
        else
        {
            cout << buffer << endl;
        }
        
        system(buffer);
        cout << "config update, gconfig:" << gconfig << endl;
           
        memset(buffer, 0, BUFFER_SIZE);
    }
    cout << "bash excute finish:" << getpid() << endl;
 
    return;
}

#if 0
void recv_reply(int client_socketfd)
{
    char type_buffer[TYPE_BUFFER_SIZE];   
    type_buffer[0] = '\0';
    int type = 0;

    while (true)
    {
        if (0 <= recv(client_socketfd, type_buffer, TYPE_BUFFER_SIZE, 0))
        {
            cout << "recv failed！i:" << getpid() << endl;
            return;   
        }
        cout << "type_buffer:" << type_buffer << endl;
        type = int(type_buffer[0] - '0');
        switch (type)
        {
            case FILE_DOWN:
            {
                fileDownloadProc(client_socketfd);
                break;
            }
            case CONFIG_UPDATE:
            {
                configUpdateProc(client_socketfd);
                break;
            }
            case BASH_CMD:
            {
                bash_cmdProc(client_socketfd);
                break;
            }
            default :
            {
                cout << "wrong type! i:" << getpid() << endl; 
            }
        }
        
    }
}
#endif
void mymkdir(int client_socketfd)
{   
    int status = 0;
    char buffer[BUFFER_SIZE];    
    buffer[0] = '\0';
    
    sprintf(buffer, "%d", client_socketfd);
    status = mkdir(buffer, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (-1 == status)
    {
        if (EEXIST != errno)
        {
            cout << "mkdir failed! i:" << getpid();
        }
    }

    
}

void mychdir(int client_socketfd)
{
    char dir[BUFFER_SIZE]; 
    int status = 0;
    dir[0] = '\0';

    sprintf(dir, "%d", client_socketfd);
    status = chdir(dir);
    if (-1 == status)
    {
        cout << "chdir failed" << endl;
        return;
    }
}


void recv_reply(int client_socketfd)
{
    char recvbuf[BUFFER_SIZE];
    char sendbuf[BUFFER_SIZE];   
    int length = 0;
    int firsttime = true;
    recvbuf[0] = '\0';
    sendbuf[0] = '\0';

    mymkdir(client_socketfd);
    mychdir(client_socketfd);
      
    while(true)
    {
        ofstream log;
        log.open("error_log.txt", ios::app);

        sprintf(sendbuf, "pid:%x, send", getpid());
        if (0 >= send(client_socketfd, sendbuf, BUFFER_SIZE, 0))
        {
            log << "send failed. pid:" << getpid() << "\r\n";
        }
        else
        {
            log << "send successed. pid:" << getpid() << "\r\n";
        }
        
        while(length = recv(client_socketfd, recvbuf, sizeof(recvbuf), 0))  
        {  
            if (length < 0)  
            {  
                if (false != firsttime)
                {
                    log << "Recieve error! pid:" << getpid() << "\r\n";  
                }
                break;  
            }  
            else
            {
                log << "pid:" << getpid() << " recvbuf:" << recvbuf << "\r\n";
                cout << "pid:" << getpid() << ":" << recvbuf << endl;	   
            }

            bzero(recvbuf, 1024);  

        }  
        log.close();
    }
    
}
void *thread(void *ptr)
{
    struct sockaddr_in server_addr;
    int client_socketfd = 0;

    memset(&server_addr, 0, sizeof(server_addr));

    client_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socketfd < 0)
    {
        cout << "Create socket fd failed！i:" << getpid() << endl; 
    }
    server_addr.sin_family  = AF_INET;
    //inet_pton(AF_INET, IP, (void *)server_addr.sin_addr.s_addr);
    server_addr.sin_addr.s_addr = inet_addr(IP);
    server_addr.sin_port = htons(SERVER_PORT);

    if (connect(client_socketfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        cout << "Connect to server failed! i:" << getpid() << endl; 
    }
    recv_reply(client_socketfd);

    return 0;
}

void fork_proc()
{
    struct sockaddr_in server_addr;
    int client_socketfd = 0;

    memset(&server_addr, 0, sizeof(server_addr));

    client_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socketfd < 0)
    {
        cout << "Create socket fd failed！pid:" << getpid() << endl; 
    }
    server_addr.sin_family  = AF_INET;
    //inet_pton(AF_INET, IP, (void *)server_addr.sin_addr.s_addr);
    server_addr.sin_addr.s_addr = inet_addr(IP);
    server_addr.sin_port = htons(SERVER_PORT);

    if (connect(client_socketfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        cout << "Connect to server failed! pid:" << getpid() << endl; 
    }
    recv_reply(client_socketfd);
}
int main(int argc, char *argv[])
{
    //pthread_t id; 
    int i = 0;
    pid_t child_pid;
    int stat_val;

    #if 0
    /*创建1000个线程*/
    vector<pthread_t> id(PTHREAD_NUM, 0);
    for  (i = 0; i < PTHREAD_NUM; i++)
    {
        int ret = pthread_create(&id[i], NULL, thread, NULL);
        if(0 != ret) 
        {
            cout << "Create pthread error! i:" << i << endl;
        }
    }
    
    pthread_join(id[0], NULL);
    #endif
    for  (i = 0; i < PID_NUM; i++)
    {
	/* 创建一个子进程 */
	    child_pid = fork();
        if (child_pid == 0)
	    {
            fork_proc();
            exit(0);
	    }   
	    else if(child_pid > 0)
	    {
    	}
        else
        {
            cout << "errno:" << errno << endl;
        }
    }
    wait(&stat_val);
    return 0;
}
   