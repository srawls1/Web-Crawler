// Spencer Rawls

#include "stdafx.h"
#include "Url.h"
#include "Socket.h"
#include "ProtectedBuffer.h"
#include "ProtectedCount.h"
#include <iostream>

#define PRINT_STD 1
#define PRINT_REQ 2
#define PRINT_UNIQUE 4
#define PRINT_HEAD 8
#define PRINT_REQUEST 16
#define PRINT_BODY 32

#define COND_PRINT(mask, str)\
if (printMask & mask)\
{\
	printf(str);\
}\

#define COND1_PRINT(mask, str, arg1)\
if (printMask & mask)\
{\
	printf(str, arg1);\
}\

#define COND2_PRINT(mask, str, arg1, arg2)\
if (printMask & mask)\
{\
	printf(str, arg1, arg2);\
}\

#define CHECK_RECV(str)\
if (str.compare("timeout") == 0)\
{\
	COND_PRINT(PRINT_STD, "failed with slow download\n");\
	return false;\
}\
if (str.compare("oversize") == 0)\
{\
	COND_PRINT(PRINT_STD, "failed with exceeding max\n");\
	return false;\
}\
if (str.compare("error") == 0)\
{\
	COND1_PRINT(PRINT_STD, "failed with %d on recv\n", WSAGetLastError());\
	return false;\
}\

const string agent_name = "TheBestCrawlerEver/1.3";

int winsock_init()
{
	WSADATA wsadata;
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsadata) != 0)
	{
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}
	return 0;
}

struct shared_data
{
	HANDLE url_semaphore;
	unordered_set<string> hosts;
	HANDLE ip_semaphore;
	unordered_set<string> ips;

	ProtectedBuffer pendingUrls;

	long num_urls = 0;
	long num_host_unique = 0;
	long num_dns = 0;
	long num_ip_unique = 0;
	long num_robot = 0;
	long num_crawled = 0;
	ULONGLONG num_links = 0;
	long num_2xx = 0;
	long num_3xx = 0;
	long num_4xx = 0;
	long num_5xx = 0;
	long num_other = 0;
	ULONGLONG total_size = 0;

	bool fileRunning = true;
	int num_to_crawl = 0;
	string filename;
};

bool parseUrl(char* arg, int printMask, Url& out, shared_data* data)
{
	++data->num_urls;

	COND1_PRINT(PRINT_STD, "URL: %s\n", arg);
	COND_PRINT(PRINT_STD, "        Parsing URL... ");

	Url url(arg);
	if (url.get_port() == 0)
	{
		COND_PRINT(PRINT_STD, "failed with invalid port\n");
		return false;
	}
	if (url.get_scheme().compare("http") != 0)
	{
		COND_PRINT(PRINT_STD, "failed with invalid scheme\n");
		return false;
	}
	if (url.get_domain().length() > MAX_HOST_LEN)
	{
		COND_PRINT(PRINT_STD, "failed with long host name\n");
		return false;
	}
	if (url.get_request().length() > MAX_REQUEST_LEN)
	{
		COND_PRINT(PRINT_STD, "failed with long request\n");
		return false;
	}
	COND2_PRINT(PRINT_STD, "host %s, port %d", url.get_domain().c_str(), url.get_port());
	COND1_PRINT(PRINT_REQ, ", request %s", url.get_request().c_str());
	COND_PRINT(PRINT_STD, "\n");
	
	out = url;

	return true;
}

bool checkHostUniqueness(const Url& url, int printMask, shared_data* data)
{
	COND_PRINT(PRINT_UNIQUE, "        Checking host uniqueness... ");

	WaitForSingleObject(data->url_semaphore, INFINITE);
	int prev = data->hosts.size();
	data->hosts.insert(url.get_domain());
	int newSize = data->hosts.size();
	ReleaseSemaphore(data->url_semaphore, 1, NULL);

	if (newSize == prev)
	{
		COND_PRINT(PRINT_UNIQUE, "failed\n");
		return false;
	}
	COND_PRINT(PRINT_UNIQUE, "passed\n");

	++data->num_host_unique;
	return true;
}

bool dns(const Url& url, int printMask, long& num_dns, in_addr& out)
{
	COND_PRINT(PRINT_STD, "        Doing DNS... ");
	ULONGLONG startTime = GetTickCount64();

	in_addr ip = url.get_ip();

	ULONGLONG endTime = GetTickCount64();
	if (ip.s_addr == INADDR_NONE)
	{
		COND1_PRINT(PRINT_STD, "failed with %d\n", WSAGetLastError());
		return false;
	}
	COND2_PRINT(PRINT_STD, "done in %d ms, found %s\n", endTime - startTime, inet_ntoa(ip));

	++num_dns;
	out = ip;
	return true;
}

bool checkIPUniqueness(const in_addr& ip, int printMask, shared_data* data)
{
	COND_PRINT(PRINT_UNIQUE, "        Checking IP uniqueness... ");

	WaitForSingleObject(data->ip_semaphore, INFINITE);
	int prev = data->ips.size();
	data->ips.insert(inet_ntoa(ip));
	int newSize = data->ips.size();
	ReleaseSemaphore(data->ip_semaphore, 1, NULL);

	if (data->ips.size() == prev)
	{
		COND_PRINT(PRINT_UNIQUE, "failed\n");
		return false;
	}
	COND_PRINT(PRINT_UNIQUE, "passed\n");
	++data->num_ip_unique;
	return true;
}

bool connectRobots(const in_addr& ip, const int port, int printMask, Socket& out)
{
	COND_PRINT(PRINT_UNIQUE, "        Connecting on robots... ");
	ULONGLONG startTime = GetTickCount64();

	int error = out.open(ip, port);

	ULONGLONG endTime = GetTickCount64();
	if (error != 0)
	{
		COND1_PRINT(PRINT_UNIQUE, "failed with %d\n", error);
		return false;
	}
	COND1_PRINT(PRINT_UNIQUE, "done in %d ms\n", endTime - startTime);
	return true;
}

bool loadRobots(Socket& sock, const Url& url, int printMask, string& robotHead)
{
	COND_PRINT(PRINT_UNIQUE, "        Loading... ");
	ULONGLONG startTime = GetTickCount64();

	string req = "HEAD /robots.txt HTTP/1.0\r\nUser-agent: "
		+ agent_name + "\r\nHost: "
		+ url.get_domain() + "\r\nConnection: close\r\n\r\n";

	COND_PRINT(PRINT_REQUEST, req.c_str());

	sock.cwrite(req);
	robotHead = sock.cread(10, 16 * 1024);
	CHECK_RECV(robotHead);

	if (robotHead.substr(0, 5).compare("HTTP/") != 0)
	{
		COND_PRINT(PRINT_STD, "failed with non-http header\n");
		return false;
	}

	ULONGLONG endTime = GetTickCount64();
	COND2_PRINT(PRINT_UNIQUE, "done in %d ms with %d bytes\n", endTime - startTime, robotHead.length());
	return true;
}

bool verifyRobot(string robotHead, int printMask, long& num_robot)
{
	COND_PRINT(PRINT_UNIQUE, "        Verifying header... ");

	int statusCode = stoi(robotHead.substr(9, 3));

	COND1_PRINT(PRINT_UNIQUE, "status code %d\n", statusCode);

	if (statusCode / 100 != 4)
	{
		++num_robot;
		return false;
	}

	return true;
}

bool connectPage(const in_addr& ip, const Url& url, int printMask, Socket& out)
{
	COND_PRINT(PRINT_STD, "      * Connecting on page... ");
	ULONGLONG startTime = GetTickCount64();

	int error = out.open(ip, url.get_port());
	if (error != 0)
	{
		COND1_PRINT(PRINT_STD, "failed with %d\n", error);
		return false;
	}

	ULONGLONG endTime = GetTickCount64();
	COND1_PRINT(PRINT_STD, "done in %d ms\n", endTime - startTime);
	return true;
}

bool loadPage(Socket& sock, const Url& url, int printMask, string& out)
{
	COND_PRINT(PRINT_STD, "        Loading... ");
	ULONGLONG startTime = GetTickCount64();

	string req = "GET " + url.get_request() + " HTTP/1.0\r\n"
		+ "User-agent: " + agent_name + "\r\n"
		+ "Host: " + url.get_domain() + "\r\n"
		+ "Connection: close\r\n\r\n";
	
	COND_PRINT(PRINT_REQUEST, req.c_str());

	sock.cwrite(req);
	out = sock.cread(10, 2 * 1024 * 1024);
	CHECK_RECV(out);

	ULONGLONG endTime = GetTickCount64();
	COND2_PRINT(PRINT_STD, "done in %d ms with %d bytes\n", endTime - startTime, out.length());
	return true;
}

bool verifyPage(string& html, int printMask, shared_data* data, string& outstr, int& outi)
{
	COND_PRINT(PRINT_STD, "        Verifying header... ");

	outi = html.find("\r\n\r\n");
	if (outi < 0 || html.substr(0, 5).compare("HTTP/") != 0)
	{
		COND_PRINT(PRINT_STD, "failed with invalid header.\n");
		return false;
	}

	outstr = html.substr(0, outi);
	int statusCode = stoi(outstr.substr(9, 3));

	COND1_PRINT(PRINT_STD, "status code %d\n", statusCode);

	int bodyEnd = html.find("\r\n\r\n", outi + 1);
	if (bodyEnd < 0)
	{
		bodyEnd = html.length();
	}

	switch (statusCode / 100)
	{
	case 2:
		++data->num_2xx;
		return true;
	case 3:
		++data->num_3xx;
		return false;
	case 4:
		++data->num_4xx;
		return false;
	case 5:
		++data->num_5xx;
		return false;
	default:
		++data->num_other;
		return false;
	}
}

bool parsePage(Url& url, string& html, int headEnd, int printMask, shared_data* data)
{
	int bodyEnd = html.find("\r\n\r\n", headEnd + 1);
	if (bodyEnd < 0)
	{
		bodyEnd = html.length() - 1;
	}
	string body = html.substr(headEnd + 4, bodyEnd - headEnd - 4) + '\n';

	COND_PRINT(PRINT_BODY, body.c_str());

	COND_PRINT(PRINT_STD, "      + Parsing page... ");

	ULONGLONG startTime = GetTickCount64();

	HTMLParserBase parser;
	string base_url = url.get_base_url();
	char* tempHtml = new char[html.length() + 1];
	char* tempUrl = new char[base_url.length() + 1];
	data->total_size += strlen(tempHtml) * 8;
	memcpy(tempHtml, html.c_str(), html.length() + 1);
	memcpy(tempUrl, base_url.c_str(), base_url.length() + 1);
	int links = 0;
	parser.Parse(tempHtml, strlen(tempHtml), tempUrl, strlen(tempUrl), &links);
	delete[] tempHtml;
	delete[] tempUrl;
	++data->num_crawled;

	ULONGLONG endTime = GetTickCount64();
	
	if (links < 0)
	{
		COND_PRINT(PRINT_STD, "failed\n");
		return false;
	}
	else
	{
		data->num_links += links;
		COND2_PRINT(PRINT_STD, "done in %d ms with %d links\n", endTime - startTime, links);
		return true;
	}

}

void crawlURL(char* arg, int printMask, shared_data* data)
{
	Url url;
	bool cont = parseUrl(arg, printMask, url, data);
	if (!cont) return;

	cont = checkHostUniqueness(url, printMask, data);
	if (!cont) return;

	in_addr ip;
	cont = dns(url, printMask, data->num_dns, ip);
	if (!cont) return;
	
	cont = checkIPUniqueness(ip, printMask, data);
	if (!cont) return;

	Socket sock;
	cont = connectRobots(ip, url.get_port(), printMask, sock);
	if (!cont) return;

	string robotHead;
	cont = loadRobots(sock, url, printMask, robotHead);
	if (!cont) return;

	cont = verifyRobot(robotHead, printMask, data->num_robot);
	if (!cont) return;

	Socket sock2;
	cont = connectPage(ip, url, printMask, sock2);
	if (!cont) return;

	string html;
	cont = loadPage(sock2, url, printMask, html);
	if (!cont) return;
	
	string head;
	int headEnd;
	cont = verifyPage(html, printMask, data, head, headEnd);
	
	if (cont)
	{
		parsePage(url, html, headEnd, printMask, data);
	}

	COND_PRINT(PRINT_HEAD, "----------------------------------------\n");
	COND1_PRINT(PRINT_HEAD, "%s\n", head.c_str());
	return;
}

int active_threads;
int time;

UINT statsThread(LPVOID param)
{
	shared_data* data = (shared_data*)param;
	time = 0;

	while (true)
	{
		Sleep(2000);
		time += 2;

		printf("[%3d] %4d Q %6d E %7d H %6d D %6d I %5d R %5d C %5d L %4dK\n",
			time, active_threads, data->pendingUrls.size(), data->num_urls, data->num_host_unique,
			data->num_dns, data->num_ip_unique, data->num_robot, data->num_crawled,
			data->num_links/1000);
		printf("     *** crawling %.2f pps @ %.2f Mbps\n", (float)data->num_crawled / time,
			(float)data->total_size / time / 1e6);
	}

	if (time == 0)
	{
		cout << "time 0 stats exiting" << endl;
		return 17;
	}

	return 0;
}

UINT crawlerThread(LPVOID param)
{
	shared_data* data = (shared_data*)param;

	while (data->fileRunning || data->pendingUrls.size() > active_threads)
	{
		char* temp = data->pendingUrls.pull();
		if (temp == NULL)
		{
			continue;
		}
		crawlURL(temp, 0, data);
		delete[] temp;
	}
	--active_threads;
	return 0;
}

UINT fileThread(LPVOID param)
{
	shared_data* data = (shared_data*)param;
	data->fileRunning = true;

	string filename = data->filename;
	ifstream in;
	in.open(filename);
	struct stat fileStat;
	int rc = stat(filename.c_str(), &fileStat);
	if (rc != 0)
	{
		cout << "Failed to open file " << filename << endl;
		data->fileRunning = false;
		return 1;
	}
	printf("Opened %s with size %ld\n", filename.c_str(), fileStat.st_size);

	string arg;
	while (in >> arg)
	{
		++data->num_to_crawl;
		char* temp = new char[arg.length() + 1];
		memcpy(temp, arg.c_str(), arg.length() + 1);
		data->pendingUrls.push(temp);
	}
	data->fileRunning = false;
	//cout << "file thread finished" << endl;
	return 0;
}


int main(int argc, char *argv[])
{
	if (argc == 1 || argc > 3)
	{
		printf("Please provide command line arguments:\n"
			"hw1.exe <URL address> OR\n"
			"hw1.exe <number of threads> <name of input file>");
		return 1;
	}
	else if (argc == 2)
	{
		if (winsock_init() != 0)
		{
			return 2;
		}
		shared_data dummy_data;
		crawlURL(argv[1], PRINT_STD | PRINT_REQ | PRINT_HEAD, &dummy_data);
	}
	else if (argc == 3)
	{
		if (winsock_init() != 0)
		{
			return 2;
		}
		int num_threads = atoi(argv[1]);

		shared_data data;

		data.ip_semaphore = CreateSemaphore(NULL, 1, 1, NULL);
		data.url_semaphore = CreateSemaphore(NULL, 1, 1, NULL);

		data.filename = argv[2];

		active_threads = 0;
		
		Sleep(100); // Give just a little time for the file thread to populate the queue

		HANDLE* crawlerHandles = new HANDLE[num_threads];
		
		for (int i = 0; i < num_threads; ++i)
		{
			++active_threads;
			crawlerHandles[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)crawlerThread, &data, 0, NULL);
		}

		HANDLE fileHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fileThread, &data, 0, NULL);

		HANDLE statsHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)statsThread, &data, 0, NULL);

		/*while (true)
		{
			if (!data.fileRunning && data.pendingUrls.size() == 0)
			{
				for (int i = 0; i < num_threads; ++i)
				{
					TerminateThread(crawlerHandles[i], 0);
				}
			}
			else
			{
				Sleep(1000);
			}
		}*/

		for (int i = 0; i < num_threads; ++i)
		{		
			WaitForSingleObject(crawlerHandles[i], INFINITE);
			CloseHandle(crawlerHandles[i]);
		}
		delete[] crawlerHandles;

		CloseHandle(data.url_semaphore);
		CloseHandle(data.ip_semaphore);

		//WaitForSingleObject(statsHandle, INFINITE);
		CloseHandle(statsHandle);

		printf("Extracted %d URLs @ %d/s\n", data.num_urls, data.num_urls / time);
		printf("Looked up %d DNS names @ %d/s\n", data.num_dns, data.num_dns / time);
		printf("Downloaded %d robots @ %d/s\n", data.num_robot, data.num_robot / time);
		printf("Crawled %d pages @ %d/s(%.2f MB)\n", data.num_crawled, data.num_crawled / time, (float)data.total_size / 1e6);
		printf("Parsed %d links @ %d/s\n", data.num_links, data.num_links / time);
		printf("HTTP codes: 2xx = %d, 3xx = %d, 4xx = %d, 5xx = %d, other = %d\n",
			data.num_2xx, data.num_3xx, data.num_4xx, data.num_5xx, data.num_other);

	}
}

