/*
 * =====================================================================================
 *
 *       Filename:  GetInfo.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/21/2010 03:04:10 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  ericsha (shakaibo), ericsha@tencent.com
 *        Company:  Tencent, China
 *
 * =====================================================================================
 */

#include "JobInfo.h"
#include "replyMsg.h"
#include "sppincl.h"
#include <stdio.h>


// 请求打包
int CJobInfo::HandleEncode( CAsyncFrame *pFrame,
        char *buf,
        int &len,
        CMsgBase *pMsg)
{
    /*这个地方要注意len的长度，如果小于我们需要的可以返回一个长度，然后会再次调用*/ 
    CMsg *msg = (CMsg*)pMsg;
	if (len < msg->input_byte_len)
    {
 	   return msg->input_byte_len;
    } 
    memcpy(buf, msg->input_buff, msg->input_byte_len);
    len = msg->input_byte_len;

    printf("CJobInfo::HandleEncode. send len: %d\n", len);
	
    pFrame->FRAME_LOG(LOG_DEBUG, "CJobInfo::HandleEncode. send len: %d\n", len);
    return 0;
}

// 回应包完整性检查
int CJobInfo::HandleInput( CAsyncFrame *pFrame,
        const char *buf,
        int len,
        CMsgBase *pMsg)
{
    CMsg *msg = (CMsg*)pMsg;
    printf("CJobInfo::HandleInput. buf len: %d;\n", len);
    pFrame->FRAME_LOG( LOG_DEBUG, "CJobInfo::HandleInput.buf:%s, CMSinput_buf:%s, CMSoutput_buf:%s, len: %d;\n",\
        buf, msg->input_buff, msg->recv_buff, len);

    /*这里的完整性检查要根据报文的情况，可能收到回复的长度不对*/
    if(len == msg->input_byte_len)
        return len;

    return 0;
}

// 回应包处理
int CJobInfo::HandleProcess( CAsyncFrame *pFrame,
        const char *buf,
        int len,
        CMsgBase *pMsg)
{
    CMsg *msg = (CMsg*)pMsg;
    char prefix[] = "\nGetInfo Recv: ";

    printf("msg recv_buf:%s, buf%s", msg->recv_buff, buf);
    pFrame->FRAME_LOG(LOG_DEBUG, "CJobInfo::Handleprocess recvbuf:%s || buf:%s\n", 
                                  msg->recv_buff, buf);

    memcpy(&(msg->recv_buff[msg->recv_byte_count]), prefix, strlen(prefix));
    msg->recv_byte_count += strlen(prefix);
    memcpy(&(msg->recv_buff[msg->recv_byte_count]), buf, len);
    msg->recv_byte_count += len;
    msg->recv_buff[msg->recv_byte_count] = '\0';

    printf( "CJobInfo::HandleProcess. buf len: %d; %s\n", 
            len, msg->recv_buff );
    pFrame->FRAME_LOG(LOG_DEBUG, "CJobInfo::HandleProcess. buf len: %d;\n", 
            len);
            
    return 0;
}


int CJobInfo::HandleError( CAsyncFrame *pFrame,
        int err_no,
        CMsgBase *pMsg)
{
    printf( "CJobInfo::HandleError. errno: %d\n", err_no);
    pFrame->FRAME_LOG( LOG_ERROR, "CJobInfo::HandleError. errno: %d\n", err_no);
    return 0;
}
