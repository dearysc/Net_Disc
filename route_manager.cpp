// #include <iostream>
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

#include "common.h"

#define BUFLEN (1024)
using namespace std;

typedef struct
{
	int ip1;
	int ip2;
	int ip3;
}ipset_t;

void * thread_func(void *p);

class CRoute_manager
{
public:
	CRoute_manager();
	~CRoute_manager();
private:
	int access_iplist[1];
	int set_num;
	pthread_t thread_id;


	ipset_t* pRoot_tables;

public:
	int server_fd;
	char ipbuf[BUFLEN];
	int init();
};

CRoute_manager::CRoute_manager():set_num(1)
{
	if( (pRoot_tables = (ipset_t*) malloc(sizeof(ipset_t)*set_num)) == NULL )
		exit(-1);

	if( (server_fd = create_bind_socket("8080")) == -1 )
		exit(-1);
	
	if( pthread_create(&thread_id,NULL,thread_func,(void*)this) != 0 )
		exit(-1);

}

int CRoute_manager::init()
{
	if(listen(server_fd, 10) == -1)
		return -1;

	return 0;

}

void* thread_func(void *p)
{
	pthread_detach(pthread_self());
	CRoute_manager* pCRoute = (CRoute_manager*)p;
	struct sockaddr in_addr;  
    socklen_t in_len;  
    int infd;  
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];  
    in_len = sizeof in_addr; 

	while(1)
	{
        
        infd = accept (pCRoute->server_fd, &in_addr, &in_len);  
        if(infd == -1)
        {
        	perror("accept error!\n");
        	continue;
        }
        int ret; 
		ret = getnameinfo (&in_addr, in_len,  
                         hbuf, sizeof hbuf,  
                         sbuf, sizeof sbuf,  
                         NI_NUMERICHOST | NI_NUMERICSERV);//flag参数:以数字名返回

		if (ret == 0)  
        {  
          printf("Accepted connection on descriptor %d "  
                 "(host=%s, port=%s)\n", infd, hbuf, sbuf);  
        }

        read(infd, pCRoute->ipbuf, BUFLEN);
        
	}
}