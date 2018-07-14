#include <sppincl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "replyMsg.h"
#include "JobPublish.h"
#include <iostream>
#include <string.h>
#include <list>
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
    return STATE_RECV_JOB; 
    //return STATE_ID_GET2;   // test L5Route Action
}

int Fini(CAsyncFrame* pFrame, CMsgBase* pMsg)
{
	CMsg *msg = (CMsg *) pMsg;
    
    pFrame->FRAME_LOG( LOG_DEBUG, "FINI ： recv data len: %d", msg->recv_byte_count);
    std::string info;
    pMsg->GetDetailInfo(info);
    pFrame->FRAME_LOG( LOG_DEBUG, "info:%s\n", info.c_str());

    blob_type rspblob;
    rspblob.data = msg->recv_buff;
    rspblob.len = msg->recv_byte_count;
	cout << "connected num:" << gConnectClient.size()  << endl;
	cout << "fini：send to client. data:" << rspblob.data << "length:" << rspblob.len << endl;
	pFrame->FRAME_LOG( LOG_DEBUG, 
        "fini：send to client. data %s, level:%d", rspblob.data, rspblob.len);
    pMsg->SendToClient(rspblob);

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
	
	cout << "sessionProcfunc: data<" << blob->data << ">, proc_param<" << proc_param << ">" << endl;
	return 0;
}

int sessionInputfunc(void* input_param, unsigned sessionId , void* blob, void* server)
{
	cout << "sessionInputfunc" << endl;
	return 0;
}


int time_task(int sid, void* cookie, void* server)
{
	//void* user_arg = cookie;
	//CServerBase* base = (CServerBase*)server;
    /*加锁，读数据库，去锁， 发给客户端ip，端口， 任务， 
    发送成功的等待接收回复，发送失败的不用管，因为没和这台建联。再把结果上传到服务器*/
    char *buf = "Session time publish";
 	SPP_ASYNC::CreateSession(2, "custom", "tcp_multi_con", "127.0.0.1", 9255, -1,
		   500000,  DEFAULT_MULTI_CON_INF,  DEFAULT_MULTI_CON_SUP);
	myMsg *msg =  new myMsg;
	strncpy(msg->ip, "127.0.0.1", sizeof("127.0.0.1"));
	//msg->port= 9255;
    SPP_ASYNC::RegSessionCallBack(2, sessionProcfunc, NULL, sessionInputfunc, NULL);
	SPP_ASYNC::SendData(2, buf, sizeof(buf), (void*)msg);
	
	printf("sid: %d, timeout [%lu]\n", sid, time(NULL));
	return 0;
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
	    SPP_ASYNC::CreateTmSession(1, 20000, time_task, (void *)base);
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
        CAsyncFrame::Instance()->AddState(STATE_RECV_JOB, new CJobPublise);
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
	cout << "spp_handle_input_1: size:" << gConnectClient.size() << endl;
	try
	{
	    cout << "add gConnected" << endl;
		CONCLIENT *client = new CONCLIENT;
		client->client_ip = extinfo->remoteip_;
		client->clienr_port = extinfo->remoteport_;
		/*fix-me根据报文的内容来确定连接类型*/
		client->client_type = NORMAL_CLIENT;
		/*fix-me避免管理员被多次添加到连接中*/
		gConnectClient.push_back(*client);
	}catch(bad_alloc &e)
	{
		cout << "new failed!" << endl; 
		base->log_.LOG_P(LOG_ERROR, "spp_handle_input, new failed %s\n",\
                     format_time(extinfo->recvtime_));
	}
	
	cout << "spp_handle_input_2: size:" << gConnectClient.size() << endl;
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
    CTCommu    * commu = (CTCommu*)blob->owner;

	//char reply[40] = "this is reply to admin";
    base->log_.LOG_P_PID(LOG_DEBUG, "spp_handle_process, %d, %d, %s, %s\n",
                         flow,
                         blob->len,
                         inet_ntoa(*(struct in_addr*)&extinfo->remoteip_),
                         format_time(extinfo->recvtime_));

    /* 简单的单发单收模型示例  */
    //replyMsg *msg = new replyMsg;
    CMsg *msg = new CMsg;
	#if 0
    if (!msg) 
	{
        blob_type respblob;
        respblob.data  = NULL;
        respblob.len   = 0;
        commu->sendto(flow, &respblob, NULL);
        base->log_.LOG_P_PID(LOG_ERROR, "close conn, flow:%u\n", flow);

        return -1;
    }
	#endif
	cout << "recv data:" << blob->data << "time" << format_time(extinfo->recvtime_) << endl;
	
    /* 设置m信息*/
    msg->SetServerBase(base);
    msg->SetTCommu(commu);
    msg->SetFlow(flow);
	msg->SetInfoFlag(true);
    //msg->SetMsgTimeout(100);
    msg->SetMsgTimeout(0);
    memcpy(msg->input_buff, blob->data, blob->len);
    msg->input_byte_len = blob->len;
    
    base->log_.LOG_P_PID(LOG_DEBUG, "spp_handle_process, %s, %d\n",
                                    msg->input_buff,
                                    msg->input_byte_len);
    //msg->SetReqPkg(reply, sizeof(reply)); /* 微线程有独立空间,这里要拷贝一次报文 */

    CAsyncFrame::Instance()->Process(msg);

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
