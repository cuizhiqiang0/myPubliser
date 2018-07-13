#ifndef __REPLY_MSG_H__
#define __REPLY_MSG_H__
#include "MsgBase.h"
#include "syncincl.h"

USING_ASYNCFRAME_NS;

#define STATE_FINISHED 				0
#define STATE_RECV_JOB              1
#if 0
#define STATE_WAITING 				1   //作为守护进程等待任务下发
#define STATE_RECV_JOB 				2	 //接收到管理者下发的一个任务请求
#define STATE_PUBLISH_JOB 			3	 //把任务下发给客户端
#endif
typedef struct rsp_pkg
{
    int mydata; //数据
	int level; //管理员还是客户端
} rsp_pkg;

typedef struct ConnectedClient
{
	unsigned client_ip;
	unsigned clienr_port;
	int 	 client_type;
}CONCLIENT;
enum data
{
	ADMIN_PUB = 1,
	PUBLISH_TO_CLIENT,
	RETURN_TO_ADMIN,
	ERROR_DATA_MAX,
};
enum ClientType
{
	NORMAL_CLIENT = 1,	
	ADMIN_CLIENT,
	ERROR_TYPE_MAX,
};
enum level
{
	CLIENT = 2,
	ADMIN,
	SERVER,
	OVERLOAD,
	ERROR_LEVEL_MAX,
};
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

class CMsg
    : public CMsgBase
{
    public:
        CMsg(): input_byte_len(0), recv_byte_count(0) {};
        char input_buff[1024]; //管理员的输入
        int input_byte_len;
        char recv_buff[4096]; //客户端回复的数据
        int recv_byte_count;
};


class replyMsg : public CSyncMsg 
{
public:
    replyMsg(){}
    /**
     * @brief 同步消息处理函数
     * @return 0, 成功-用户自己回包到前端,框架不负责回包处理
     *         其它, 失败-框架关闭与proxy连接, 但不负责回业务报文
     */
    virtual int HandleProcess(); 
	
};
#endif
