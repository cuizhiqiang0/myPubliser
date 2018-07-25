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
    if(!mysql_real_connect(&mysql,"10.242.170.126","root",
                     "123456","taskPublish",0,NULL,0))
    {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
    }
    else
    {
        printf("Connected........\n");
    }

    /*执行成功直接更新执行结果就可以了*/
    if (execute_state)
    {
        sqltmp << "update T_execute set taskState = 2, finalResult = 1" << " where taskId = " << taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
        sql = sqltmp.str();
        cout << "update_filePublish:sql:" << sql.c_str() << endl;
        ret = mysql_query(&mysql, sql.c_str());
        if (0 != ret)
        {
            printf("update_filePublish:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
        }
        else
        { 
            printf("update_filePublish:update T-execute success\n");
        }
    }
    /*执行不成功，查看次数是不是达到三次了，是的话就把结果置位失败*/
    else
    {
        sqltmp << "select taskState, retryTimes from T_execute where taskId = "<< taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
        sql = sqltmp.str();
        cout << "update_filePublish:sql:" << sql.c_str() << endl;
        ret = mysql_query(&mysql, sql.c_str());
        if(ret)
        {
            printf("Error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
        }
        else
        {
            printf("Query made ....t<%d>\n", ret);
            res = mysql_use_result(&mysql);
            if(NULL != res)
            {
                printf("res not null\n");
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
                    cout << "update_filePublish:sql:" << sql.c_str() << endl;
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
    if(!mysql_real_connect(&mysql,"10.242.170.126","root",
                     "123456","taskPublish",0,NULL,0))
    {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
    }
    else
    {
        printf("Connected........\n");
    }

    /*执行成功直接更新执行结果就可以了*/
    if (execute_state)
    {
        sqltmp << "update T_execute set taskState = 2, finalResult = 1, executeResult = '" << result  << "' where taskId = " << taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
        sql = sqltmp.str();
        cout << "update_filePublish:sql:" << sql.c_str() << endl;
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
        sqltmp << "select taskState, retryTimes from T_execute where taskId = "<< taskId << \
                                                " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
        sql = sqltmp.str();
        cout << "update_filePublish:sql:" << sql.c_str() << endl;
        ret = mysql_query(&mysql, sql.c_str());
        if(ret)
        {
            printf("Error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
        }
        else
        {
            printf("Query made ....t<%d>\n", ret);
            res = mysql_use_result(&mysql);
            if(NULL != res)
            {
                printf("res not null\n");
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
                    cout << "update_filePublish:sql:" << sql.c_str() << endl;
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
        printf("Connected........\n");
    }
    
    /*执行成功直接更新执行结果就可以了*/
    sqltmp << "update T_execute set taskState = 2, finalResult = 1, executeResult = '" << recvbuf  << "' where taskId = " << taskId << \
                                            " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
    sql = sqltmp.str();
    cout << "update_filePublish:sql:" << sql.c_str() << endl;
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("update_filePublish:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    { 
        printf("update_filePublish:update T-execute success\n");
    }
   
    mysql_close(&mysql);
}

int sessionProcfunc(int event, int sessionId, void* proc_param, void* data_blob, void* server)
{
    blob_type   * blob    = (blob_type*)data_blob;
    myMsg       * msg = (myMsg *)proc_param;
    unsigned char recvReply[1100] = {0};
    int i, type, len;
    cout << "sessionProcfunc: data<" << blob->data << ">,  param:client_ip<" << msg->ip << "> client_port<" << msg->port << "> taskID<" << msg->taskId << ">" << endl;
    memcpy(recvReply, blob->data, blob->len);
    for (i = 0; i < 15; i++)
    {
        printf("<%x>", recvReply[i]);
    }
    printf("\n");
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
    SPP_ASYNC::RecycleCon(sessionId, -1);
    SPP_ASYNC::DestroySession(sessionId);

    return 0;
}

int sessionInputfunc(void* input_param, unsigned sessionId , void* blob, void* server)
{
    blob_type    *newblob      = (blob_type*)blob;
    cout << "sessioninputfunc: len<" << newblob->len << ">" << endl;
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
int file_publishProc(char *taskId, char *client_ip, char *client_port, int taskType, char *filename)
{
    /*fix-me,根据读取数据库的结果，这里来调用不同的API*/
    char *filestring = NULL;
    char *buffer = NULL;
    char *buf = NULL;
    long fliesize;
    int  i, len, port, sid;
    char md5buf[70];
    char md5result[33];
    printf("file_publishProc: client_ip<%s>, client_port<%s>, taskType<%d>, filename<%s>\n", client_ip, client_port, taskType, filename);

    sscanf(client_port, "%d", &port);
    printf("file_publish，port<%d>\n", port);
    

    sid = SPP_ASYNC::CreateSession(-1, "custom", "tcp_multi_con", client_ip, port, -1,
          500000,  DEFAULT_MULTI_CON_INF,  DEFAULT_MULTI_CON_SUP);
    if (sid < 0)
    {
        printf("file_publishProc create session failed <%d>\n", sid);
        return CREATE_SESSION_FAILED;
    }

    buf = new char [512];
    filestring = new char [FILE_DIR_SIZE];
    sprintf(filestring, "%s%s", FILE_DIR, filename);
    ifstream file(filestring, ios::in|ios::binary|ios::ate);
    fliesize = file.tellg();
    file.seekg(0, ios::beg);
    cout << "filesize:" << fliesize << endl;

    len = strlen(filename);
    printf("file_publish：len<%x>", len);

    myMsg *msg = new myMsg;
    strncpy(msg->ip, client_ip, strlen(client_ip));
    msg->port = port;
    sscanf(taskId, "%d", &(msg->taskId));
    
    SPP_ASYNC::RegSessionCallBack(sid, sessionProcfunc, (void *)msg, sessionInputfunc, NULL);
    
    /*ver:1, filetrans:A, fileend: E*/
    buf[0] = 0xFF;
    buf[1] = 0xEE;
    switch (taskType)
    {
        case FILE_PUBLISH:
        {
            buf[2] = 0x1A;
            break;
        }
        case CONFIG_UPDATE:
        {
            buf[2] = 0x19;
            break;
        }
        case BASH_FILE:
        {
            buf[2] = 0x18;
            break;
        }
        default:
        {
            printf("file_publishProc, head type error\n");
            SPP_ASYNC::DestroySession(sid);
            delete []filestring;
            delete []buf;
            return WRONG_TYPE;
        }
    }
    buf[3] = len & 0xFF;//文件名长度
    memcpy(buf + 4, filename, len);
    buf[4 + len] = (fliesize >> 24) & 0xFF;//内容长度
    buf[5 + len] = (fliesize >> 16) & 0xFF;
    buf[6 + len] = (fliesize >> 8) & 0xFF;
    buf[7 + len] = fliesize & 0xFF; 
    /*md5*/
    sprintf(md5buf, "%s %s", "md5sum", filestring);
    execute_bash(md5buf, md5result);
    memcpy(buf + 8 + len, md5result, 32);
    for (i = 0; i < 50; i++)printf("<%x>", buf[i]);
    if (0 != SPP_ASYNC::SendData(sid, buf, 512, NULL, (void *)msg))
    {
        printf("send file header failed.");
        delete []filestring;
        delete []buf;
        SPP_ASYNC::DestroySession(sid);
        return SEND_TO_CLIENT_FAILED;
    }
    
    delete []filestring;
    delete []buf;
    
    buffer = new char [fliesize + 512];
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
            return WRONG_TYPE;
        }
    }
    buffer[3] = len & 0xFF;//文件名长度
    memcpy(buffer + 4, filename, len);
    buffer[4 + len] = (fliesize >> 24) & 0xFF;//内容长度
    buffer[5 + len] = (fliesize >> 16) & 0xFF;
    buffer[6 + len] = (fliesize >> 8) & 0xFF;
    buffer[7 + len] = fliesize & 0xFF;
    file.read(buffer + 8 + len, fliesize);
    file.close();

    for (i = 0; i < 15; i++)
    {
        printf("<%x>", buffer[i]);
    }

    if (0 != SPP_ASYNC::SendData(sid, buffer, fliesize + 512, NULL, (void *)msg))
    {     
        printf("send file failed.");
        delete[] buffer;
        SPP_ASYNC::DestroySession(sid);
        return SEND_TO_CLIENT_FAILED;
    }
    delete[] buffer;

    return PUBLISH_SUCCESS;
}

int bash_publishProc(char *taskId, char *client_ip, char *client_port, int taskType, char *bashString)
{
    int  i, len, port, sid;
    char *buf = NULL;
    
    printf("bash_publishProc: client_ip<%s>, client_port<%s>, taskType<%d>, bashString<%s>\n", client_ip, client_port, taskType, bashString);
    sscanf(client_port, "%d", &port);
    printf("bash_publishProc:<%d>\n", port);

    sid = SPP_ASYNC::CreateSession(-1, "custom", "tcp_multi_con", client_ip, port, -1,
          500000,  DEFAULT_MULTI_CON_INF,  DEFAULT_MULTI_CON_SUP);
    if (sid < 0)
    {
        printf("bash_publishProc create session failed <%d>\n", sid);
        return CREATE_SESSION_FAILED;
    }
    /*ver:1, filetrans:A, fileend: E*/

    len = strlen(bashString);
    printf("bash_publishProc：bashlen<%x>", len);

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

    for (i = 0; i < 15; i++)
    {
        printf("<%x>", buf[i]);
    }
    delete []buf;

    return PUBLISH_SUCCESS;
}


int publish(char *taskId, char *client_ip, char *client_port, char *taskType, char *taskString)
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
            ret = file_publishProc(taskId, client_ip, client_port, task_type, taskString);
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
void updateT_execute(char *taskId, char *client_ip, char *client_port, int newretryTimes, bool publish_state)
{
    MYSQL mysql;
    stringstream sqltmp;
    string sql;
    int ret;

    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql,DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
    }
    else
    {
        printf("Connected........");
    }

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
    sql = sqltmp.str();
    cout << "updateT_execute:sql:" << sql.c_str() << endl;
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("updateT_execute:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    { 
         printf("updateT_execute:update T-execute certainip success\n");
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
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
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
    cout << "insertT_execute:sql:" << sql.c_str() << endl;
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
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
    }
    
    sqltmp << "update T_addtask set clientCount =  clientCount + 1 where taskId = "  << taskId << ";" << endl;

    sql = sqltmp.str();
    cout << "updateT_addtask:sql:" << sql.c_str() << endl;
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
    printf("updateT_addtask:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    { 
     printf("updateT_addtask:update success\n");
    }

    mysql_close(&mysql);
}
void task_publish_to_client_insert_T_execute(char *taskId, char *client_ip, char* client_port, char *taskType, char *taskString)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream  sqlselect;
    string sql;
    int ret;
    bool exist = false;

    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
    }

    sqlselect << "select * from T_execute where taskId = " << taskId << " and ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
    sql = sqlselect.str();
    cout << "task_publish_to_client_insert_T_execute:sql:" << sql.c_str() << endl; 
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
            while ((row = mysql_fetch_row(res)))
            {
                exist = true;
                #if 0
                /*判断 state和重做的次数*/
                printf("taskstate<%s>,retrytimes<%s>\n", row[3], row[4]);
              
                sscanf(row[3], "%d", &taskState);
                sscanf(row[4], "%d", &retryTimes);
                printf("existed task taskState<%d>, retrytimes<%d>\n", taskState, retryTimes);
                if ((taskState == 3) && (retryTimes < 3))
                {
                    /*再下发一次，如果下发成功retrytimes +1， state 改成waiting，下发不成功就直接retrytimes +1， state还是failed*/
                    if (PUBLISH_SUCCESS == publish(taskId, client_ip, client_port, taskType, taskString))
                    {
                        updateT_execute(taskId, client_ip, client_port, retryTimes + 1, true);
                    }
                    else
                    {
                        updateT_execute(taskId, client_ip, client_port, retryTimes + 1, false);
                    }
                }
                #endif
            }
            /*不存在这个记录，那么我们就添加*/
            if (false == exist)
            {
                /*下发任务，并且增天记录*/
                if (PUBLISH_SUCCESS == publish(taskId, client_ip, client_port, taskType, taskString))
                {
                    insertT_execute(taskId, client_ip, client_port, true);
                }
                else
                {
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
void task_publish_to_client( char *taskId, char *taskType, char *taskString, char *client_ip, char *local_ip)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream sqltmp;
    string sql;
    int ret;

    mysql_init(&mysql);
     if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                      DB_PASSWORD, DB_NAME,0,NULL,0))
     {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
     }
     else
     {
        printf("Connected........\n");
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
    cout << "task_publish_to_client:sql:" << sql.c_str() << endl; 
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
                while ((row = mysql_fetch_row(res)))
                {
                    /*对这个client下发任务，同时把这条记录增加到T_execute的数据表里*/
                    printf("task_publish_to_client:clientip<%s>, clientPort<%s>\n", row[1], row[2]);
                    task_publish_to_client_insert_T_execute(taskId, row[1], row[2], taskType, taskString);
                }
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
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
    }
    else
    {
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
        printf("db_state_update:Error connecting to database:%s\n",mysql_error(&mysql));
    }
    else
    {
    }
    
    sqltmp << "update T_addtask set taskState = 2 where clientCount >= (select count(*) from T_connect);" <<endl;
    sql = sqltmp.str();
    ret = mysql_query(&mysql, sql.c_str());
    if(ret)
    {
        printf("db_state_update:error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
    }
    else
    {
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

    sscanf(retrytime, "%d", &retryTimes);
    ss << "select taskType, taskString from T_addtask where taskId = " << taskId << ";" << endl;
    sql = ss.str();
    
    /*数据库结构初始化和连接*/
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
    }
    else
    {
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
                if (PUBLISH_SUCCESS == publish(taskId, client_ip, client_port, row[0], row[1]))
                {
                    updateT_execute(taskId, client_ip, client_port, retryTimes + 1, true);
                }
                else
                {
                    updateT_execute(taskId, client_ip, client_port, retryTimes + 1, false);
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

    ss << "select * from T_execute where taskState = 3 and retryTimes < 3;" << endl;
    sql = ss.str();
    
    /*数据库结构初始化和连接*/
    mysql_init(&mysql);
    if(!mysql_real_connect(&mysql, DB_HOST_IP, DB_USER,
                  DB_PASSWORD, DB_NAME,0,NULL,0))
    {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
    }
    else
    {
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
   
    read_task_from_db();
    taskretry();
    db_state_update();
        
    return 0;
}

void heart_protocol_mysql_update(char *clientIp, unsigned int clientPort, char *serverIp)
{
    MYSQL             mysql;
    MYSQL_RES         *res = NULL;
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
        printf("connect mysql failed\n");
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
            /*不存在这个客户端的记录，则新加入*/
            if (NULL == mysql_fetch_row(res))
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
                updatesqltmp << "update T_connect set client_state = " << state << " where client_ip = '" << clientIp << \
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
        SPP_ASYNC::CreateTmSession(1, TIME_INTERVAL, time_task, (void *)base);
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
    blob_type* blob = (blob_type*)arg1;
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
