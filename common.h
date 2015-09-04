#ifndef __COMMON_H__
#define __COMMON_H__

#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <json/json.h>

using namespace std;
int create_bind_socket(const char* port);

int make_socket_non_blocking (int sfd);

void split(std::string& s, std::string& delim,std::vector< std::string >* ret);

unsigned int ip_str2num(const string str);

string ip_num2str(unsigned int ip);

template <typename T>
T from_str(const string& str);

template <typename T>
string to_str(const T& t);

#endif
