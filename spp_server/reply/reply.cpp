#include <iostream>
#include <sppincl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "replyMsg.h"
#include "RecvJobState.h"


using namespace std;
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
    //CAsyncFrame *pFrame = (CAsyncFrame *)arg1;
	# if 0
	CMsg *msg = (CMsg *) pMsg;
    pFrame->FRAME_LOG( LOG_DEBUG,
                       "uin: %d, level: %d, coin: %d, seed_num: %d, gain_num: %d, result: %d\n",
                       msg->uin, msg->level, msg->coin, msg->seed_num, msg->gain_num, msg->result);
	#endif
	pFrame->FRAME_LOG( LOG_DEBUG, "FINI");
    std::string info;
    pMsg->GetDetailInfo(info);
    pFrame->FRAME_LOG( LOG_DEBUG, "%s\n", info.c_str());

    rsp_pkg pkg;
    pkg.mydata = RETURN_TO_ADMIN;
    pkg.level = SERVER;

    blob_type rspblob;
    rspblob.data = (char *)&pkg;
    rspblob.len = sizeof(pkg);
	cout << "fini：send to client. data  mydata:" << pkg.mydata << "level:" << pkg.level << endl;
	pFrame->FRAME_LOG( LOG_DEBUG, "fini：send to client. data  mydata: %d, level:%d", pkg.mydata, pkg.level);
    pMsg->SendToClient(rspblob);

    return 0;
}

int OverloadProcess(CAsyncFrame* pFrame, CMsgBase* pMsg)
{
    //CAsyncFrame *pFrame = (CAsyncFrame *)arg1;
    //CMsg *msg = (CMsg *) pMsg;

    pFrame->FRAME_LOG( LOG_DEBUG, "Overload.\n" );

    rsp_pkg pkg;
    pkg.mydata = OVERLOAD;
    pkg.level = SERVER;


    blob_type rspblob;
    rspblob.data = (char *)&pkg;
    rspblob.len = sizeof(pkg);
	cout << "fini：send to client. data  mydata:" << pkg.mydata
		 << "level:" << pkg.level << endl;
    pMsg->SendToClient(rspblob);

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
    	#if 0
		/*自动化生成的是同步的框架，这里改成异步的*/
        /* 初始化框架 */
        int iRet = CSyncFrame::Instance()->InitFrame(base, 100000);
        if (iRet < 0)
        {
            base->log_.LOG_P_PID(LOG_FATAL, "Sync framework init failed, ret:%d\n", iRet);
            return -1;
        }
		
		/* 业务自身初始化 */
		// ......
		#endif
		
        CPollerUnit* pPollerUnit = SPP_ASYNC::GetPollerUnit();
        CTimerUnit* pTimerUnit = SPP_ASYNC::GetTimerUnit();

        base->log_.LOG_P(LOG_DEBUG, "init AsyncFrame, PollerUnit: %p, TimerUnit: %p\n",
                pPollerUnit, pTimerUnit);

        CAsyncFrame::Instance()->InitFrame(base, pPollerUnit, pTimerUnit, 100); 
       
        /*初始化异步框架*/
        CAsyncFrame::Instance()->InitFrame2(base, 100, 0); 

        CAsyncFrame::Instance()->RegCallback(CAsyncFrame::CBType_Init, Init);
        CAsyncFrame::Instance()->RegCallback(CAsyncFrame::CBType_Fini, Fini);
        CAsyncFrame::Instance()->RegCallback(CAsyncFrame::CBType_Overload, OverloadProcess);

        //CAsyncFrame::Instance()->AddState(STATE_WAITING, new CWaitingState);
        CAsyncFrame::Instance()->AddState(STATE_RECV_JOB, new CRecvJobState);
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
    blob_type* blob = (blob_type*)arg1;
    TConnExtInfo* extinfo = (TConnExtInfo*)blob->extdata;
    CServerBase* base = (CServerBase*)arg2;

    base->log_.LOG_P(LOG_DEBUG, "spp_handle_input, %d, %d, %s\n",
                     flow,
                     blob->len,
                     inet_ntoa(*(struct in_addr*)&extinfo->remoteip_));

	cout << "handle input! flow:" << flow << " data:" << blob->data << endl;

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
	//服务器容器对象
    CServerBase* base = (CServerBase*)arg2;
    base->log_.LOG_P(LOG_DEBUG, "spp_handle_route, %d\n", flow);
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

	char reply[40] = "this is reply to admin";
    base->log_.LOG_P_PID(LOG_DEBUG, "spp_handle_process, %d, %d, %s\n",
                         flow,
                         blob->len,
                         inet_ntoa(*(struct in_addr*)&extinfo->remoteip_));

    /* 简单的单发单收模型示例  */
    replyMsg *msg = new replyMsg;
    if (!msg) 
	{
        blob_type respblob;
        respblob.data  = NULL;
        respblob.len   = 0;
        commu->sendto(flow, &respblob, NULL);
        base->log_.LOG_P_PID(LOG_ERROR, "close conn, flow:%u\n", flow);

        return -1;
    }

	cout << "recv data:" << blob->data << endl;
    /* 设置m信息*/
    msg->SetServerBase(base);
    msg->SetTCommu(commu);
    msg->SetFlow(flow);
	//msg->SetInfoFlag(true);
    //msg->SetMsgTimeout(100);
    msg->SetMsgTimeout(0);
    msg->SetReqPkg(reply, sizeof(reply)); /* 微线程有独立空间,这里要拷贝一次报文 */

    CSyncFrame::Instance()->Process(msg);

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
        CSyncFrame::Instance()->Destroy();
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
