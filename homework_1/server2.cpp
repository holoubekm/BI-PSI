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
#include <vector>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <fstream>
#include <sys/types.h>
#include <queue>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;

static int sock_srv = -1;
static int num_clients = 0;
const int max_time = 45;

#define checkClosed() if(rcv->closed) {closeClient(sock_cli, "Klient se odpojil"); return NULL;} 

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


class BufReader
{
public:
	BufReader(int psock_cli, time_t pbegin_time) : sock_cli(psock_cli), begin_time(pbegin_time), closed(false)
	{
		buf = queue<char>();
	}

	char* readBytes(int len)
	{
		while(buf.size() < len && !closed)
			fetchData();

		char* data = new char[len];
		for(int x = 0; x < len; x++)
		{
			data[x] = buf.front();
			buf.pop();
		}
		return data;
	}

	char* readLine()
	{
		char cur = ' ';
		char last = ' ';
		vector<char> vec;

		while(!closed)
		{
			cur = readByte();
			vec.push_back(cur);

			if(cur == '\n' && last == '\r')
				break;
			last = cur;
		}

		if(closed)
			return NULL;

		int len = vec.size();
		char* data = new char[len + 1];
		for(int x = 0; x < len; x++)
			data[x] = vec[x];
		data[len] = '\0';
		return data;
	}

	char readByte()
	{
		while(buf.empty() && !closed)
			fetchData();
		char val = buf.front();
		buf.pop();
		return val;
	}

	void sendBytes(Resp resp)
	{
		int index = (int) resp;
		const char* data = resps[index];
		int total = strlen(data);
		int sent = 0;
		int len = 0;

		while(sent < total && !closed)
		{
			len = send(sock_cli, data + sent, total - sent, 0);
			if(len > 0)
			{
				sent += len;
			}
			else if(len == 0)
			{
				closed = true;
				break;
			}
			else
			{
				cout << errno << endl;
			}

			usleep(100);

			if(float(time(NULL) - begin_time) > max_time && resp != TIMEOUT)
			{
				sendBytes(TIMEOUT);
				closed = true;
				cout << "Timeout" << endl;
				break;
			}
		}
	}

	void sendAndClose(Resp resp)
	{
		sendBytes(resp);
		close(sock_cli);
		closed = true;
		cout << "Klient byl odpojen" << endl;
	}

private:
	void fetchData()
	{
		const int bufsize = 1024;
		char buffer[bufsize];

		int read = 0;
		int len = 0;

		// cout << "Fetching.." << endl;
		while(read == 0)
		{
			len = recv(sock_cli, buffer, bufsize, 0);
			if(len > 0)
			{
				read += len;
				for(int x = 0; x < len; x++)
					buf.push(buffer[x]);
			}
			else if(len == 0)
			{
				closed = true;
				break;
			}

			usleep(100);

			if(float(time(NULL) - begin_time) > max_time)
			{
				cout << "Timeout" << endl;
				sendBytes(TIMEOUT);
				closed = true;
				break;
			}
		}
	}
public:
	bool closed;

	queue<char> buf;
	int sock_cli;
	time_t begin_time;
};

void* clientThread(void *arg)
{
	int client_no = ++num_clients;
	const clock_t begin_time = time(NULL);
	int pass_int = -1;

	cout << "Pripojil se klient cislo: " << client_no << endl;
	intptr_t socket = (intptr_t)arg;
	int sock_cli = (int) socket;

	BufReader* rcv = new BufReader(sock_cli, begin_time);


	rcv->sendBytes(LOGIN);
	checkClosed();
	const char* login = rcv->readLine();
	checkClosed();
	string login_str(login);
	rcv->sendBytes(PASSWORD);
	checkClosed();
	string pass = rcv->readLine();

	istringstream is(pass);
	is >> pass_int;

	if(pass_int != getHash(login) || login_str.find("Robot") != 0)
	{
		rcv->sendAndClose(LOGIN_FAILED);
		return NULL;
	}
	delete[] login;

	rcv->sendBytes(OK);
	checkClosed();

	while(true)
	{
		char byte = rcv->readByte();
		checkClosed();

		if(byte == 'I')
		{
			cout << "Prichazi info zprava" << endl;
			const char* dummy = "NFO ";
			for(int x = 0; x < 4; x++)
			{
				byte = rcv->readByte();
				checkClosed();
				if(byte != dummy[x])
				{
					cout << "Klient cislo: " << client_no << " syntax error" << endl;
					rcv->sendAndClose(SYNTAX_ERROR);
					return NULL;
				}
			}

			const char* info = rcv->readLine();
			cout << info << endl;
			delete[] info;
			rcv->sendBytes(OK);
			checkClosed();
		}
		else if(byte == 'F')
		{
			cout << "Prichazi foto" << endl;
			const char* dummy = "OTO ";
			for(int x = 0; x < 4; x++)
			{
				byte = rcv->readByte();
				checkClosed();
				if(byte != dummy[x])
				{
					cout << "Klient cislo: " << client_no << " syntax error" << endl;
					rcv->sendAndClose(SYNTAX_ERROR);
					return NULL;
				}
			}

			int cnt = 0;
			while(true)
			{
				byte = rcv->readByte();
				checkClosed();

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
					cout << "Klient cislo: " << client_no << " syntax error" << endl;
					rcv->sendAndClose(SYNTAX_ERROR);
					return NULL;
				}
			}

			const char* img = rcv->readBytes(cnt + 4);

			long long hash = 0;
			for(int x = 0; x < cnt; x++)
				hash += (int)(unsigned char)img[x];

			long long crc = 0;
			for(int x = cnt; x < cnt + 4; x++)
			{
				cout << (int)(unsigned char)img[x] << endl;
				crc = (crc * 256) + (unsigned char)img[x];
			}

			cout << "crc: " << crc << ", hash: " << hash << endl;
			if(crc == hash)
			{
				stringstream os;
				os << "foto" << (rand() % 1000) << ".png";
				
				ofstream output (os.str().c_str(), ios::out | ios::binary);
    			output.write (img, cnt);
    			output.close();

    			rcv->sendBytes(OK);
				checkClosed();
			}
			else
			{
				cout << "Bad photo checksum" << endl;
				rcv->sendBytes(BAD_CHECKSUM);
				checkClosed();
			}
			delete[] img;
		}
		else
		{
			cout << "prisel znak >" << byte << "<" << endl;
			cout << "Klient cislo: " << client_no << " syntax error" << endl;
			rcv->sendAndClose(SYNTAX_ERROR);
			return NULL;
		}
	}
	cout << "Klient cislo: " << client_no << " se odpojil" << endl;
	return NULL;
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
		startClientThread(sock_cli);
	}
	
	close(sock_srv);
	return 0;
}