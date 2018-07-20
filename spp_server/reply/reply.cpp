#include <sppincl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <string.h>
#include <list>
#include <mysql/mysql.h>

#include "replyMsg.h"
#include "JobPublish.h"


#define SIZE 512
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

int sessionProcfunc(int event, int sessionId, void* proc_param, void* data_blob, void* server)
{
    blob_type   * blob    = (blob_type*)data_blob;
    myMsg       * param = (myMsg *)proc_param;
     cout << "sessionProcfunc: data<" << blob->data << ">,  param<" << param->ip << ">" << endl;
    return 0;
}

int sessionInputfunc(void* input_param, unsigned sessionId , void* blob, void* server)
{
    blob_type    *newblob      = (blob_type*)blob;
    cout << "sessioninputfunc: data<" << newblob->data << ">, proc_param<" << server << ">" << endl;
    return newblob->len;
}


int time_task(int sid, void* cookie, void* server)
{
    //void* user_arg = cookie;
    //CServerBase* base = (CServerBase*)server;
    /*加锁，读数据库，去锁， 发给客户端ip，端口， 任务， 
    发送成功的等待接收回复，发送失败的不用管，因为没和这台建联。再把结果上传到服务器*/


    #if 0
    /*fix-me,根据读取数据库的结果，这里来调用不同的API*/
    const char * filename = "/data/home/waltercui/file/Jane.txt";
    char * buffer;
    char buf[SIZE];
    long size, times, ans, ans1;
    int  i, len, ret, filesize_foronce;
    ifstream file(filename, ios::in|ios::binary|ios::ate);
    size = file.tellg();
    
    file.seekg(0, ios::beg);
    cout << "size:" << size << endl;
    buffer = new char [size + 512];

     SPP_ASYNC::CreateSession(2, "custom", "tcp_multi_con", "127.0.0.1", 9255, -1,
           500000,  DEFAULT_MULTI_CON_INF,  DEFAULT_MULTI_CON_SUP);

    myMsg *msg =  new myMsg;
    strncpy(msg->ip, "127.0.0.1", sizeof("127.0.0.1"));

    /*ver:1, filetrans:A, fileend: E*/
    
    len = strlen("Jane.txt");
    printf("len<%x>", len);

    filesize_foronce = int(SIZE - 5 - len);
    //msg->port= 9255;
    SPP_ASYNC::RegSessionCallBack(2, sessionProcfunc, NULL, sessionInputfunc, NULL);

    /*fix-me循环发，每次发4K* */
    ans = (long)(size / filesize_foronce);
    ans1 = (long)(size % filesize_foronce == 0) ? 0 : 1;
    printf("size<%d>, SIZE<%d>, ans<%d>, an2<%d>\n", size, filesize_foronce, ans, ans1);
    times = ans + ans1;
    printf("times<%d>\n", times); 

    buf[0] = 0xFF;
    buf[1] = 0xEE;
    buf[2] = 0x1A;
    buf[3] = len & 0xFF;//文件名长度
    memcpy(buf + 4, "Jane.txt", strlen("Jane.txt"));
    buf[4 + len] = (size >> 24) & 0xFF;//内容长度
    buf[5 + len] = (size >> 16) & 0xFF;
     buf[6 + len] = (size >> 8) & 0xFF;
    buf[7 + len] = size & 0xFF;    
    
    //memcpy(buf + 3 + 2 + len, buffer + i * filesize_foronce, filesize_foronce);
    ret = SPP_ASYNC::SendData(2, buf, sizeof(buf), (void*)msg);
    {
        if (0 != ret)
        {
            printf("send file head failed. ret<%d>, i<%d>", ret, i);
        }
    }
    
    buffer[0] = 0xFF;
    buffer[1] = 0xEE;
    buffer[2] = 0x1E;
    buffer[3] = len & 0xFF;//文件名长度
    memcpy(buffer + 4, "Jane.txt", strlen("Jane.txt"));
    buffer[4 + len] = (size >> 24) & 0xFF;//内容长度
    buffer[5 + len] = (size >> 16) & 0xFF;
     buffer[6 + len] = (size >> 8) & 0xFF;
    buffer[7 + len] = size & 0xFF;
    
    file.read(buffer + 8 + len, size);
        file.close();
    for (i = 0; i < 15; i++)
    {
        printf("<%x>", buffer[i]);
    }
    //memcpy(buf + 3 + 2 + len, buffer + i * filesize_foronce, filesize_foronce);
    ret = SPP_ASYNC::SendData(2, buffer, size + 512, (void*)msg);
    {
        if (0 != ret)
        {
            printf("send file failed. ret<%d>, i<%d>", ret, i);
        }
    }
    delete[] buffer;
    #endif
    printf("sid: %d, timeout [%lu]\n", sid, time(NULL));
        
  
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
    if (!mysql_real_connect(&mysql, "10.242.170.126", "root", "123456", "taskPublish", 0, NULL, 0))
    {
        printf("connect mysql failed\n");
    }
    
    sqltmp << "select * from T_connect where client_ip = '" << clientIp <<"'" << " and client_port = " << clientPort<< ";" <<endl;
    cout << "select sql: " << sqltmp.str() << endl;
    sql = sqltmp.str();
    ret = mysql_query(&mysql, sql.c_str());
    if(ret)
    {
        printf("Error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
        return;
    }
    else
    {
        printf("Query made ....t<%d>\n", ret);
        res = mysql_use_result(&mysql);
        if(NULL != res)
        {
            /*不存在这个客户端的记录，则新加入*/
            if (NULL == mysql_fetch_row(res))
            {
                printf("instert:\n");
                updatesqltmp << "insert into T_connect(client_ip, client_port, server_ip, client_state) values ('" << \
                        clientIp << "', " << clientPort << ", '"<< serverIp << "', " << state << ");" << endl;
                sql = updatesqltmp.str();
                mysql_free_result(res);
                cout << "sql:" << sql.c_str() << endl; 
                ret = mysql_query(&mysql, sql.c_str());
                if (0 != ret)
                {
                    printf("instert failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
                }
            }
            /*存在这个客户端的记录，则更新连接状态*/
            else
            {
                printf("update:\n");
                updatesqltmp << "update T_connect set client_state = " << state << " where client_ip = '" << clientIp << \
                           "' and client_port = " << clientPort << ";" << endl;
                sql = updatesqltmp.str();
                mysql_free_result(res);
                cout << "SQL:" << sql.c_str() << endl;
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

    cout << "handle input! flow:" << flow << " data:" << blob->data << "time:" << 
                     format_time(extinfo->recvtime_) << endl;

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
    TConnExtInfo* extinfo = (TConnExtInfo*)blob->extdata;
    //服务器容器对象
    CServerBase* base = (CServerBase*)arg2;
    base->log_.LOG_P(LOG_DEBUG, "spp_handle_route, %d\n", flow);
    cout << "spp_handle_route time:" << format_time(extinfo->recvtime_) << endl;
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
    unsigned char heartbeat[513];
    int i;
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
    cout << "recv data:" << blob->data << "|time" << format_time(extinfo->recvtime_) << "pid:" << getpid() << endl;
    /*fix-me,这里是心跳报文，所以直接操作数据库就可以了*/
    memcpy(heartbeat, blob->data, blob->len);
    
    printf("heart protocol:");
    for(i = 0; i < 11; i++)
    {
        printf("<%x>", heartbeat[i]);
    }
    printf("\n");
    printf("client ip<%s>\n",inet_ntoa(*(struct in_addr*)&extinfo->remoteip_));
    //printf("client port<%d>\n", extinfo->remoteport_);
    printf("client port<%d>\n", (int)((int)(heartbeat[3] << 8) + (int)(heartbeat[4] & 0xFF))) ;
    printf("server ip<%s>\n", inet_ntoa(*(struct in_addr*)&extinfo->localip_));
    //clientPort =  extinfo->remoteport_;
    clientPort =  (int)((heartbeat[3] << 8) + heartbeat[4] & 0xFF);
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
