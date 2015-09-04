#ifndef __COMMON_H__
#define __COMMON_H__

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <sstream>
#include <fcntl.h>
#include <json/json.h>

using namespace std;
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

int make_socket_non_blocking (int sfd)  
{  
  int flags, s;  
  
  //得到文件状态标志  
  flags = fcntl (sfd, F_GETFL, 0);  
  if (flags == -1)  
    {  
      perror ("fcntl");  
      return -1;  
    }  
  
  //设置文件状态标志  
  flags |= O_NONBLOCK;  
  s = fcntl (sfd, F_SETFL, flags);  
  if (s == -1)  
    {  
      perror ("fcntl");  
      return -1;  
    }  
  
  return 0;  
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

/***********************************/
/*** inet_aton和inet_ntoa非线程安全 ***/
/***********************************/
// int ip_str2num(const string& s)
// {
// 	struct in_addr tmp_addr;

// 	if( inet_aton(s.c_str(),&tmp_addr))
// 	{
// 		int ret_ip;
// 		ret_ip = ntohl(tmp_addr.s_addr);
// 		return ret_ip;
// 	}
// 	return -1;
// }

// string ip_num2str(unsigned int ip)
// {
// 	struct in_addr tmp_addr;
// 	tmp_addr.s_addr ＝ htonl(ip);
	
// 	string str;
// 	str.assign(inet_ntoa(tmp_addr));
// 	return str;
// }

unsigned int ip_str2num(const string str)
{
	struct in_addr tmp_addr;
	if( inet_pton(AF_INET, str.c_str(), (void*)&tmp_addr) != 0)
	{
		unsigned int ret_ip;
		ret_ip = ntohl(tmp_addr.s_addr);
		return ret_ip;
	}
	return -1;

}

string ip_num2str(unsigned int ip)
{
	struct in_addr tmp_addr;
	tmp_addr.s_addr = htonl(ip);

	char ipbuf[20];
	inet_ntop(AF_INET, (void *)&tmp_addr, ipbuf, 20);

	string str;
	str.assign(ipbuf);
	return str;
}

template <typename T>
T from_str(const string& str)
{
	istringstream s(str);
	T t;
	s>>t;
	return t;
}

template <typename T>
string to_str(const T& t)
{
	ostringstream s;
	s<<t;
	return s.str();
}
#endif
