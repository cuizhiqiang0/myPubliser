#include <iostream>
#include <arpa/inet.h>
#include <netinet/in.h>                         // for sockaddr_in  
#include <sys/types.h>                          // for socket  
#include <sys/stat.h> 
#include <sys/socket.h>                         // for socket    
#include <stdlib.h>    
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>

using namespace std;

#define PTHREAD_NUM 1000     
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
        cout <<"filename:" << gfilename << "open failed! i: " << pthread_self() << endl;
        
        return;
    }
    while (length = recv(client_socketfd, buffer, BUFFER_SIZE, 0))
    {
        if (length < 0)
        {
            cout << "recv data failed! i:" << pthread_self() << endl;
            break; 
        }
        else
        {
            cout << buffer << endl;
        }
        writelength = fwrite(buffer, sizeof(char), length, fp);
        if (writelength < length)
        {
            cout << "write failed ! i:" << pthread_self() << "filename:" << gfilename << endl;
            break;
        }
           
        memset(buffer, 0, BUFFER_SIZE);
    }
    cout << "file recv finish! filename:" << filename << "i:" << pthread_self() << endl;

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
            cout << "recv data failed! i:" << pthread_self() << endl;
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
    cout << "config update finish:" << pthread_self() << endl;

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
            cout << "recv data failed! i:" << pthread_self() << endl;
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
    cout << "bash excute finish:" << pthread_self() << endl;
 
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
            cout << "recv failed！i:" << pthread_self() << endl;
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
                cout << "wrong type! i:" << pthread_self() << endl; 
            }
        }
        
    }
}
#endif
void recv_reply(int client_socketfd)
{
    char recvbuf[BUFFER_SIZE];
    char sendbuf[BUFFER_SIZE];   
    recvbuf[0] = '\0';
    sendbuf[0] = '\0';
    int length = 0;


    while (true)
    {
    
        sprintf(sendbuf, "tid:%lu, send", pthread_self());
        if (0 >= send(client_socketfd,sendbuf, BUFFER_SIZE, 0))
        {
            cout << "sendfailed:tid" << pthread_self() << endl;
        }
        
        while(length = recv(client_socketfd, recvbuf, sizeof(recvbuf), 0))  
        {  
            if (length < 0)  
            {  
                printf("Recieve Failed!\n");  
                break;  
            }  

            else
                printf("tid:%s\n", recvbuf);
                cout << "tid:" << pthread_self() << ":" << recvbuf << endl;	

            bzero(recvbuf, 1024);  

        }  
        
    }
}
void *thread(void *ptr)
{
    struct sockaddr_in server_addr;
    int client_socketfd = 0;

    memset(&server_addr, 0, sizeof(server_addr));

    cout << "tid:" << pthread_self() << endl;
    client_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socketfd < 0)
    {
        cout << "Create socket fd failed！i:" << pthread_self() << endl; 
    }
    server_addr.sin_family  = AF_INET;
    //inet_pton(AF_INET, IP, (void *)server_addr.sin_addr.s_addr);
    server_addr.sin_addr.s_addr = inet_addr(IP);
    server_addr.sin_port = htons(SERVER_PORT);

    if (connect(client_socketfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        cout << "Connect to server failed! i:" << pthread_self() << endl; 
    }
    recv_reply(client_socketfd);

    return 0;
}

int main(int argc, char *argv[])
{
    //pthread_t id; 
    int i = 0;
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
    
    return 0;
}
   