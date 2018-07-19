#include <mysql/mysql.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <sstream>
using namespace std;

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

void first_test()
{
     MYSQL mysql;
     MYSQL_RES *res;
     MYSQL_ROW row;
     char query[] = "select * from T_connect where server_ip = '10.242.170.126';";
     char b[20] = "10.242.170.126";
     char c[20] = "10.242.170.126";
     int state = 6;
     int port = 48310;
     stringstream ss, sqltmp;
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

     ss << "select * from T_connect where client_ip = '" << b <<"'" << "and client_port = " << port<< ";" <<endl;
     cout << ss.str() << endl;
     sql = ss.str();
     ret = mysql_query(&mysql, sql.c_str());
     if(ret)
     {
         printf("Error making query:<%s>, ret<>\n",mysql_error(&mysql), ret);
     }
     else
     {
         printf("Query made ....t<%d>\n", ret);
         res = mysql_use_result(&mysql);
         if(NULL != res)
         {
            printf("res not null\n");
            if (NULL == mysql_fetch_row(res))
		    {
				sqltmp << "insert into T_connect(client_ip, client_port, server_ip, client_state) values ('" << \
					    b << "', " << port << ", '"<< c << "', " << state << ");" << endl;
                cout << sqltmp.str() << endl;
    			sql = sqltmp.str();
                mysql_free_result(res);
				ret = mysql_query(&mysql, sql.c_str());
				if (0 != ret)
			    {
			        printf("instert failed, ret<%d>\n", ret);
			    }
			}
			/*存在这个客户端的记录，则更新连接状态*/
			else
			{
				sqltmp << "update T_connect set client_state = " << state << " where client_ip = '" << b << \
						  "' and client_port = " << port << ";" << endl;
				cout << sqltmp.str() << endl;
    			sql = sqltmp.str();
                mysql_free_result(res);
                ret = mysql_query(&mysql, sql.c_str());
				if (0 != ret)
			    {
			        printf("update failed, ret<%d>, <%s>\n", ret, mysql_error(&mysql));
			    }
			}
            #if 0
            printf("<%s>\n", mysql_fetch_row(res) == NULL ? "NULL" : "NOT NULL");
            while (row = mysql_fetch_row(res))
            {
                if (NULL == row)
                {
                    printf("no more");
                    break;
                }
                for (i = 0; i < mysql_num_fields(res); ++i)
                {
                    printf("%s ", row[i]);
                }
                printf("\n");
            }
            #endif
         }
         else printf("res null\n");
     }
     mysql_close(&mysql);

}
int main()
{

    first_test();
    
    return 0;
 }
