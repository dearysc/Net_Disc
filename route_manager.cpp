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

	if( pthread_create(&thread_id,NULL,maintain_thread,(void*)this) != 0 )
		exit(-1);

	if( pthread_create(&thread_id,NULL,cellinfo_thread,(void*)this) != 0 )
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
	memset(celladdr_buf,0,sizeof celladdr_buf);

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

	memcpy(&t, buf, sizeof t);
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

	memcpy(&t, buf, sizeof t);
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

	memcpy(&t, buf, sizeof t);
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

	connect(sockfd, (struct sockaddr*)&access_addr, sizeof access_addr);

	ipset_t* access_buf = new ipset_t[_route_table.size()];

	ipset_t* p = access_buf;
	for(int i = 0; i < _route_table.size(); i++)
	{
		p[i] = _route_table[i];
		p++;
	}

	int ret = write(sockfd,(void*)access_buf,sizeof(access_buf));
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



