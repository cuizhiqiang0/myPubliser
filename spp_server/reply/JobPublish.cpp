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

extern list<CONCLIENT> gConnectClient;

int CJobPublise::HandleEncode(CAsyncFrame *pFrame,
        CActionSet *pActionSet,
        CMsgBase *pMsg) 
{
	list<CONCLIENT>::iterator itera;
	int i = 1;
    /*向所有连接的客户端发包*/
	for (itera = gConnectClient.begin(), i = 1; itera != gConnectClient.end(); itera++)
	{
		if (NORMAL_CLIENT == itera->client_type)
	 	{
			static CJobInfo jobData;
		
			CActionInfo *pAction = new CActionInfo(512);
		    pAction->SetID(i);
		    pAction->SetDestIp(inet_ntoa(*(struct in_addr*)&(itera->client_ip)));
		    pAction->SetDestPort(itera->clienr_port);
		    pAction->SetProto(ProtoType_TCP);
		    pAction->SetActionType(ActionType_SendRecv_KeepAliveWithPending);
		    pAction->SetTimeout(200);
		    pAction->SetIActionPtr((IAction *)&jobData);

   			pActionSet->AddAction(pAction);
		}
		
	}
    #if 0
    static CUpdateData UpdateData;

    CActionInfo *pAction1 = new CActionInfo(512);
    pAction1->SetID(3);
    pAction1->SetDestIp("172.25.0.29");
    pAction1->SetDestPort(5575);
    pAction1->SetProto(ProtoType_TCP);
    pAction1->SetActionType(ActionType_SendRecv_KeepAliveWithPending);
    pAction1->SetTimeout(200);
    pAction1->SetIActionPtr((IAction *)&UpdateData);

    pActionSet->AddAction(pAction1);
    #endif
    return 0;
}

int CJobPublise::HandleProcess(CAsyncFrame *pFrame,
        CActionSet *pActionSet,
        CMsgBase *pMsg)
{
	CMsg *msg = (CMsg *)pMsg;
    pFrame->FRAME_LOG( LOG_DEBUG, "HandleProcess:");
	cout << "HandleProcess:" << endl;
	char replybuf[] = "this is reply";
    memcpy(msg->recv_buff, replybuf, sizeof(replybuf));
	msg->recv_byte_count = sizeof(replybuf);
    return STATE_FINISHED;
    #if 0
    CMsg *msg = (CMsg *)pMsg;
    int err1 = 0 ;
    int cost1 = 0;
    int size1 = 0;

    CActionSet::ActionSet &action_set = pActionSet->GetActionSet();
    CActionSet::ActionSet::iterator it = action_set.begin();
    for(; it != action_set.end(); it++ )
    {
        CActionInfo *pInfo = *it;
        char *buf = NULL;

        int id;
        pInfo->GetID(id);
        
        if( id == 3 )
        {
            pInfo->GetErrno(err1);
            pInfo->GetBuffer( &buf, size1);
            if( err1 == 0 )
            {
                if( size1 == sizeof(int)*3
                        && msg->uin == *(int *)buf )
                {
                    msg->result = *(int*)(buf+sizeof(int)*2);
                }
            }
            else
            {
                msg->result = -1;
                msg->level = -3;
                msg->coin = -3;

                break;
            }

            pInfo->GetTimeCost( cost1 );

        }
    }
    #endif
}
