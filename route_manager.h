#ifndef __ROUTE_MANAGER_H__
#define __ROUTE_MANAGER_H__

#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <sys/epoll.h>
// #include <algorithm>
#include <json/json.h>

#include "common.h"

#define BUFLEN (1024)
#define RANGE (1000)
#define EVENTSNUM (20)
#define CELLPORT (9999)

using namespace std;

typedef struct     		 // 一个cell存储set
{
	unsigned int lower_bounds;   // SHA%RANGE 下界
	unsigned int upper_bounds;	 // SHA%RANGE 上界
	unsigned int ip1;
	unsigned int ip2;
	unsigned int ip3;
}ipset_t;

typedef struct    		// cell 返回给route的信息，用于提供给运维
{
	unsigned int ip;
	long capacity_all;  //总的容量 
	long capacity_used; //已经用了的容量
}cellinfo_t;

typedef struct
{
	unsigned long new_ip;
	int new_range;
}move_request_t;

/*********************************************************/
class CRoute_manager
{
public:
	CRoute_manager();
	~CRoute_manager();
private:
	string access_iplist[1];   	          // Access服务器的IP
	unsigned int access_portlist[1]			      // Access服务器的PORT
	int set_num;					 	  // Cell服务器集合数，默认3台一个集合
	
	int flag;      					 	  // 记录扩容还是缩容,1---扩容  -1----缩容 
	int range_mid;					 	  // 记录扩容或者缩容过程的中间范围

	vector<ipset_t>::iterator from_set;   // 记录从哪个集合拷贝到哪个集合
	vector<ipset_t>::iterator to_set;

	int maintain_servfd;			 	  // 运维服务socket_fd
	int info_fd;						  // 接受Cell请求的服务socket_fd # 这里可能会改成向Cell请求状态信息
	
	vector<ipset_t> _route_table;		  // 存储route表的容器
	vector<cellinfo_t> _cell_info;        // 存储Cell状态信息的容器

	char mt_recvbuf[BUFLEN];     		  // 接收运维端buffer
	char stat_sendbuf[BUFLEN];			  // 向运维发状态信息buffer
	char infobuf[BUFLEN];
	
	sockaddr_in celladdr_buf[3];		  // 需要进行扩容或者缩容的cell地址
	move_request_t move_req[3];			  // 扩容/缩容请求消息

private:
	void readconf(const char* filepath);  // 读配置文件
	vector<ipset_t>::iterator 			  // 给一个IP,搜索这个IP所属SET的SHA上，下界
		search_bounds(const string& ip,unsigned int& lower,unsigned int& upper);

	void add_routetable(ipset_t );	  				   // 向route表中加入一条记录
	void del_routetable(vector<ipset_t>::iterator );   // 从route表中删除一条记录

	string prepare_infodata();		      // 生成json格式的状态信息，用于发给运维
	int parse_moverequest(Json::Value root);  // 把运维发来的要求扩容或者缩容的请求jsonp字符器解析出来
	bool do_datamove_request();

	int send_table2access(string& access_ip); // 把更新后的route_table发给Access
public:
	pthread_t mt_thread_id;			 	  // 线程id
	pthread_t info_thread_id;
	// 线程函数
	friend void* maintain_thread(void* p);
	friend void* cellinfo_thread(void* p);
	friend void* request_cellinfo_thread(void* p);
	
};



#endif