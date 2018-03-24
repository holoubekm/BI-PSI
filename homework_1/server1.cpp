#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <strings.h>
#include <ctime>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;

static int sock_srv = -1;
static int num_clients = 0;

enum Resp
{
	LOGIN = 0,
	PASSWORD,
	OK,
	BAD_CHECKSUM,
	LOGIN_FAILED,
	SYNTAX_ERROR,
	TIMEOUT
};

static const char* resps[] = 
{
	"200 LOGIN \r\n",
	"201 PASSWORD \r\n",
	"202 OK \r\n",
	"300 BAD CHECKSUM \r\n",
	"500 LOGIN FAILED \r\n",
	"501 SYNTAX ERROR \r\n",
	"502 TIMEOUT \r\n"
};

void die(const string& msg)
{
	cerr << msg << endl;
	exit(1);
}

void dieAndClose(const string& msg)
{
	close(sock_srv);
	die(msg);
}

void sendResponse(int sock_cli, Resp resp)
{
	int index = (int)resp;
	if (send(sock_cli, resps[index], strlen(resps[index]), 0) < 0)
	{
		dieAndClose("Nelze odeslat odpoved");
	}
}

void sendAndCloseClient(int sock_cli, Resp resp, const string& msg)
{
	cout << msg << endl;
	sendResponse(sock_cli, resp);
	close(sock_cli);
	cout << "Klient byl odpojen" << endl;
}

void closeClient(int sock_cli, const string& msg)
{
	close(sock_cli);
	cout << msg << endl;
}

int getHash(const string& msg)
{
	int len = msg.length() - 2;
	int hash = 0;
	const char* data = msg.c_str();
	for(int x = 0; x < len; x++)
		hash += data[x];
	return hash;
}


string readLine(int sock_cli, bool& closed, time_t begin_time)
{
	stringstream msg;
	const int bufsize = 1;
	char buf[bufsize];
	int len = 0;
	

	bool finished = false;
	char cur = ' ', last = ' ';
	while(true)
	{
		len = recv(sock_cli, buf, bufsize, 0);

		if(len > 0)
		{
			
			for(int x = 0; x < len; x++)
			{
				cur = buf[x];
				msg.write(buf + x, 1);
				if(last == '\r' && cur == '\n')
				{
					finished = true;
				}

				last = cur;
			}
		}
		else if(len == 0)
		{
			closed = true;
			break;
		}

		if(finished)
			break;
		usleep(1000);

		if(float(time(NULL) - begin_time) > 45)
		{
			sendResponse(sock_cli, TIMEOUT);
			closed = true;
			cout << "Timeout" << endl;
			break;

		}
	}
	return msg.str();
}

char readByte(int sock_cli, bool& closed, time_t begin_time)
{
	int len;
	char buf[1];
	bool finished = false;
	while(true)
	{
		len = recv(sock_cli, buf, 1, 0);
		if(len > 0)
		{
			return buf[0];
		}
		else if(len == 0)
		{
			closed = true;
			break;
		}

		if(finished)
			break;
		usleep(1000);

		if(float(time(NULL) - begin_time) > 45)
		{
			sendResponse(sock_cli, TIMEOUT);
			closed = true;
			cout << "Timeout" << endl;
			break;

		}
	}
	return ' ';
}

char* readBytes(int sock_cli, int cnt, bool& closed, time_t begin_time)
{
	stringstream msg;
	const int bufsize = 1;
	char buf[bufsize];
	char* data = new char[cnt];
	int len = 0;
	int totalread = 0;
	bool finished = false;

	while(true)
	{
		len = recv(sock_cli, buf, bufsize, 0);

		if(len > 0)
		{
			
			for(int x = 0; x < len; x++)
			{
				data[totalread + x] = buf[x];
				totalread += len;
				if(totalread == cnt)
				{
					finished = true;
				}
			}
		}
		else if(len == 0)
		{
			closed = true;
			break;
		}

		if(finished)
			break;

		if(float(time(NULL) - begin_time) > 45)
		{
			sendResponse(sock_cli, TIMEOUT);
			closed = true;
			cout << "Timeout" << endl;
			break;

		}
	}
	return data;
}

void* clientThread(void *arg)
{
	int client_no = ++num_clients;
	const clock_t begin_time = time(NULL);

	cout << "Pripojil se klient cislo: " << client_no << endl;
	intptr_t socket = (intptr_t)arg;
	int sock_cli = (int) socket;

	string msg;

	bool closed = false;
	sendResponse(sock_cli, LOGIN);
	string login = readLine(sock_cli, closed, begin_time);
	if(closed)
	{
		closeClient(sock_cli, "Klient se odpojil");
		return NULL;
	}

	int hash = getHash(login);
	sendResponse(sock_cli, PASSWORD);
	msg = readLine(sock_cli, closed, begin_time);
	if(closed)
	{
		closeClient(sock_cli, "Klient se odpojil");
		return NULL;
	}

	int pass;
	istringstream is(msg);
	is >> pass;

	if(pass != hash || login.find("Robot") != 0)
	{
		sendAndCloseClient(sock_cli, LOGIN_FAILED, "Zadano spatne heslo");
		return NULL;
	}

	sendResponse(sock_cli, OK);

	// cout << ">" << msg << "<" << endl;
	while(true)
	{
		char byte = readByte(sock_cli, closed, begin_time);
		if(closed)
		{
			closeClient(sock_cli, "Klient se odpojil");
			return NULL;
		}

		if(byte == 'I')
		{
			const char* tmp = "NFO ";
			for(int x = 0; x < 4; x++)
			{
				byte = readByte(sock_cli, closed, begin_time);
				if(closed)
				{
					closeClient(sock_cli, "Klient se odpojil");
					return NULL;
				}
				if(byte != tmp[x])
				{
					sendAndCloseClient(sock_cli, SYNTAX_ERROR, "Syntax error");
					return NULL;
				}
			}

			msg = readLine(sock_cli, closed, begin_time);
			if(closed)
			{
				closeClient(sock_cli, "Klient se odpojil");
				return NULL;
			}
			cout << "msg: " << msg << endl;
			if(closed)
			{
				closeClient(sock_cli, "Klient se odpojil");
				return NULL;
			}
		}
		else if(byte == 'F')
		{
			const char* tmp = "OTO ";
			for(int x = 0; x < 4; x++)
			{
				byte = readByte(sock_cli, closed, begin_time);
				if(closed)
				{
					closeClient(sock_cli, "Klient se odpojil");
					return NULL;
				}
				if(byte != tmp[x])
				{
					sendAndCloseClient(sock_cli, SYNTAX_ERROR, "Syntax error");
					return NULL;
				}
			}

			int cnt = 0;
			while(true)
			{
				byte = readByte(sock_cli, closed, begin_time);
				if(closed)
				{
					closeClient(sock_cli, "Klient se odpojil");
					return NULL;
				}
				if(byte <= '9' && byte >= '0')
				{
					cnt = cnt * 10 + (byte - '0');
				}
				else if(byte = ' ')
				{
					break;
				}
				else
				{
					sendAndCloseClient(sock_cli, SYNTAX_ERROR, "Syntax error");
					return NULL;
				}
			}

			cout << "cnt: " << cnt << endl;
			char* data = readBytes(sock_cli, cnt + 4, closed, begin_time);
			if(closed)
			{
				closeClient(sock_cli, "Klient se odpojil");
				return NULL;
			}
			long long hash = 0;
			for(int x = 0; x < cnt; x++)
				hash += (unsigned char)data[x];

			unsigned long long crc = 0;
			for(int x = cnt; x < cnt + 4; x++)
			{
				cout << (int)(unsigned char)data[x] << endl;
				crc = (crc * 256) + (unsigned char)data[x];
			}

			cout << "crc: " << crc << ", hash: " << hash << endl;
			if(crc == hash)
			{
				os << "foto" << (rand() % 1000) << ".png";
				stringstream os;
				
				ofstream output (os.str().c_str(), ios::out | ios::binary);
    			output.write (data, cnt);
    			output.close();
			}
			else
			{
				sendResponse(sock_cli, BAD_CHECKSUM);
				cout << "Bad photo checksum" << endl;
				
			}
			delete[] data;
		}
		else
		{
			sendAndCloseClient(sock_cli, SYNTAX_ERROR, "Syntax error");
			return NULL;
		}
		sendResponse(sock_cli, OK);
	}

	close(sock_cli);
	cout << "Klient " << client_no << " byl odpojen" << endl;
}

void startClientThread(int sock_cli)
{
	pthread_t thr;
	pthread_attr_t attr;
	int err;
	intptr_t socket = (intptr_t)sock_cli;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&thr, &attr, &clientThread, (void *) socket) != 0)
		dieAndClose("Nelze vytvorit nove vlakno");
	pthread_attr_destroy(&attr);
}

int main(int argc, char* argv[])
{
	if(argc < 2)
		die("Usage: ./server port");

	int port = -1;
	istringstream is(argv[1]);
	is >> port;

	if(port <= 0)
		die("Unknown port number");

	int sock_cli;
	struct sockaddr_in srv_addr, cli_addr;
	
	if ((sock_srv = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		die("Socket nelze otevrit");

	bzero(&srv_addr, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(port);

	if (bind(sock_srv, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
		dieAndClose("Nelze provest binding");

	if (listen(sock_srv, 0) < 0)
		dieAndClose("Nelze provest listen");

	socklen_t rem_addr_length;
	while (true) 
	{
		rem_addr_length = sizeof(cli_addr);
		if ((sock_cli = accept(sock_srv, (struct sockaddr *)&cli_addr, &rem_addr_length)) < 0)
			dieAndClose("Nelze provest accept");


		int flags = fcntl(sock_cli, F_GETFL);
		if(fcntl(sock_cli, F_SETFL, flags | O_NONBLOCK))
		{
			dieAndClose("Nelze nastavit socket na neblokovaci");
		}

		bool dummy;
		// readLine(sock_cli, dummy);
		startClientThread(sock_cli);
	}
	
	close(sock_srv);
	return 0;
}