#ifndef __REPLY_MSG_H__
#define __REPLY_MSG_H__
#include "MsgBase.h"
#include "syncincl.h"

USING_ASYNCFRAME_NS;

#define STATE_FINISHED         0
#define STATE_RECV_HEART       1

#define IF_NAME "eth1"

#define SIZE 512

#define IP_SIZE     20 


/*定时器触发去读数据库中新任务的时间间隔毫秒*/
#define TIME_INTERVAL   5000
typedef struct rsp_pkg
{
    int mydata; //数据
    int level; //管理员还是客户端
} rsp_pkg;

typedef struct ConnectedClient
{
    unsigned client_ip;
    unsigned clienr_port;
    int      client_type;
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
    
class myMsg
{
    public:
        char ip[20];
        //int port;
};
class CMsg
    : public CMsgBase
{
    public:
        CMsg(){};
        unsigned char heartBuff[513]; //管理员的输入
        char client_ip[20];
        int client_port;
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
