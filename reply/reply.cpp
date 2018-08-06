#include <sppincl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <mysql/mysql.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <list>

#include "replyMsg.h"
#include "JobPublish.h"

using namespace std;

list<CONCLIENT> gConnectClient;
int flow;
int time_task(int sid, void* cookie, void* server);
typedef struct MEM_PACKED         

{
        char name[20];      
        unsigned long total;
        char name2[20];
}MEM_OCCUPY;

typedef struct MEM_PACK         
{
    double total,used_rate;

}MEM_PACK;
double get_memoccupy ()    

{
    FILE *fd;
    double mem_total,memfree;
    double mem_used_rate;;
    char buff[256];
    MEM_OCCUPY m ;
    //MEM_PACK p;
    fd = fopen ("/proc/meminfo", "r");

    fgets (buff, sizeof(buff), fd);
    sscanf (buff, "%s %lu %s\n", m.name, &m.total, m.name2);
    mem_total=m.total;
    fgets (buff, sizeof(buff), fd);
    sscanf (buff, "%s %lu %s\n", m.name, &m.total, m.name2);
    memfree = m.total;
    //printf("total:<%lf>, free<%f>\n", mem_total, memfree);
    mem_used_rate=(1 - memfree/mem_total)*100;
    
    return mem_used_rate ;
}

char *format_time( time_t tm)
{
    static char str_tm[1024];
    struct tm tmm;
    memset(&tmm, 0, sizeof(tmm) );
    localtime_r((time_t *)&tm, &tmm);

    snprintf(str_tm, sizeof(str_tm), "[%04d-%02d-%02d %02d:%02d:%02d]",
            tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday,
            tmm.tm_hour, tmm.tm_min, tmm.tm_sec);

    return str_tm;
}

int Init(CAsyncFrame* pFrame, CMsgBase* pMsg)
{
    //CAsyncFrame *pFrame = (CAsyncFrame *)arg1;
    //CMsg *msg = (CMsg *) arg2;
    cout << "frame init" << endl; 
    return STATE_RECV_HEART; 
    //return STATE_ID_GET2;   // test L5Route Action
}

int Fini(CAsyncFrame* pFrame, CMsgBase* pMsg)
{
    //CMsg *msg = (CMsg *) pMsg;
    
    pFrame->FRAME_LOG( LOG_DEBUG, "FINI ： recv heart fini");
    std::string info;
    pMsg->GetDetailInfo(info);
    pFrame->FRAME_LOG( LOG_DEBUG, "info:%s\n", info.c_str());

    cout << "fini：!:" << endl;
    //pMsg->SendToClient(rspblob);

    return 0;
}
int execute_bash(char *buf, char *resultbuf)
{
    char *line = NULL;
    FILE *fp;
    int i = 0;
    int len = 0;
    int count  = 0;

    line = new char[BASH_RESULT_SIZE];
    
    if ((fp = popen(buf, "r")) == NULL) {
        cout << "execute_bash error:" << buf << endl;
        return -1;
    }
    memset(line, 0, BASH_RESULT_SIZE);
    while (fgets(line, sizeof(line)-1, fp) != NULL){
        i = strlen(line);
        len = ((BASH_RESULT_SIZE - count - i) > i) ? i : (BASH_RESULT_SIZE - count -i);
        if (len <= 0)
        {
            pclose(fp);
            delete []line;
            return 0;
        } 
        memcpy(resultbuf + count, line, i);
        count = count + i ;
        memset(line, 0, BASH_RESULT_SIZE);
    }
    pclose(fp);
    delete []line;
    return 0;
}

int OverloadProcess(CAsyncFrame* pFrame, CMsgBase* pMsg)
{
    pFrame->FRAME_LOG( LOG_DEBUG, "Overload.\n" );
    char overload_str[] = "overload happens";

    blob_type rspblob;
    rspblob.data = overload_str;
    rspblob.len = strlen(overload_str);

    cout << "overload：send to client. data  mydata:" << rspblob.data
         << "length:" << rspblob.len << endl;
    pFrame->FRAME_LOG( LOG_DEBUG, 
        "overloaded：send to client. data %s, level:%d", rspblob.data, rspblob.len);
    pMsg->SendToClient(rspblob);

    return 0;
}
void update_filePublish(int taskId, char *client_ip, int client_port, bool execute_state)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream sqltmp,sqlup;
    string sql;
    int ret;

    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   

    /*执行成功直接更新执行结果就可以了*/
    if (execute_state)
    {
        printf("task execute success taskId<%d>, client_ip<%s>, client_port<%d>\n", taskId, client_ip, client_port);
        sqltmp << "update T_execute set taskState = 2, finalResult = 1" << " where taskId = " << taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
        sql = sqltmp.str();
        //cout << "update_filePublish:sql:" << sql.c_str() << endl;
        ret = mysql_query(&mysql, sql.c_str());
        if (0 != ret)
        {
            printf("update_filePublish:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
        }
        else
        { 
        }
    }
    /*执行不成功，查看次数是不是达到三次了，是的话就把结果置位失败*/
    else
    {
        printf("task execute failed taskId<%d>, client_ip<%s>, client_port<%d>\n", taskId, client_ip, client_port);
        sqltmp << "select taskState, retryTimes from T_execute where taskId = "<< taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
        sql = sqltmp.str();
        //cout << "update_filePublish:sql:" << sql.c_str() << endl;
        ret = mysql_query(&mysql, sql.c_str());
        if(ret)
        {
            printf("Error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
        }
        else
        {
            res = mysql_use_result(&mysql);
            if(NULL != res)
            {
                while ((row = mysql_fetch_row(res)))
                {
                    if (NULL == row)
                    {
                        break;
                    }
                    if (0 == strcmp("3", row[1]))
                    {
                        sqlup << "update T_execute set taskState = 3, finalResult = 2" << " where taskId = " << taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
                    }
                    else
                    {
                        sqlup << "update T_execute set taskState = 3" << " where taskId = " << taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
                    }
                    mysql_free_result(res);
                    sql = sqlup.str();
                    //cout << "update_filePublish:sql:" << sql.c_str() << endl;
                    ret = mysql_query(&mysql, sql.c_str());
                    if (0 != ret)
                    {
                        printf("update_filePublish:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
                    }
                    else
                    { 
                    }
                    break;
                }
                
            }
            else 
            {
                printf("res null\n");
                mysql_free_result(res);
            }
        }
        
    }
    
    mysql_close(&mysql);
}

void update_bashFile(int taskId, char *client_ip, int client_port, bool execute_state, unsigned char * result)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream sqltmp,sqlup;
    string sql;
    int ret;

    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   

    /*执行成功直接更新执行结果就可以了*/
    if (execute_state)
    {
        printf("task execute success taskId<%d>, client_ip<%s>, client_port<%d>\n", taskId, client_ip, client_port);
        sqltmp << "update T_execute set taskState = 2, finalResult = 1, executeResult = '" << result  << "' where taskId = " << taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
        sql = sqltmp.str();
        //cout << "update_filePublish:sql:" << sql.c_str() << endl;
        ret = mysql_query(&mysql, sql.c_str());
        if (0 != ret)
        {
            printf("update_bashFile:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
        }
        else
        { 
        }
    }
    /*执行不成功，查看次数是不是达到三次了，是的话就把结果置位失败*/
    else
    {
        printf("task execute failed taskId<%d>, client_ip<%s>, client_port<%d>\n", taskId, client_ip, client_port);
        sqltmp << "select taskState, retryTimes from T_execute where taskId = "<< taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
        sql = sqltmp.str();
        //cout << "update_filePublish:sql:" << sql.c_str() << endl;
        ret = mysql_query(&mysql, sql.c_str());
        if(ret)
        {
            printf("Error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
        }
        else
        {
            res = mysql_use_result(&mysql);
            if(NULL != res)
            {
                while ((row = mysql_fetch_row(res)))
                {
                    if (NULL == row)
                    {
                        break;
                    }
                    if ( 0 == strcmp("3", row[1]))
                    {
                        sqlup << "update T_execute set taskState = 3, finalResult = 2" << " where taskId = " << taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
                    }
                    else
                    {
                        sqlup << "update T_execute set taskState = 3" << " where taskId = " << taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
                    }
                    mysql_free_result(res);
                    sql = sqlup.str();
                    //cout << "update_filePublish:sql:" << sql.c_str() << endl;
                    ret = mysql_query(&mysql, sql.c_str());
                    if (0 != ret)
                    {
                        printf("update_bashFile:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
                    }
                    else
                    { 
                    }
                    break;
                }
                
            }
            else 
            {
                printf("res null\n");
                mysql_free_result(res);
            }
        }
        
    }
    
    mysql_close(&mysql);
}
void update_bashStr(int taskId, char *client_ip, int client_port, unsigned char *recvbuf)
{
    MYSQL mysql;
    stringstream sqltmp;
    string sql;
    int ret;
    
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql,"10.242.170.126","root",
                     "123456","taskPublish",0,NULL,0))
    {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
    }
    else
    {
      
    }
    printf("task execute success taskId<%d>, client_ip<%s>, client_port<%d>\n", taskId, client_ip, client_port);
    /*执行成功直接更新执行结果就可以了*/
    sqltmp << "update T_execute set taskState = 2, finalResult = 1, executeResult = '" << recvbuf  << "' where taskId = " << taskId << \
                                            " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
    sql = sqltmp.str();
    //cout << "update_filePublish:sql:" << sql.c_str() << endl;
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("update_bashStr:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    { 
        //printf("update_filePublish:update T-execute success\n");
    }
   
    mysql_close(&mysql);
}

int sessionProcfunc(int event, int sessionId, void* proc_param, void* data_blob, void* server)
{
    blob_type   * blob    = (blob_type*)data_blob;
    myMsg       * msg = (myMsg *)proc_param;
    unsigned char recvReply[1100] = {0};
    int type, len;
    //cout << "sessionProcfunc: data<" << blob->data << ">,  param:client_ip<" << msg->ip << "> client_port<" << msg->port << "> taskID<" << msg->taskId << ">" << endl;
    memcpy(recvReply, blob->data, blob->len);
    recvReply[blob->len] = '\0';
    #if 0
    for (i = 0; i < 15; i++)
    {
        printf("<%x>", recvReply[i]);
    }
    printf("\n");
    #endif
    type = (int)recvReply[2] & 0xF;
    len = (int)recvReply[3] & 0xFF;
    if (type == 0x5 || type == 0x6)
    {
        update_filePublish(msg->taskId, msg->ip, msg->port, (int)(recvReply[4 + len]) == 0x1);
    }
    else if (type == 0x7)
    {
        update_bashFile(msg->taskId, msg->ip, msg->port, (int)(recvReply[4 + len]) == 0x1, recvReply + 7 + len);
    }
    else if (type == 0x4)
    {
        //resultlen = (int)(recvReply[3] << 8) + (int)(recvReply[4] & 0xFF);
        update_bashStr(msg->taskId, msg->ip, msg->port, recvReply + 5);
    }
    else
    {
        cout << "recv reply type error" << endl;
        return 0;
    }
    SPP_ASYNC::RecycleCon(sessionId, -1);
    SPP_ASYNC::DestroySession(sessionId);

    return 0;
}

int sessionInputfunc(void* input_param, unsigned sessionId , void* blob, void* server)
{
    blob_type    *newblob      = (blob_type*)blob;
    //cout << "sessioninputfunc: len<" << newblob->len << ">" << endl;
    return newblob->len;
}

// 获取本机ip  
int get_local_ip(const char *eth_inf, char *ip)  
{  
    int sd;  
    struct sockaddr_in sin;  
    struct ifreq ifr;  
  
    sd = socket(AF_INET, SOCK_DGRAM, 0);  
    if (-1 == sd)  
    {  
        printf("socket error: %s\n", strerror(errno));  
        return -1;        
    }  
  
    strncpy(ifr.ifr_name, eth_inf, IFNAMSIZ);  
    ifr.ifr_name[IFNAMSIZ - 1] = 0;  
      
    // if error: No such device  
    if (ioctl(sd, SIOCGIFADDR, &ifr) < 0)  
    {  
        printf("ioctl error: %s\n", strerror(errno));  
        close(sd);  
        return -1;  
    }  
  
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));  
    snprintf(ip, IP_SIZE, "%s", inet_ntoa(sin.sin_addr));  
  
    close(sd);  
    return 0;  
}
#if 0
int file_publishProc
(char *taskId, char *client_ip, char *client_port, int taskType, char *filename,char *md5buf, char *filebuffer, long filesize)
{
    /*fix-me,根据读取数据库的结果，这里来调用不同的API*/
    char *buffer = NULL;
    int   len, port, sid, ret;

    printf("file_publishProc: client_ip<%s>, client_port<%s>, taskType<%d>, filename<%s>, filesize<%ld>\n",
        client_ip, client_port, taskType, filename, filesize);

    sscanf(client_port, "%d", &port);
    //printf("file_publish，port<%d>\n", port);
    

    sid = SPP_ASYNC::CreateSession(-1, "custom", "tcp_multi_con", client_ip, port, -1,
          500000,  DEFAULT_MULTI_CON_INF,  DEFAULT_MULTI_CON_SUP);
    if (sid < 0)
    {
        printf("file_publishProc create session failed <%d>\n", sid);
        return CREATE_SESSION_FAILED;
    }

    len = strlen(filename);
    //printf("file_publish：len<%x>", len);

    myMsg *msg = new myMsg;
    strncpy(msg->ip, client_ip, strlen(client_ip));
    msg->port = port;
    sscanf(taskId, "%d", &(msg->taskId));
    
    
    SPP_ASYNC::RegSessionCallBack(sid, sessionProcfunc, (void *)msg, sessionInputfunc, NULL);
    
    
    buffer = new char [filesize + 512];
    buffer[0] = 0xFF;
    buffer[1] = 0xEE;
    switch (taskType)
    {
        case FILE_PUBLISH:
        {
            buffer[2] = 0x1E;
            break;
        }
        case CONFIG_UPDATE:
        {
            buffer[2] = 0x1D;
            break;
        }
        case BASH_FILE:
        {
            buffer[2] = 0x1C;
            break;
        }
        default:
        {
            printf("file_publishProc,content type error\n");
            SPP_ASYNC::DestroySession(sid);
            delete []buffer;
            delete msg;
            return WRONG_TYPE;
        }
    }
    buffer[3] = len & 0xFF;//文件名长度
    memcpy(buffer + 4, filename, len);
    buffer[4 + len] = (filesize >> 24) & 0xFF;//内容长度
    buffer[5 + len] = (filesize >> 16) & 0xFF;
    buffer[6 + len] = (filesize >> 8) & 0xFF;
    buffer[7 + len] = filesize & 0xFF;
    memcpy(buffer + 8 + len, md5buf, 32);
    memcpy(buffer + 40 + len, filebuffer, filesize);
 
    #if 0
    for (i = 0; i < 15; i++)
    {
        printf("<%x>", buffer[i]);
    }
    #endif
    ret = SPP_ASYNC::SendData(sid, buffer, filesize + 512, NULL, (void *)msg);
    if (0 != ret)
    {     
        printf("send file failed.,ret <%d>\n", ret);
        delete[] buffer;
        delete msg;
        SPP_ASYNC::DestroySession(sid);
        return SEND_TO_CLIENT_FAILED;
    }
    delete[] buffer;

    return PUBLISH_SUCCESS;
}

int bash_publishProc(char *taskId, char *client_ip, char *client_port, int taskType, char *bashString)
{
    int   len, port, sid;
    char *buf = NULL;
    
    //printf("bash_publishProc: client_ip<%s>, client_port<%s>, taskType<%d>, bashString<%s>\n", client_ip, client_port, taskType, bashString);
    sscanf(client_port, "%d", &port);
    //printf("bash_publishProc:<%d>\n", port);

    sid = SPP_ASYNC::CreateSession(-1, "custom", "tcp_multi_con", client_ip, port, -1,
          500000,  DEFAULT_MULTI_CON_INF,  DEFAULT_MULTI_CON_SUP);
    if (sid < 0)
    {
        printf("bash_publishProc create session failed <%d>\n", sid);
        return CREATE_SESSION_FAILED;
    }
    /*ver:1, filetrans:A, fileend: E*/

    len = strlen(bashString);
    //printf("bash_publishProc：bashlen<%x>", len);

    myMsg *msg = new myMsg;
    strncpy(msg->ip, client_ip, strlen(client_ip));
    msg->port = port;
    sscanf(taskId, "%d", &(msg->taskId));

    SPP_ASYNC::RegSessionCallBack(sid, sessionProcfunc, (void *)msg, sessionInputfunc, NULL);

    buf = new char [SIZE];
    buf[0] = 0xFF;
    buf[1] = 0xEE;
    buf[2] = 0x1B;
    buf[3] = len & 0xFF;//bash 命令长度
    memcpy(buf + 4, bashString, len);
     
    if (0 != SPP_ASYNC::SendData(sid, buf, 4 + len , NULL, (void *)msg))
    {
        printf("send file header failed.\n");
        delete []buf;
        return SEND_TO_CLIENT_FAILED;
    }
    #if 0
    for (i = 0; i < 15; i++)
    {
        printf("<%x>", buf[i]);
    }
    #endif
    delete []buf;

    return PUBLISH_SUCCESS;
}
#endif
#if 0
int publish
(char *taskId, char *client_ip, char *client_port, char *sendbuffer, long buffersize)
{
    int task_type = 0;
    int ret = PUBLISH_SUCCESS;

    sscanf(taskType, "%d", &task_type);
    switch(task_type)
    {
        case FILE_PUBLISH:
        case CONFIG_UPDATE:
        case BASH_FILE:
        {
            ret = file_publishProc(taskId, client_ip, client_port, task_type, taskString, md5buf, filebuffer, filesize);
            break;
        }
        case BASH_STRING:
        {
            ret = bash_publishProc(taskId, client_ip, client_port, task_type, taskString);
            break;
        }
        default:
        {
            cout << "task publish wrong type" << endl;
            ret = WRONG_TYPE;
        }
    }
    

    return ret;
}
#endif
int publish(char *taskId, char *client_ip, char *client_port, PACK_BLOB *blob)
{
    int port, sid, ret;

    sscanf(client_port, "%d", &port);
    sid = SPP_ASYNC::CreateSession(-1, "custom", "tcp_multi_con", client_ip, port, -1,
          500000,  DEFAULT_MULTI_CON_INF,  DEFAULT_MULTI_CON_SUP);
    if (sid < 0)
    {
        printf("file_publishProc create session failed <%d>\n", sid);
        return CREATE_SESSION_FAILED;
    }

    myMsg *msg = new myMsg;
    strncpy(msg->ip, client_ip, strlen(client_ip));
    msg->port = port;
    sscanf(taskId, "%d", &(msg->taskId));
    //blob->_cb_info = (void*)msg;
    
    SPP_ASYNC::RegSessionCallBack(sid, sessionProcfunc, (void *)msg, sessionInputfunc, NULL);
    
    ret = SPP_ASYNC::SendData(sid, blob, NULL);
    if (0 != ret)
    {     
        printf("send file failed.,ret <%d>\n", ret);
        delete msg;
        SPP_ASYNC::DestroySession(sid);
        return SEND_TO_CLIENT_FAILED;
    }
    return PUBLISH_SUCCESS;
}
void updateT_execute(char *taskId, char *client_ip, char *client_port, int newretryTimes, bool publish_state)
{
    MYSQL mysql;
    stringstream sqltmp;
    string sql;
    int ret;

    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   

    if (NULL == taskId)
    {
        sqltmp << "update T_execute set taskState = 3" << " where ip = '" << client_ip \
            << "' and port = " << client_port << " and taskState = 1;" << endl;
    }
    else
    {
        /*下发成功*/
        if (publish_state)
        {
            sqltmp << "update T_execute set taskState = " << "1, retryTimes = " << newretryTimes << " where taskId = " << taskId << \
                                                    " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
        }
        /*下发失败*/
        else
        {
            /*超过三次就把最终的结果置位失败*/
            if (newretryTimes >= 3)
            {
                sqltmp << "update T_execute set taskState = " << "3, retryTimes = " << newretryTimes << " where taskId = " << taskId << \
                                                    " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
            }
            else
            {
                sqltmp << "update T_execute set taskState = " << "3, retryTimes = " << newretryTimes << ", finalResult = " << "2 where taskId = " << taskId << \
                                                    " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
            }
        }
    }
     
    sql = sqltmp.str();
    //cout << "updateT_execute:sql:" << sql.c_str() << endl;
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("updateT_execute:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    { 
    }

      mysql_close(&mysql);
}

void insertT_execute(char *taskId, char *client_ip, char *client_port, bool publish_state)
{
    MYSQL mysql;
    stringstream sqltmp;
    string sql;
    int ret;

    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   
    
    
    if (publish_state)
    {
        sqltmp << "insert into T_execute (taskId, ip, port) values (" << taskId << ", '" << client_ip << "', " << client_port << ");" << endl;
    }
    else
    {
        sqltmp << "insert into T_execute (taskId, ip, port,taskState) values (" << taskId << ", '" << client_ip << "', " << client_port << ", " << "3);" << endl;
    }
    
    sql = sqltmp.str();
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("insertT_execute:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
 
    mysql_close(&mysql);
}
void updateT_addtask(char *taskId)
{
    MYSQL mysql;
    stringstream sqltmp;
    string sql;
    int ret;

    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   
    
    sqltmp << "update T_addtask set clientCount =  clientCount + 1 where taskId = "  << taskId << ";" << endl;

    sql = sqltmp.str();
    //cout << "updateT_addtask:sql:" << sql.c_str() << endl;
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("updateT_addtask:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    { 
    }

    mysql_close(&mysql);
}
void client_dead_proc( char *client_ip, char* client_port)
{
    MYSQL mysql;
    stringstream sqltmp;
    string sql;
    int ret;

    mysql_init(&mysql);
   if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   
    
    sqltmp << "delete from T_connect where client_ip ='" << client_ip << "' and client_port = " << client_port  << ";"<< endl;
  
    sql = sqltmp.str();
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("insertT_execute:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
 
    mysql_close(&mysql);


}
void task_publish_to_client_insert_T_execute
(char *taskId, char *client_ip, char* client_port, PACK_BLOB *blob)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream  sqlselect;
    string sql;
    int ret;
    bool exist = false;

    assert(NULL != taskId);
    assert(NULL != client_ip);
    assert(NULL != client_port);
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   

    sqlselect << "select * from T_execute where taskId = " << taskId << " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
    sql = sqlselect.str();
    //cout << "task_publish_to_client_insert_T_execute:sql:" << sql.c_str() << endl; 
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("task_publish_to_client_insert_T_execute:select failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    {
        res = mysql_use_result(&mysql);
        if(NULL != res)
        {
            /*如果这个地方已经存在这个的记录，那么去查看这个记录的状态，如果是waiting或者success就不用管，如果是失败的话次数小于三就再下发一次，并把state改成waiting*/
            while (row = mysql_fetch_row(res))
            {
                if (row == NULL)break;
                exist = true;
            }
            /*不存在这个记录，那么我们就添加*/
            if (false == exist)
            {
                /*下发任务，并且增天记录*/
                ret = publish(taskId, client_ip, client_port, blob);
                if (PUBLISH_SUCCESS == ret)
                {
                    insertT_execute(taskId, client_ip, client_port, true);
                }
                else if (SEND_TO_CLIENT_FAILED == ret)
                {
                    printf("taskpublish failed taskid<%s>, client_ip<%s>, client_port<%s>\n", taskId, client_ip, client_port);
                    insertT_execute(taskId, client_ip, client_port, false);
                    client_dead_proc(client_ip, client_port);
                }
                else
                {
                    printf("taskpublish failed taskid<%s>, client_ip<%s>, client_port<%s>\n", taskId, client_ip, client_port);
                    insertT_execute(taskId, client_ip, client_port, false);
                }
                updateT_addtask(taskId);
            }
            mysql_free_result(res);
        }
        else 
        {
            printf("task_publish_to_client_insert_T_execute:res null\n");
            mysql_free_result(res);
        }
    }
    
    mysql_close(&mysql);
}
int getFileMd5(char *filestring, char *md5buf)
{    
    char md5exbuf[FILE_DIR_SIZE + 10];
    sprintf(md5exbuf, "%s %s", "md5sum", filestring);
    //cout << md5exbuf << endl;
    return execute_bash(md5exbuf, md5buf);
}
long bashStrPack(char *bashString, char *buf)
{
    int len = strlen(bashString);
    
    buf[0] = 0xFF;
    buf[1] = 0xEE;
    buf[2] = 0x1B;
    buf[3] = len & 0xFF;//bash 命令长度
    memcpy(buf + 4, bashString, len);

    return (long)(4 + len); 
}

long filePubPack(char *tasktype, char *taskString, char *md5buf, char *sendbuf, long filesize, ifstream file)
{
    int taskType = 0;
    int len = 0;

    len = strlen(taskString);
    sscanf(tasktype, "%d", &taskType);
    
    sendbuf[0] = 0xFF;
    sendbuf[1] = 0xEE;
    switch (taskType)
    {
        case FILE_PUBLISH:
        {
            sendbuf[2] = 0x1E;
            break;
        }
        case CONFIG_UPDATE:
        {
            sendbuf[2] = 0x1D;
            break;
        }
        case BASH_FILE:
        {
            sendbuf[2] = 0x1C;
            break;
        }
        default:
        {
            printf("filePubPack,file type error\n");
            return -1;
        }
    }
    
    sendbuf[3] = len & 0xFF;//文件名长度
    memcpy(sendbuf + 4, taskString, len);    
    sendbuf[4 + len] = (filesize >> 24) & 0xFF;//内容长度   
    sendbuf[5 + len] = (filesize >> 16) & 0xFF;
    sendbuf[6 + len] = (filesize >> 8) & 0xFF;
    sendbuf[7 + len] = filesize & 0xFF;  
    memcpy(sendbuf + 8 + len, md5buf, 32); 
    file.read(sendbuf + 40 + len, filesize); 
    file.close();
    
    return (long)(filesize + SIZE);
}

void task_publish_to_client( char *taskId, char *taskType, char *taskString, char *client_ip, char *local_ip)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream sqltmp;
    string sql;
    int ret;
    long len = 0;
    char *sendbuf = NULL;
    char *filestring = NULL;
    char md5buf[33] = {0};
    long filesize = 0;
    int type = 0;
    int filenamelen = 0;
    double memused = 0;

    assert(NULL != taskId);
    assert(NULL != taskType);
    assert(NULL != taskString);
    assert(NULL != local_ip);
    
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   

    if (NULL == client_ip)
    {
        sqltmp << "select * from T_connect where server_ip = '" << local_ip << "';" << endl;
    }
    else if (NULL != client_ip)
    {
        sqltmp << "select * from T_connect where client_ip = '" << client_ip << "' and server_ip = '" << local_ip << "';" << endl;
    }
    
    sql = sqltmp.str();
    //cout << "task_publish_to_client:sql:" << sql.c_str() << endl; 
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("task_publish_to_client:select failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    {
        res = mysql_use_result(&mysql);
        if(NULL != res)
        {
            if (0 == strcmp(taskType, "3"))
            {
                sendbuf = new char [SIZE];
                if (NULL == sendbuf)
                {
                    printf("no memory file<%s>\n", taskString);
                    return ;
                }
                len = bashStrPack(taskString, sendbuf);         
            }
            else
            { 
                filestring = new char [FILE_DIR_SIZE];
                if (NULL == filestring)
                {
                    printf("no memory file<%s>\n", taskString);
                    return ;
                }
                sprintf(filestring, "%s%s", FILE_DIR, taskString);
                ifstream file(filestring, ios::in|ios::binary|ios::ate);
                filesize = file.tellg();
                file.seekg(0, ios::beg);        
                if (filesize == -1)
                {
                    file.close();
                    delete []filestring;
                    printf("file<%s> doesnot exist\n", taskString);
                    return ;
                }
                if (0 != getFileMd5(filestring, md5buf))
                {
                    file.close();
                    delete []filestring;
                    printf("file<%s> get md5 error\n", taskString);
                    return ;
                }
                delete[]filestring;
                
                sendbuf = new char [filesize + SIZE];
                if (NULL == sendbuf)
                {
                    printf("no memory file<%s>\n", taskString);
                    return ;
                }

                filenamelen = strlen(taskString);
                sscanf(taskType, "%d", &type);
                
                sendbuf[0] = 0xFF;
                sendbuf[1] = 0xEE;
                switch (type)
                {
                    case FILE_PUBLISH:
                    {
                        sendbuf[2] = 0x1E;
                        break;
                    }
                    case CONFIG_UPDATE:
                    {
                        sendbuf[2] = 0x1D;
                        break;
                    }
                    case BASH_FILE:
                    {
                        sendbuf[2] = 0x1C;
                        break;
                    }
                    default:
                    {
                        printf("filePubPack,file type error\n");
                        delete []sendbuf;
                        return ;
                    }
                }
                
                sendbuf[3] = filenamelen & 0xFF;//文件名长度
                memcpy(sendbuf + 4, taskString, filenamelen);    
                sendbuf[4 + filenamelen] = (filesize >> 24) & 0xFF;//内容长度   
                sendbuf[5 + filenamelen] = (filesize >> 16) & 0xFF;
                sendbuf[6 + filenamelen] = (filesize >> 8) & 0xFF;
                sendbuf[7 + filenamelen] = filesize & 0xFF;  
                memcpy(sendbuf + 8 + filenamelen, md5buf, 32); 
                file.read(sendbuf + 40 + filenamelen, filesize); 
                file.close();

                len = filesize + filenamelen + 40;
                assert(NULL != sendbuf);
            }
            
     
            long *usednum = new long;
            if (usednum == NULL)
            {
                cout << "new used num error" << endl;
            }
            *usednum = 0;
            PACK_BLOB blob(sendbuf, len, NULL, usednum);
            while (row = mysql_fetch_row(res))
            {
                /*对这个client下发任务，同时把这条记录增加到T_execute的数据表里*/
                //printf("task_publish_to_client:clientip<%s>, clientPort<%s>\n", row[1], row[2]);
                if (NULL == row)
                {
                    break;
                }
                memused = get_memoccupy();
                //printf("memused:<%f>\n", memused);
                if (memused < 80.0)
                {
                    *usednum += 1;
                    task_publish_to_client_insert_T_execute(taskId, row[1], row[2], &blob);
                }
                else
                {
                    break;
                }
            }
            
            //delete []sendbuf;           
                
            mysql_free_result(res);
        }
        else 
        {
            printf("task_publish_to_client:res null\n");
            mysql_free_result(res);
        }
    }

    mysql_close(&mysql);
}

void read_task_from_db()
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char server_ip[IP_SIZE] = {0} ;
    stringstream ss, sqltmp;
    string sql;
    int ret;

    ss << "select * from T_addtask where taskState = 1;" << endl;
    sql = ss.str();
    
    /*数据库结构初始化和连接*/
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   
    ret = mysql_query(&mysql, sql.c_str());
    if(ret)
    {
        printf("Error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
        mysql_close(&mysql);
        return;
    }
    else
    {
        res = mysql_use_result(&mysql);
        if(NULL != res)
        {
            get_local_ip(IF_NAME, server_ip);        
            while ((row = mysql_fetch_row(res)))
            {
                if (NULL == row)
                {
                    break;
                }
                /*开始增添工作，指定ip，对这个ip的客户端下发任务, 不指定ip就对所有的客户端发包*/
                task_publish_to_client(row[0], row[1], row[2], row[3], server_ip);
            }
            
            mysql_free_result(res);
        }
        else 
        {
            printf("res null\n");
            mysql_free_result(res);
        }
    }
    mysql_close(&mysql);
    
}
void  db_state_update()
{
    MYSQL mysql;
    stringstream sqltmp;
    string sql;
    int ret;
    
    /*数据库结构初始化和连接*/
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   
    
    sqltmp << "update T_addtask set taskState = 2 where clientCount >= (select count(*) from T_connect);" <<endl;
    sql = sqltmp.str();
    ret = mysql_query(&mysql, sql.c_str());
    if(ret)
    {
        printf("db_state_update:error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
    }
    
    mysql_close(&mysql);
}
void taskretryproc(char *taskId, char *client_ip, char *client_port, char *retrytime)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream ss;
    string sql;
    int ret, retryTimes;
    char *sendbuf = NULL;
    long len = 0;
    long *usednum = NULL;
    char *filestring = NULL;
    char md5buf[33] = {0};
    long filesize = 0;
    int type = 0;
    int filenamelen = 0;
    float memused = 0;

    sscanf(retrytime, "%d", &retryTimes);
    ss << "select taskType, taskString from T_addtask where taskId = " << taskId << ";" << endl;
    sql = ss.str();
    
    /*数据库结构初始化和连接*/
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            //updateT_execute(taskId, client_ip, client_port,retryTimes + 1, false);
            return;
        }
    }
   
   
    ret = mysql_query(&mysql, sql.c_str());
    if(ret)
    {
        printf("Error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
    }
    else
    {
        res = mysql_use_result(&mysql);
        if(NULL != res)
        {      
            while ((row = mysql_fetch_row(res)))
            {
                if (NULL == row)
                {
                    break;
                }
                if (0 == strcmp(row[0], "3"))
                {
                    sendbuf = new char [SIZE];
                    if (NULL == sendbuf)
                    {
                        printf("no memory file<%s>\n", row[1]);
                        mysql_close(&mysql);
                         updateT_execute(taskId, client_ip, client_port,retryTimes + 1, false);
                        return ;
                    }
                    len = bashStrPack(row[1], sendbuf);         
                }
                else
                { 
                    filestring = new char [FILE_DIR_SIZE];
                    if (NULL == filestring)
                    {
                        printf("no memory file<%s>\n", row[1]);
                        mysql_close(&mysql);
                        updateT_execute(taskId, client_ip, client_port,retryTimes + 1, false);
                        return ;
                    }
                    sprintf(filestring, "%s%s", FILE_DIR, row[1]);
                    ifstream file(filestring, ios::in|ios::binary|ios::ate);
                    filesize = file.tellg();
                    file.seekg(0, ios::beg);        
                    if (filesize == -1)
                    {
                        file.close();
                        delete []filestring;
                        printf("file<%s> doesnot exist\n", row[1]);
                        mysql_close(&mysql);
                        updateT_execute(taskId, client_ip, client_port,retryTimes + 1, false);
                        return ;
                    }
                    if (0 != getFileMd5(filestring, md5buf))
                    {
                        file.close();
                        delete []filestring;
                        cout << md5buf << endl;
                        printf("file<%s> get md5 error\n", row[1]);
                        mysql_close(&mysql);
                        updateT_execute(taskId, client_ip, client_port,retryTimes + 1, false);
                        return ;
                    }
                    delete[]filestring;
                    
                    sendbuf = new char [filesize + SIZE];
                    if (NULL == sendbuf)
                    {
                        printf("no memory file<%s>\n", row[1]);
                        mysql_close(&mysql);
                        updateT_execute(taskId, client_ip, client_port,retryTimes + 1, false);
                        return ;
                    }

                    filenamelen = strlen(row[1]);
                    sscanf(row[0], "%d", &type);
                    
                    sendbuf[0] = 0xFF;
                    sendbuf[1] = 0xEE;
                    switch (type)
                    {
                        case FILE_PUBLISH:
                        {
                            sendbuf[2] = 0x1E;
                            break;
                        }
                        case CONFIG_UPDATE:
                        {
                            sendbuf[2] = 0x1D;
                            break;
                        }
                        case BASH_FILE:
                        {
                            sendbuf[2] = 0x1C;
                            break;
                        }
                        default:
                        {
                            printf("filePubPack,file type error\n");
                            delete []sendbuf;
                            updateT_execute(taskId, client_ip, client_port,retryTimes + 1, false);
                            return ;
                        }
                    }
                    
                    sendbuf[3] = filenamelen & 0xFF;//文件名长度
                    memcpy(sendbuf + 4, row[1], filenamelen);    
                    sendbuf[4 + filenamelen] = (filesize >> 24) & 0xFF;//内容长度   
                    sendbuf[5 + filenamelen] = (filesize >> 16) & 0xFF;
                    sendbuf[6 + filenamelen] = (filesize >> 8) & 0xFF;
                    sendbuf[7 + filenamelen] = filesize & 0xFF;  
                    memcpy(sendbuf + 8 + filenamelen, md5buf, 32); 
                    file.read(sendbuf + 40 + filenamelen, filesize); 
                    file.close();

                    len = filesize + filenamelen + 40;
                    assert(NULL != sendbuf);

                }
                usednum = new long;
                if (usednum == NULL)
                {
                    cout << "new used num error" << endl;
                }
                *usednum = 0;
                PACK_BLOB blob(sendbuf, len, NULL, usednum);
                memused = get_memoccupy();
                //printf("memused:<%f>\n", memused);
                if (memused < 80.0)
                {
                    ret = publish(taskId, client_ip, client_port, &blob);
                    if (PUBLISH_SUCCESS == ret)
                    {
                        updateT_execute(taskId, client_ip, client_port, retryTimes + 1, true);
                    }
                    else if (SEND_TO_CLIENT_FAILED == ret)
                    {
                        printf("taskpublish failed taskid<%s>, client_ip<%s>, client_port<%s>\n", taskId, client_ip, client_port);
                        updateT_execute(taskId, client_ip, client_port,retryTimes + 1, false);
                        if (retryTimes == 2)
                        {
                            client_dead_proc(client_ip, client_port);
                        }
                        delete usednum;
                        delete sendbuf;
                    }
                    else
                    {
                        printf("taskpublish failed taskid<%s>, client_ip<%s>, client_port<%s>\n", taskId, client_ip, client_port);
                        updateT_execute(taskId, client_ip, client_port, retryTimes + 1, false);
                        delete usednum;
                        delete sendbuf;
                    }
                    
                   
                }
                else 
                {
                    break;
                }
            }
 
            mysql_free_result(res);
        }
        else    
        {
            printf("res null\n");
            mysql_free_result(res);
        }
    }
    mysql_close(&mysql);
    
}

/**/
void taskretry()
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream ss, sqltmp;
    string sql;
    int ret;

    ss << "select * from T_execute where finalResult !=2 and taskState = 3 and retryTimes < 3;" << endl;
    sql = ss.str();
    
    /*数据库结构初始化和连接*/
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to DB_HOST_IP:%s\n",mysql_error(&mysql));
        if(!mysql_real_connect(&mysql, SECOND_DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
        {
            printf("Error connecting to SECOND_DB_HOST_IP:%s\n",mysql_error(&mysql));
            return;
        }
    }
   
    ret = mysql_query(&mysql, sql.c_str());
    if(ret)
    {
        printf("Error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
    }
    else
    {
        res = mysql_use_result(&mysql);
        if(NULL != res)
        {      
            while ((row = mysql_fetch_row(res)))
            {
                if (NULL == row)
                {
                    break;
                }
            /*开始增添工作，指定ip，对这个ip的客户端下发任务, 不指定ip就对所有的客户端发包*/
                taskretryproc(row[0], row[1], row[2], row[4]);
            }
            
            mysql_free_result(res);
        }
        else 
        {
            printf("res null\n");
            mysql_free_result(res);
        }
    }
    mysql_close(&mysql);
}
int time_task(int sid, void* cookie, void* server)
{
    //void* user_arg = cookie;
    //CServerBase* base = (CServerBase*)server;
    /*加锁，读数据库，去锁， 发给客户端ip，端口， 任务， 
    发送成功的等待接收回复，发送失败的不用管，因为没和这台建联。再把结果上传到服务器*/
    float memused = 0;
    memused = get_memoccupy();
    if (memused < 80.0)
    {
        read_task_from_db();
        taskretry();
    }
    
    db_state_update();
        
    return 0;
}

void heart_protocol_mysql_update(char *clientIp, unsigned int clientPort, char *serverIp)
{
    MYSQL             mysql;
    MYSQL_RES         *res = NULL;
    MYSQL_ROW row;
    stringstream sqltmp, updatesqltmp;
    string sql;
    int state = 3;
    int ret = 0;

    if (NULL == mysql_init(&mysql))
    {
        printf("mysql init failed\n");
    }
    if (!mysql_real_connect(&mysql, DB_HOST_IP,  DB_USER,  DB_PASSWORD,  DB_NAME, 0, NULL, 0))
    {
        printf("connect mysql DB_HOST_IP failed\n");
        if (!mysql_real_connect(&mysql, SECOND_DB_HOST_IP,  DB_USER,  DB_PASSWORD,  DB_NAME, 0, NULL, 0))
        {
            printf("connect mysql SECOND_DB_HOST_IP failed\n");  
            return;
        }
    }
    
    sqltmp << "select * from T_connect where client_ip = '" << clientIp <<"'" << " and client_port = " << clientPort<< ";" <<endl;
    sql = sqltmp.str();
    ret = mysql_query(&mysql, sql.c_str());
    if(ret)
    {
        printf("Error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
        return;
    }
    else
    {
        res = mysql_use_result(&mysql);
        if(NULL != res)
        {
            row = mysql_fetch_row(res);
            /*不存在这个客户端的记录，则新加入*/
            if (NULL == row)
            {
                updatesqltmp << "insert into T_connect(client_ip, client_port, server_ip, client_state) values ('" << \
                        clientIp << "', " << clientPort << ", '"<< serverIp << "', " << state << ");" << endl;
                sql = updatesqltmp.str();
                mysql_free_result(res);
                ret = mysql_query(&mysql, sql.c_str());
                if (0 != ret)
                {
                    printf("instert failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
                }
            }
            /*存在这个客户端的记录，则更新连接状态*/
            else
            {
                if (0 != strncmp(serverIp, row[3], strlen(row[3])))
                {
                    updateT_execute(NULL, clientIp, row[2], 0, false);
                }
                
                updatesqltmp << "update T_connect set client_state = " << state << ",server_ip= '" << serverIp<< "' where client_ip = '" << clientIp << \
                           "' and client_port = " << clientPort << ";" << endl;
                sql = updatesqltmp.str();
                mysql_free_result(res);
                ret = mysql_query(&mysql, sql.c_str());
                if (0 != ret)
                {
                    printf("update failed, ret<%d>, <%s>\n", ret, mysql_error(&mysql));
                }
             
            }
        }
        else 
        {
            printf("res null\n");
            mysql_free_result(res);
        }
  
   }
   mysql_close(&mysql);

}
void *timerinit(void *ptr)
{
    SPP_ASYNC::CreateTmSession(1, TIME_INTERVAL, time_task, NULL);
}

/**
 * @brief 业务模块初始化插件接口（可选实现proxy,worker）
 * @param arg1 - 配置文件
 * @param arg2 - 服务器容器对象
 * @return 0 - 成功, 其它失败
 */
extern "C" int spp_handle_init(void* arg1, void* arg2)
{
    const char * etc  = (const char*)arg1;
    CServerBase* base = (CServerBase*)arg2;

    base->log_.LOG_P_PID(LOG_DEBUG, "spp_handle_init, config:%s, servertype:%d\n", etc, base->servertype());

    if (base->servertype() == SERVER_TYPE_WORKER)
    {    
        /*这个地方还要起一个定时器*/
        pthread_t t0;
        if(pthread_create(&t0, NULL, timerinit, NULL) == -1)
        {
            cout << ("fail to create pthread t0");
        }
        #if 0
        SPP_ASYNC::CreateSession(2, "custom", "tcp_multi_con", "127.0.0.1", 9255, -1,
           500000,  DEFAULT_MULTI_CON_INF,  DEFAULT_MULTI_CON_SUP)
        SPP_ASYNC::RegSessionCallBack(2, sessionProcfunc, NULL, sessionInputfunc, NULL);
        #endif
        /*初始化异步框架*/
        CAsyncFrame::Instance()->InitFrame2(base, 100, 0); 

        CAsyncFrame::Instance()->RegCallback(CAsyncFrame::CBType_Init, Init);
        CAsyncFrame::Instance()->RegCallback(CAsyncFrame::CBType_Fini, Fini);
        CAsyncFrame::Instance()->RegCallback(CAsyncFrame::CBType_Overload, OverloadProcess);

        //CAsyncFrame::Instance()->AddState(STATE_WAITING, new CWaitingState);
        CAsyncFrame::Instance()->AddState(STATE_RECV_HEART, new CJobPublise);
        //CAsyncFrame::Instance()->AddState(STATE_PUBLISH_JOB, new CJobPublise);
    }
    
    return 0;
}


/**
 * @brief 业务模块检查报文合法性与分包接口(proxy)
 * @param flow - 请求包标志
 * @param arg1 - 数据块对象
 * @param arg2 - 服务器容器对象
 * @return ==0  数据包还未完整接收,继续等待
 *         > 0  数据包已经接收完整, 返回包长度
 *         < 0  数据包非法, 连接异常, 将断开TCP连接
 */
extern "C" int spp_handle_input(unsigned flow, void* arg1, void* arg2)
{
    //数据块对象，结构请参考tcommu.h
    blob_type* blob = (blob_type*)arg1;
    //extinfo有扩展信息
    TConnExtInfo* extinfo = (TConnExtInfo*)blob->extdata;
    //服务器容器对象
    CServerBase* base = (CServerBase*)arg2;

    base->log_.LOG_P(LOG_DEBUG, "spp_handle_input, %d, %d, %s, %s\n",\
                     flow,\
                     blob->len,\
                     inet_ntoa(*(struct in_addr*)&extinfo->remoteip_),\
                     format_time(extinfo->recvtime_));
    #if 0
    cout << "handle input! flow:" << flow << " data:" << blob->data << "len: " << blob->len << " |time:" << 
                     format_time(extinfo->recvtime_) << endl;
    #endif
    return blob->len;
}

/**
 * @brief 业务模块报文按worker组分发接口(proxy)
 * @param flow - 请求包标志
 * @param arg1 - 数据块对象
 * @param arg2 - 服务器容器对象
 * @return 处理该报文的worker组id
 */
extern "C" int spp_handle_route(unsigned flow, void* arg1, void* arg2)
{
    //数据块对象，结构请参考tcommu.h
    //blob_type* blob = (blob_type*)arg1;
    //extinfo有扩展信息
    //TConnExtInfo* extinfo = (TConnExtInfo*)blob->extdata;
    //服务器容器对象
    CServerBase* base = (CServerBase*)arg2;
    base->log_.LOG_P(LOG_DEBUG, "spp_handle_route, %d\n", flow);
    //cout << "spp_handle_route time:" << format_time(extinfo->recvtime_) << endl;
    #if 0
    int route_no = 2;
    return GROUPID(route_no);
    #endif
    return 1;
}

/**
 * @brief 业务模块报文,worker组的处理接口(worker)
 * @param flow - 请求包标志
 * @param arg1 - 数据块对象
 * @param arg2 - 服务器容器对象
 * @return 0 - 成功,其它表示失败
 */
extern "C" int spp_handle_process(unsigned flow, void* arg1, void* arg2)
{
    blob_type   * blob    = (blob_type*)arg1;
    TConnExtInfo* extinfo = (TConnExtInfo*)blob->extdata;
    CServerBase* base  = (CServerBase*)arg2;
    //CTCommu    * commu = (CTCommu*)blob->owner;
    unsigned char heartbeat[SIZE + 1];
    unsigned int clientPort;
    char clientIp[20];
    char serverIp[20];
    heartbeat[0] = '\0';
    
    //char reply[40] = "this is reply to admin";
    base->log_.LOG_P_PID(LOG_DEBUG, "spp_handle_process, %d, %d, %s, %s\n",
                         flow,
                         blob->len,
                         inet_ntoa(*(struct in_addr*)&extinfo->remoteip_),
                         format_time(extinfo->recvtime_));
    //cout << "recv client heartbeat" << format_time(extinfo->recvtime_) << "pid:" << getpid() << endl;
    /*fix-me,这里是心跳报文，所以直接操作数据库就可以了*/
    memcpy(heartbeat, blob->data, blob->len);
  
    clientPort =  (int)((heartbeat[3] << 8) + (int)(heartbeat[4] & 0xFF));
    strncpy(clientIp, inet_ntoa(*(struct in_addr*)&extinfo->remoteip_), strlen(inet_ntoa(*(struct in_addr*)&extinfo->remoteip_)) );
    strncpy(serverIp, inet_ntoa(*(struct in_addr*)&extinfo->localip_), strlen(inet_ntoa(*(struct in_addr*)&extinfo->localip_)) );

    heart_protocol_mysql_update(clientIp, clientPort, serverIp);
    
    return 0;
}


/**
 * @brief 业务服务终止接口函数(proxy/worker)
 * @param arg1 - 保留
 * @param arg2 - 服务器容器对象
 * @return 0 - 成功,其它表示失败
 */
extern "C" void spp_handle_fini(void* arg1, void* arg2)
{
    CServerBase* base = (CServerBase*)arg2;
    base->log_.LOG_P(LOG_DEBUG, "spp_handle_fini\n");

    if ( base->servertype() == SERVER_TYPE_WORKER )
    {
        CAsyncFrame::Instance()->FiniFrame();
        //CSyncFrame::Instance()->Destroy();
        CAsyncFrame::Destroy();
    }
}

#if 0
/**
 * @brief 提取模调上报信息的回调函数
 * @param flow - 请求包标志
 * @param arg1 - 数据块对象
 * @param arg2 - 上报实例
 * @return 0-成功, >0 无需上报, <0 失败上报异常
 */
extern "C" int spp_handle_report(unsigned flow, void* arg1, void* arg2)
{
    blob_type   * blob    = (blob_type*)arg1;
    CReport     * rpt     = (CReport *)arg2;

    char *pMsg = blob->data;
    int len = blob->len;
    
    uint32_t cmd = 0;
    int ret = 0;
    
    rpt->set_cmd(cmd);
    rpt->set_result(ret);
    
    return 0;
}
#endif
