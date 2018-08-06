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

#ifndef __JOBPUBLISH_H__
#define __JOBPUBLISH_H__
#include "IState.h"
#include "replyMsg.h"
#include <list>

USING_ASYNCFRAME_NS;

extern list<CONCLIENT> gConnectClient;

class CJobPublise
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
