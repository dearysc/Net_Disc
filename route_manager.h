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



// 接收运维请求
void* maintain_thread(void *p)
{
	// pthread_detach(pthread_self());

	CRoute_manager* pCRoute = (CRoute_manager*)p;
	struct sockaddr in_addr;  
    socklen_t in_len;  
    int infd;  
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];  
    in_len = sizeof in_addr; 

	while(1)
	{
        
        infd = accept (pCRoute->maintain_servfd, &in_addr, &in_len);  
        if(infd == -1)
        {
        	perror("accept error!\n");
        	continue;
        }

        int cnt = read(infd, pCRoute->mt_recvbuf, BUFLEN);
        
        if(cnt < 0)
        {
        	perror("read error!");
        	exit(-1);
        }
        else if(cnt == 0)  // thd other side closed
        {
        	close(infd);
        	continue;
        }

        Json::Reader reader;
		Json::Value root;

		if(reader.parse(pCRoute->mt_recvbuf,root))
		{
			if(root["Act"].asString() == string("Update"))
			{
				pCRoute->parse_moverequest(root);
				if( pCRoute->do_datamove_request() )
				{
					send_table2access(access_iplist[0]);
				}

			}

			if(root["Act"].asString() == string("Request"))
			{
				string str = pCRoute->prepare_infodata();
				strcpy(pCRoute->stat_sendbuf,str.c_str());
				write(infd, pCRoute->stat_sendbuf, strlen(str.c_str()));
			}
		}


        
	}
}


// 可能作为服务器接收Cell请求
void * cellinfo_thread(void* p)
{
	CRoute_manager* pCRoute = (CRoute_manager*)p;
 
	struct epoll_event event;
	struct epoll_event* events;
	int efd;

	efd = epoll_create();

	event.data.fd = pCRoute->info_fd;
	event.events = EPOLLIN | EPOLLET;  //设置成边沿触发
	if(epoll_ctl(efd,EPOLL_CTL_ADD,pCRoute->info_fd,&event) == -1)
		exit(-1);

	events = calloc(EVENTSNUM,sizeof epoll_event);

    while(1)
	{
        int n = epoll_wait(efd,events,EVENTSNUM,-1);
        for(int i = 0; i < n; i++)
        {
        	if((events[i].events & EPOLLERR) ||  
              (events[i].events & EPOLLHUP) ||  
              (!(events[i].events & EPOLLIN))) 
        	{
            	perror("epoll error!");
              	close(events[i].data.fd); 
              	continue;
          	}
          	else if(events[i].data.fd == pCRoute->info_fd)
          	{
          		while(1)
          		{
          			struct sockaddr in_addr;  
				    socklen_t in_len;  
				    int infd;  
				    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];  
				    in_len = sizeof in_addr; 
				    infd = accept (pCRoute->info_fd, &in_addr, &in_len); 
				    if (infd == -1)  
                    {  
                      if ((errno == EAGAIN) || (errno == EWOULDBLOCK))  
                        {  
                          /* We have processed all incoming 
                             connections. */  
                          break;  
                        }  
                      else  
                        {  
                          perror ("accept");  
                          break;  
                        }  
                    } 

                    //将地址转化为主机名或者服务名  
	                s = getnameinfo (&in_addr, in_len,  
				                       hbuf, sizeof hbuf,  
				                       sbuf, sizeof sbuf,  
				                       NI_NUMERICHOST | NI_NUMERICSERV);//flag参数:以数字名返回  
				                      //主机地址和服务地址  
	  
	                if (s == 0)  
	                {  
	                	printf("Accepted connection on descriptor %d "  
	                       	  "(host=%s, port=%s)\n", infd, hbuf, sbuf);  
	                }  

	                if(make_socket_non_blocking (infd) == -1)  
	                	exit(-1);

	                event.data.fd = infd;
	                event.events = EPOLLIN | EPOLL_ET;
	                if(epoll_ctl(efd,EPOLL_CTL_ADD,infd,&event) == -1)
	                	exit(-1);
          		}
          		continue;
          	}
          	else if(events[i].events & EPOLLIN)
          	{
          		int n = 0; 
          		int nread; 
          		int fd = events[i].data.fd;

				while ((nread = read(fd, pCRoute->infobuf + n, BUFLEN)) > 0) 
				{  
				    n += nread;  
				}  
				if (nread == -1 && errno != EAGAIN) 
				{  
				    perror("read error"); 
				    continue;
				}  
				if (nread == 0)
				{
					event.data.fd = fd;
	                event.events = EPOLLIN | EPOLL_ET;
					epoll_ctl(efd,EPOLL_CTL_DEL,fd,&event);

					close(fd);
					continue;
				}

				event.data.fd = fd;
                event.events = EPOLLOUT | EPOLL_ET;
				epoll_ctl(efd,EPOLL_CTL_MOD,fd,&event);

          	}
          	else if(events[i].events & EPOLLOUT)
          	{
          		int nwrite, data_size = strlen(pCRoute->infobuf);  
				int n = data_size;  
				int fd = events[i].data.fd;
				while (n > 0) 
				{  
				    nwrite = write(fd, pCRoute->infobuf + data_size - n, n);  
				    if (nwrite < n) 
				    {  
				        if (nwrite == -1 && errno != EAGAIN)
				        {  
				            perror("write error");  
				            continue;
				        }  
				         
				    }  
				    if(nwrite == 0)  // the other side closed
				    {
				    	event.data.fd = fd;
		                event.events = EPOLLOUT | EPOLL_ET;
						epoll_ctl(efd,EPOLL_CTL_DEL,fd,&event);

						close(fd);
						continue;
				    }
				    n -= nwrite;  
				}
				// 重新监听EPOLLIN事件
				event.data.fd = fd;
                event.events = EPOLLIN | EPOLL_ET;
				epoll_ctl(efd,EPOLL_CTL_MOD,fd,&event);
				
          	}

        }
    }
}

#endif