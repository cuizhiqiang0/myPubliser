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

#ifndef __RECVJOBSTATE_H__
#define __RECVJONSTATE_H__
#include "IState.h"

USING_ASYNCFRAME_NS;
#define MOD_ID  1000
#define CMD_ID  500

class CRecvJobState
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
