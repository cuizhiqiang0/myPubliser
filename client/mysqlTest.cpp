#include <mysql/mysql.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#define IF_NAME "eth1"
using namespace std;

#define IP_SIZE     16  
// 获取本机ip  
int get_local_ip(const char *eth_inf, char *ip)  
{  
    int sd;  
    struct sockaddr_in sin;  
    struct ifreq ifr;  
  
    sd = socket(AF_INET, SOCK_DGRAM, 0);  
    if (-1 == sd)  
    {  
        printf("socket error: %s\n", strerror(errno));  
        return -1;        
    }  
  
    strncpy(ifr.ifr_name, eth_inf, IFNAMSIZ);  
    ifr.ifr_name[IFNAMSIZ - 1] = 0;  
      
    // if error: No such device  
    if (ioctl(sd, SIOCGIFADDR, &ifr) < 0)  
    {  
        printf("ioctl error: %s\n", strerror(errno));  
        close(sd);  
        return -1;  
    }  
  
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));  
    snprintf(ip, IP_SIZE, "%s", inet_ntoa(sin.sin_addr));  
  
    close(sd);  
    return 0;  
}

void my_insert()
{
    MYSQL mysql;
    if (NULL == mysql_init(&mysql))
    {
        printf("mysql init failed\n");
    }
    if (!mysql_real_connect(&mysql, "10.242.170.126", "root", "123456", "taskPublish", 0, NULL, 0))
    {
        printf("connect mysql failed\n");
    }
    string sql = "insert into T_connect(client_ip, client_port, server_ip, client_state) values ('2.2.2.2', 676, '10.242.170.126', 0);";
    if (!mysql_query(&mysql, sql.c_str()))
    {
        printf("query1 failed\n");
    }
    mysql_close(&mysql);
}
void my_update()
{
    MYSQL mysql;
    if (NULL == mysql_init(&mysql))
    {
        printf("mysql init failed\n");
    }

    if (!mysql_real_connect(&mysql, "10.242.170.126", "root", "123456", "taskPublish", 0, NULL, 0))
    {
        printf("connect mysql failed\n");
    }

    string sql = "insert into T_connect(client_ip, client_port, server_ip, client_state) values ('2.2.2.2', 777, '10.242.170.126', 0);";
    if (!mysql_query(&mysql, sql.c_str()))
    {
        printf("query1 failed\n");
    }

    sql = "update T_connect set client_state = 1 where client_port = 777;";
    if (!mysql_query(&mysql, sql.c_str()))
    {
        printf("query2 failed\n");
    }
    mysql_close(&mysql);

}

void my_query()
{
    MYSQL mysql;
    MYSQL_RES *result = NULL;

    if (NULL == mysql_init(&mysql))
    {
        printf("mysql init failed\n");
    }
    if (!mysql_real_connect(&mysql, "10.242.170.126", "root", "123456", "taskPublish", 0, NULL, 0))
    {
        printf("connect mysql failed\n");
    }
    string sql = "select * from T_connect;";
    if (!mysql_query(&mysql, sql.c_str()))
    {
        printf("query failed\n");
    }
    result = mysql_store_result(&mysql);
    if (NULL == result)
    {
        printf("use result failed\n");
    }
    int rowcount = mysql_num_rows(result);
    cout << "rowcount" << rowcount << endl;
    mysql_close(&mysql);
}
void updateT_execute(char *taskId, char *client_ip, char *client_port, int newretryTimes)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream sqltmp;
    string sql;
    int ret,i;

     mysql_init(&mysql);
     if(!mysql_real_connect(&mysql,"10.242.170.126","root",
                     "123456","taskPublish",0,NULL,0))
     {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
     }
     else
     {
        printf("Connected........");
     }

    sqltmp << "update T_execute set taskState = " << "1, retryTimes = " << newretryTimes << " where taskId = " << taskId << \
                                                " ip = " << client_ip << " port = " << client_port << ";" << endl;
    sql = sqltmp.str();
    cout << "updateT_execute:sql:" << sql.c_str() << endl;
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("updateT_execute:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    { 
         printf("updateT_execute:update T-execute certainip success\n");
    }

      mysql_close(&mysql);
}

void insertT_execute(char *taskId, char *client_ip, char *client_port)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream sqltmp;
    string sql;
    int ret,i;

     mysql_init(&mysql);
     if(!mysql_real_connect(&mysql,"10.242.170.126","root",
                     "123456","taskPublish",0,NULL,0))
     {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
     }
     else
     {
        printf("Connected........");
     }

    sqltmp << "insert into T_execute (taskId, ip, port) values (" << taskId << ", '" << client_ip << "', " << client_port << ");" << endl;
    sql = sqltmp.str();
    cout << "insertT_execute:sql:" << sql.c_str() << endl;
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("insertT_execute:update failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    { 
         printf("insertT_execute:update T-execute certainip success\n");
    }

    mysql_close(&mysql);
}

void task_publish_to_client_insert_T_execute(char *taskId, char *client_ip, char* client_port)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream  sqlselect;
    string sql;
    int ret;
    bool exist = false;
    int taskState, retryTimes;

     mysql_init(&mysql);
     if(!mysql_real_connect(&mysql,"10.242.170.126","root",
                     "123456","taskPublish",0,NULL,0))
     {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
     }
     else
     {
        printf("Connected........\n");
     }
    sqlselect << "select * from T_execute where taskId = " << taskId << " and  ip = '" << client_ip << "' and port = " << client_port << ";" << endl;
    sql = sqlselect.str();
    cout << "task_publish_to_client_insert_T_execute:sql:" << sql.c_str() << endl; 
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("task_publish_to_client_insert_T_execute:select failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    {
        res = mysql_use_result(&mysql);
        if(NULL != res)
        {
            /*如果这个地方已经存在这个的记录，那么去查看这个记录的状态，如果是waiting或者success就不用管，如果是失败的话次数小于三就再下发一次，并把state改成waiting*/
            while (row = mysql_fetch_row(res))
            {
                exist = true;
                /*判断 state和重做的次数*/
                printf("taskstate<%s>,retrytimes<%s>\n", row[3], row[4]);
              
                sscanf(row[3], "%d", &taskState);
                sscanf(row[4], "%d", &retryTimes);
                printf("existed task taskState<%d>, retrytimes<%d>\n", taskState, retryTimes);
                if ((taskState == 2) && (retryTimes < 3))
                {
                    /*再下发一次，同时retrytimes +1， state 改成waiting*/
                    //publish(client_ip, client_port, taskType, taskString);
                    updateT_execute(taskId, client_ip, client_port, retryTimes + 1);
                }
                
            }
            /*不存在这个记录，那么我们就添加*/
            if (false == exist)
            {
                /*下发任务，并且增天记录*/
                //publish(client_ip, client_port, taskType, taskString);
                insertT_execute(taskId, client_ip, client_port);
            }
            mysql_free_result(res);
        }
        else 
        {
            printf("task_publish_to_client_insert_T_execute:res null\n");
            mysql_free_result(res);
        }
    }
    
    mysql_close(&mysql);
}
void task_publish_to_client( char *taskId, char *client_ip, char *local_ip)
{
    MYSQL mysql;
    MYSQL_RES *res;
    MYSQL_ROW row;
    stringstream sqltmp;
    string sql;
    int ret;

    mysql_init(&mysql);
     if(!mysql_real_connect(&mysql,"10.242.170.126","root",
                     "123456","taskPublish",0,NULL,0))
     {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
     }
     else
     {
        printf("Connected........\n");
     }

    if (NULL == client_ip)
    {
        sqltmp << "select * from T_connect where server_ip = '" << local_ip << "';" << endl;
    }
    else if (NULL != client_ip)
    {
        sqltmp << "select * from T_connect where client_ip = '" << client_ip << "' and server_ip = '" << local_ip << "';" << endl;
    }
    
    sql = sqltmp.str();
    cout << "task_publish_to_client:sql:" << sql.c_str() << endl; 
    ret = mysql_query(&mysql, sql.c_str());
    if (0 != ret)
    {
        printf("task_publish_to_client:select failed, ret<%d>,<%s>\n", ret, mysql_error(&mysql));
    }
    else
    {
            res = mysql_use_result(&mysql);
            if(NULL != res)
            {
                while (row = mysql_fetch_row(res))
                {
                    /*对这个client下发任务，同时把这条记录增加到T_execute的数据表里*/
                    printf("task_publish_to_client:clientip<%s>, clientPort<%s>\n", row[1], row[2]);
                    task_publish_to_client_insert_T_execute(taskId, row[1], row[2]);
                }
                mysql_free_result(res);
            }
            else 
            {
                printf("task_publish_to_client:res null\n");
                mysql_free_result(res);
            }
    }

    mysql_close(&mysql);
}
void first_test()
{
     MYSQL mysql;
     MYSQL_RES *res;
     MYSQL_ROW row;
     char ip[32] = {20} ;
     stringstream ss, sqltmp;
     string sql;
     int ret;
     mysql_init(&mysql);
     if(!mysql_real_connect(&mysql,"10.242.170.126","root",
                     "123456","taskPublish",0,NULL,0))
     {
        printf("Error connecting to database:%s\n",mysql_error(&mysql));
     }
     else
     {
        printf("Connected........");
     }

     ss << "select * from T_addtask where taskState = 1" <<endl;
     cout << ss.str() << endl;
     sql = ss.str();
     ret = mysql_query(&mysql, sql.c_str());
     if(ret)
     {
         printf("Error making query:<%s>, ret<%d>\n",mysql_error(&mysql), ret);
     }
     else
     {
         printf("Query made ....t<%d>\n", ret);
         res = mysql_use_result(&mysql);
         if(NULL != res)
         {
            printf("res not null\n");

            get_local_ip(IF_NAME, ip);
            if(0 != strcmp(ip, ""))
            {
                printf("%s ip is %s\n",IF_NAME, ip);
            }

            while (row = mysql_fetch_row(res))
            {
               if (NULL == row)
                {
                    break;
                }
                /*开始增添工作，指定ip，对这个ip的客户端下发任务, 不指定ip就对所有的客户端发包*/
                task_publish_to_client(row[0], row[3], ip);
            }
            mysql_free_result(res);
         }
         else 
         {
             printf("res null\n");
             mysql_free_result(res);
         }
     }
     mysql_close(&mysql);

}
int main()
{

    first_test();
    
    return 0;
 }
