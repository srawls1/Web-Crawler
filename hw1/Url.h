// Spencer Rawls
#ifndef URL_H
#define URL_H
#include <string>

class Url
{
	string scheme;
	string domain;
	int port;
	string path;
	string query;
public:
	string get_scheme() const;
	string get_domain() const;
	in_addr get_ip() const;
	int get_port() const;
	string get_base_url() const;
	string get_path() const;
	string get_query() const;
	string get_request() const;
	Url(char* url);
	Url();
};

#endif