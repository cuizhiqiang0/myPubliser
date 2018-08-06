/*
 * =====================================================================================
 *
 *       Filename:  JobPublish.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/21/2010 03:39:54 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  ericsha (shakaibo), ericsha@tencent.com
 *        Company:  Tencent, China
 *
 * =====================================================================================
 */
#include <iostream>
#include <list>
#include <sys/socket.h>  
#include <netinet/in.h>
#include <arpa/inet.h>

#include "replyMsg.h"
#include "JobPublish.h"
#include "JobInfo.h"
#include "ActionInfo.h"
#include "AsyncFrame.h"
#include "CommDef.h"

using namespace std;
extern char *format_time( time_t tm);

int CJobPublise::HandleEncode(CAsyncFrame *pFrame,
        CActionSet *pActionSet,
        CMsgBase *pMsg) 
{
    CMsg *ptrmsg = (CMsg *)pMsg;
    printf("ip<%s>\n",ptrmsg->client_ip);
    printf("port<%d>", ptrmsg->client_port);
    
    return 0;
    #if 0
    //list<CONCLIENT>::iterator itera;
    int i = 1;
    cout << "JobPublish connected num:" << gConnectClient.size() << endl;
    static CJobInfo jobData;

    pFrame->FRAME_LOG( LOG_DEBUG, "JobPublise：find client. ");    
    cout << "JobPublise：find client " << endl;
    
    CActionInfo *pAction = new CActionInfo(512);
    pAction->SetID(i);
    pAction->SetDestIp("10.123.5.46");
    pAction->SetDestPort(9244);
    pAction->SetProto(ProtoType_TCP);
    pAction->SetActionType(ActionType_SendRecv_KeepAliveWithPending);
    pAction->SetTimeout(200);
    pAction->SetIActionPtr((IAction *)&jobData);

    pActionSet->AddAction(pAction);
     
    return 0;
    /*向所有连接的客户端发包*/
    #endif
}

int CJobPublise::HandleProcess(CAsyncFrame *pFrame,
        CActionSet *pActionSet,
        CMsgBase *pMsg)
{
    //CMsg *msg = (CMsg *)pMsg;
    pFrame->FRAME_LOG( LOG_DEBUG, "HandleProcess:");
    cout << "HandleProcess: state recv heart done" << endl;
    
    return STATE_FINISHED;
}
