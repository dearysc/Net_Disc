#ifndef __COMMON_H__
#define __COMMON_H__

#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <stringstream>

int create_bind_socket(const char* port)
{
	struct addrinfo hints;
	struct addrinfo* res,*cur;
	int ret;
	int serverfd;

	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;	

	ret = getaddrinfo(NULL,port,&hints,&res);
	if(ret != 0)
	{
		fprintf(stderr, "getaddrinfo:%s\n",gai_strerror(ret));
		exit(-1);
	}

	for(cur = res;cur != NULL;cur = cur->ai_next)
	{
		serverfd = socket(cur->ai_family,cur->ai_socktype,cur->ai_protocol);
		if(serverfd == -1)
		{
			continue;
		}

		ret = bind(serverfd,cur->ai_addr,cur->ai_addrlen);
		if(ret == 0)
			break;

		close(serverfd);
	}

	if(cur == NULL)
	{
		fprintf(stderr, "can not bind!\n");
		exit(-1);
	}

	freeaddrinfo(res);
	return serverfd;
}

void split(std::string& s, std::string& delim,std::vector< std::string >* ret)  
{  
    size_t last = 0;  
    size_t index=s.find_first_of(delim,last);  
    while (index!=std::string::npos)  
    {  
        ret->push_back(s.substr(last,index-last));  
        last=index+1;  
        index=s.find_first_of(delim,last);  
    }  
    if (index-last>0)  
    {  
        ret->push_back(s.substr(last,index-last));  
    }  
}  

void Parse_JsonStr()
{

}

#endif