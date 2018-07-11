/*
 * =====================================================================================
 *
 *       Filename:  GetState.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/21/2010 03:40:16 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  ericsha (shakaibo), ericsha@tencent.com
 *        Company:  Tencent, China
 *
 * =====================================================================================
 */

#ifndef __WAITINGSTATE_H__
#define __WAITINGSTATE_H__
#include "IState.h"

USING_ASYNCFRAME_NS;
#define SERVER_IP "10.123.5.46"
#define SERVER_PORT 5574

class CWaitingState
    : public IState
{
    public:
        virtual int HandleEncode(CAsyncFrame *pFrame,
                CActionSet *pActionSet,
                CMsgBase *pMsg) ;

        virtual int HandleProcess(CAsyncFrame *pFrame,
                CActionSet *pActionSet,
                CMsgBase *pMsg) ;
};

#endif
