#ifndef __REPLY_MSG_H__
#define __REPLY_MSG_H__
#include "MsgBase.h"
#include "syncincl.h"

USING_ASYNCFRAME_NS;

#define STATE_FINISHED         0
#define STATE_RECV_HEART       1

/*网卡的名字*/
#define IF_NAME      "eth1"
/*数据库的IP地址*/
#define DB_HOST_IP   "10.242.170.126"
#define SECOND_DB_HOST_IP "10.242.171.104"
/*数据库的用户名*/
#define DB_USER      "root"
/*数据库的密码*/
#define DB_PASSWORD  "!@#$123456"
/*数据库名字*/
#define DB_NAME      "taskPublish"

/*文件的目录*/
#define FILE_DIR     "/data/home/waltercui/file/"

/*报文一次发送的长度*/
#define SIZE             512
/*ip地址的大小*/
#define IP_SIZE          20 
#define FILE_DIR_SIZE    80

#define BASH_RESULT_SIZE 1000
#define BASH_REPLY_SIZE  1500
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

/*任务下发的类型， 目前支持四种*/
enum TASK_TYPE
{
    FILE_PUBLISH = 1,
    CONFIG_UPDATE,
    BASH_STRING,
    BASH_FILE,
};

/*任务下发的结果*/
enum TASK_PUBLISH_ANS
{
    PUBLISH_SUCCESS = 0,
    WRONG_TYPE,
    SEND_TO_CLIENT_FAILED,
    MD5_CALC_ERROR,
    CREATE_SESSION_FAILED,
    FILE_DOESNOT_EXIST,
    NO_MEMORY,
};
    
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
        int port;
        int taskId;
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
