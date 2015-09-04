// #include <iostream>

#include "route_manager.h"

CRoute_manager::CRoute_manager()
{
	readconf("./conf.json");

	if( (maintain_servfd = create_bind_socket("8080")) == -1 )
		exit(-1);

	if(listen(maintain_servfd, 10) == -1)
		exit(-1);

	if( (info_fd = create_bind_socket("9090")) == -1 )
		exit(-1);

	if(make_socket_non_blocking(info_fd) == -1)
		exit(1);      

	if(listen(info_fd, SOMAXCONN) == -1)
		exit(-1);

	if( pthread_create(&mt_thread_id,NULL,maintain_thread,(void*)this) != 0 )
		exit(-1);

	if( pthread_create(&info_thread_id,NULL,cellinfo_thread,(void*)this) != 0 )
		exit(-1);

	// if( pthread_create(&thread_id,NULL,request_cellinfo_thread,(void*)this) != 0 )
	// 	exit(-1);
	

}

CRoute_manager::~CRoute_manager()
{

}


void CRoute_manager::readconf(const char* filepath)
{
	Json::Reader reader;
	Json::Value root;

	ifstream is;

	is.open(filepath, ios::binary);
	if(reader.parse(is,root))
	{
		int set_num = root["cell_addr"].size();
		
		int seg = RANGE / set_num;
		for (int i = 0 ; i < set_num ; i++)
		{
			ipset_t tmp;
			tmp.lower_bounds = i * seg;
			tmp.upper_bounds = (i+1) * seg;
			tmp.ip1 = ip_str2num( root["cell_addr"][i]["IP1"].asString() );
			tmp.ip2 = ip_str2num( root["cell_addr"][i]["IP2"].asString() );
			tmp.ip3 = ip_str2num( root["cell_addr"][i]["IP3"].asString() );
			
			if( i == set_num-1)
				tmp.upper_bounds = RANGE;

			_route_table.push_back(tmp);
		}

		access_iplist[0] = root["Access_ip"].asString();
		access_portlist[0] = root["Access_port"].asInt();
	}

	is.close();
	
}

vector<ipset_t>::iterator CRoute_manager::search_bounds(const string& ip, unsigned int& lower,unsigned int& upper)
{
	unsigned int ipnum;
	ipnum = ip_str2num(ip);

	vector<ipset_t>::iterator it;
	for( it = _route_table.begin(); it != _route_table.end(); it++)
	{
		if ( (*it).ip1 == ipnum || (*it).ip2 == ipnum || (*it).ip3 == ipnum )
		{
			lower = (*it).lower_bounds;
			upper = (*it).upper_bounds;
			return it;
		}

	}
	
}


int CRoute_manager::parse_moverequest(Json::Value root)
{
	memset(celladdr_buf,0,sizeof(celladdr_buf));

	string tmp;

	tmp = root["old_ip"]["IP1"].asString();
	celladdr_buf[0].sin_family = AF_INET;
	celladdr_buf[0].sin_port = htons(CELLPORT);
	inet_pton(AF_INET, tmp.c_str(), &celladdr_buf[0].sin_addr);

	tmp = root["old_ip"]["IP2"].asString();
	celladdr_buf[1].sin_family = AF_INET;
	celladdr_buf[1].sin_port = htons(CELLPORT);
	inet_pton(AF_INET, tmp.c_str(), &celladdr_buf[1].sin_addr);

	tmp = root["old_ip"]["IP3"].asString();
	celladdr_buf[2].sin_family = AF_INET;
	celladdr_buf[2].sin_port = htons(CELLPORT);
	inet_pton(AF_INET, tmp.c_str(), &celladdr_buf[2].sin_addr);
	
	if( (flag = root["flag"].asInt() ) == 1)    //表示扩容
	{    	
		unsigned int lower,upper;
		from_set = search_bounds(tmp,lower,upper);
		range_mid = (lower + upper) / 2;
	}
	else if( (flag = root["flag"].asInt() ) == -1)  //表示缩容
	{
		unsigned int lower,upper;
		from_set = search_bounds(tmp,lower,upper);
		to_set = search_bounds(root["new_ip"]["IP1"].asString(),lower,upper);
		range_mid = (*from_set).lower_bounds;
	}

	move_req[0].new_ip = ip_str2num( root["new_ip"]["IP1"].asString() );
	move_req[1].new_ip = ip_str2num( root["new_ip"]["IP2"].asString() );
	move_req[2].new_ip = ip_str2num( root["new_ip"]["IP3"].asString() );

	move_req[0].new_range = range_mid;
	move_req[1].new_range = range_mid;
	move_req[2].new_range = range_mid;

	return 0;
}


bool CRoute_manager::do_datamove_request()
{
	bool all_done = false;

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	char buf[20];
	int t; 
	int success_times = 0;
	connect(sockfd, (struct sockaddr*)&celladdr_buf[0] , sizeof(celladdr_buf[0]));
	if( write(sockfd, (void*)&move_req[0], sizeof(move_req[0])) <= 0 )
	{
		perror("write error!");
		close(sockfd);
	}
	
	if (read(sockfd,buf,20) <= 0)
	{
		perror("read error!");
		close(sockfd);
	}

	memcpy(&t, buf, sizeof(t));
	if( t == 1)
		success_times++;
	close(sockfd);
	///****************************************************///
	connect(sockfd, (struct sockaddr*)&celladdr_buf[1] , sizeof(celladdr_buf[1]));
	if( write(sockfd, (void*)&move_req[1], sizeof(move_req[1])) <= 0 )
	{
		perror("write error!");
		close(sockfd);
	}
	
	if (read(sockfd,buf,20) <= 0)
	{
		perror("read error!");
		close(sockfd);
	}

	memcpy(&t, buf, sizeof (t));
	if( t == 1)
		success_times++;
	close(sockfd);
	///****************************************************///
	connect(sockfd, (struct sockaddr*)&celladdr_buf[2] , sizeof(celladdr_buf[2]));
	if( write(sockfd, (void*)&move_req[2], sizeof(move_req[2])) <= 0 )
	{
		perror("write error!");
		close(sockfd);
	}
	
	if (read(sockfd,buf,20) <= 0)
	{
		perror("read error!");
		close(sockfd);
	}

	memcpy(&t, buf, sizeof (t));
	if( t == 1)
		success_times++;
	close(sockfd);
	///****************************************************///
	if(success_times == 3)
		all_done = true;

	if(all_done == true)
	{
		if( flag == 1)
		{
			ipset_t new_set;
			new_set.ip1 = move_req[0].new_ip;
			new_set.ip2 = move_req[1].new_ip;
			new_set.ip3 = move_req[2].new_ip;
			new_set.lower_bounds = range_mid;

			new_set.upper_bounds = (*from_set).upper_bounds;
			(*from_set).upper_bounds = range_mid;
			add_routetable(new_set);
			
		}
		else if(flag == -1)
		{
			(*to_set).upper_bounds = (*from_set).upper_bounds;
			del_routetable(from_set);
		}
	}
	return all_done;
}

void CRoute_manager::add_routetable(ipset_t new_set)
{
	_route_table.push_back(new_set);
}

void CRoute_manager::del_routetable(vector<ipset_t>::iterator it)
{
	_route_table.erase(it);
}


int CRoute_manager::send_table2access(string& access_ip)
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	sockaddr_in access_addr;
	access_addr.sin_family = AF_INET;
	
	access_addr.sin_port = htons(access_portlist[0]);

	inet_pton(AF_INET, access_ip.c_str(), &access_addr.sin_addr);

	connect(sockfd, (struct sockaddr*)&access_addr, sizeof (access_addr));

	ipset_t* access_buf = new ipset_t[_route_table.size()];

	ipset_t* p = access_buf;
	for(int i = 0; i < _route_table.size(); i++)
	{
		p[i] = _route_table[i];
		p++;
	}

	int ret = write(sockfd,(void*)access_buf, sizeof(access_buf));
	if(ret == -1)
	{
		perror("write access error!");
		return -1;
	}
	close(sockfd);
	return 0;
}

string CRoute_manager::prepare_infodata()  // 和运维采用json传递数据
{
	vector<ipset_t>::iterator it;
	Json::Value root;
	for( it = _route_table.begin(); it != _route_table.end() ; it++)
	{
		Json::Value set;
		set["IP1"] = Json::Value( ip_num2str((*it).ip1) );
		set["IP2"] = Json::Value( ip_num2str((*it).ip2) );
		set["IP3"] = Json::Value( ip_num2str((*it).ip3) );

		root["cell_addr"].append(set);
		
	}

	Json::FastWriter writer;
	string jsonstr = writer.write(root);

	return jsonstr;
}


// 接收运维请求
void* maintain_thread(void *p)
{
	// pthread_detach(pthread_self());

	CRoute_manager* pCRoute = (CRoute_manager*)p;
	struct sockaddr in_addr;  
    socklen_t in_len;  
    int infd;  
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];  
    in_len = sizeof (in_addr); 

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
					pCRoute->send_table2access(pCRoute->access_iplist[0]);
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

	events = (epoll_event*)calloc(EVENTSNUM, sizeof(epoll_event));

    whi le(1) 
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
				    in_len = sizeof(in_addr); 
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
				            break;
				        }  
				         
				    }  
				    if(nwrite == 0)  // the other side closed
				    {
				    	event.data.fd = fd;
		                event.events = EPOLLOUT | EPOLL_ET;
						epoll_ctl(efd,EPOLL_CTL_DEL,fd,&event);

						close(fd);
						break;
				    }
				    n -= nwrite;  
				}
				// 重新监听EPOLLIN事件
				if(n == 0)
				{
					event.data.fd = fd;
                	event.events = EPOLLIN | EPOLL_ET;
					epoll_ctl(efd,EPOLL_CTL_MOD,fd,&event);
				}
          	}

        }
    }
}



