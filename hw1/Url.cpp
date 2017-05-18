// Spencer Rawls
#include "stdafx.h"
#include "Url.h"

string Url::get_scheme() const
{
	return scheme;
}

string Url::get_domain() const
{
	return domain;
}

in_addr Url::get_ip() const
{
	int ip = inet_addr(domain.c_str());
	in_addr addr;
	if (ip == INADDR_NONE)
	{
		struct hostent* host = gethostbyname(domain.c_str());
		if (host == NULL)
		{
			addr.s_addr = INADDR_NONE;
			return addr;
		}
		in_addr* address = (in_addr*)host->h_addr;
		return *address;
	}
	addr.s_addr = ip;
	return addr;
}

int Url::get_port() const
{
	return port;
}

string Url::get_base_url() const
{
	return scheme + "://" + domain;
}

string Url::get_path() const
{
	return path;
}

string Url::get_query() const
{
	return query;
}

string Url::get_request() const
{
	return path + query;
}

Url::Url(char* url)
{
	//Separate off scheme
	char* schemeEnd = strstr(url, "://");
	if (schemeEnd == NULL)
	{
		schemeEnd = url;
	}
	else
	{
		*schemeEnd = 0;
		scheme = string(url);
		schemeEnd += 3; // get rid of the ://
	}
	
	//Truncate fragment
	char* fragmentStart = strchr(schemeEnd, '#');
	if (fragmentStart != NULL)
	{
		*fragmentStart = 0;
	}
	else
	{
		fragmentStart = strchr(schemeEnd, 0);
	}

	// Find port, path, and query starts
	char* portStart = strchr(schemeEnd, ':');
	char* pathStart = strchr(schemeEnd, '/');
	char* queryStart = strchr(schemeEnd, '?');

	if (queryStart == NULL)
	{
		queryStart = fragmentStart;
		query = "";
	}
	else
	{
		query = string(queryStart);
		*queryStart = 0;
	}

	if (pathStart == NULL || (long)pathStart > (long)queryStart)
	{
		pathStart = queryStart;
		path = "/";
	}
	else
	{
		path = string(pathStart);
		*pathStart = 0;
	}

	if (portStart == NULL || (long)portStart > (long)pathStart)
	{
		port = 80;
	}
	else
	{
		port = atoi(portStart);
	}

	domain = string(schemeEnd);
}

Url::Url() {} // The data will be filled in later