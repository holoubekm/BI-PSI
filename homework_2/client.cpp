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
#include <ctype.h>
#include <string>
#include <fcntl.h>
#include <fstream>
#include <sys/types.h>
#include <cmath>
#include <netdb.h>
#include <queue>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <set>
#include <queue>
#include <arpa/inet.h>

/********************************************************/
/********************************************************/
/************							     ************/
/************   Created by Martin Holoubek   ************/
/************							     ************/
/********************************************************/
/********************************************************/


using namespace std;
typedef unsigned char uchar;
typedef unsigned long ulong;

class Packet;
bool sendRawPacket(Packet*);
bool recvRawPacket(Packet*&);

static const uint16_t g_Port = 4000;
static const uint16_t g_Timeout = 100;
static const uint16_t g_Data_Size = 255;
static const uint16_t g_Frame_Cnt = 8;
static const uint16_t g_Frame_Size = g_Frame_Cnt * g_Data_Size;
static const uint16_t g_Send_Limit = 20;

enum FLAG
{
	NONE,
	RST,
	FIN,
	SYN,
	ERROR
};

static const uchar g_Flags[] = { 0x0, 0x1, 0x2, 0x4 };

class Packet
{
public:
	Packet(const uchar pid_Vec[4], const uchar pseq_Vec[2], const uchar pack_Vec[2], uchar pflag, uchar* pdata, uchar plen) : sent_Cnt(0)
	{
		id_Vec = new uchar[4];
		seq_Vec = new uchar[2];
		ack_Vec = new uchar[2];
		memcpy(id_Vec, pid_Vec, sizeof(uchar) * 4);
		id_Val = id_Vec[0];
		id_Val = (id_Val << 8) + id_Vec[1];
		id_Val = (id_Val << 8) + id_Vec[2];
		id_Val = (id_Val << 8) + id_Vec[3];
		memcpy(seq_Vec, pseq_Vec, sizeof(uchar) * 2);
		seq_Val = seq_Vec[0];
		seq_Val = (seq_Val << 8) + seq_Vec[1];
		memcpy(ack_Vec, pack_Vec, sizeof(uchar) * 2);
		ack_Val = ack_Vec[0];
		ack_Val = (ack_Val << 8) + ack_Vec[1];
		len = plen;
		data = new uchar[len];
		memcpy(data, pdata, len * sizeof(uchar));
		flag_Val = pflag;
		flag = ERROR;
		for(int x = 0; x < 4; x++)
			if(g_Flags[x] == pflag)
			flag = (FLAG)x;
	}

	Packet(uint32_t pid_Val, uint16_t pseq_N_Val, uint16_t pack_N_Val, FLAG pflag, uchar* pdata, uchar plen) : sent_Cnt(0)
	{
		id_Vec = new uchar[4];
		seq_Vec = new uchar[2];
		ack_Vec = new uchar[2];
		id_Val = pid_Val;
		id_Vec[0] = (id_Val >> 24) & 0xFF;
		id_Vec[1] = (id_Val >> 16) & 0xFF;
		id_Vec[2] = (id_Val >> 8) & 0xFF;
		id_Vec[3] = id_Val & 0xFF;
		seq_Val = pseq_N_Val;
		seq_Vec[0] = (seq_Val >> 8) & 0xFF;
		seq_Vec[1] = seq_Val & 0xFF;
		ack_Val = pack_N_Val;
		ack_Vec[0] = (ack_Val >> 8) & 0xFF;
		ack_Vec[1] = ack_Val & 0xFF;
		flag_Val = g_Flags[(int)pflag];
		flag = pflag;
		len = plen;
		data = new uchar[len];
		memcpy(data, pdata, len * sizeof(uchar));
	}

	~Packet()
	{
		delete[] id_Vec;
		delete[] seq_Vec;
		delete[] ack_Vec;
		if(data && len)
			delete[] data;
	}

	bool limitReached()
	{
		return sent_Cnt >= g_Send_Limit;
	}

	friend ostream& operator<<(ostream& os, const Packet& cur)
	{
		os << "id_Vec: " << cur.id_Val << endl;
		os << "seq_Vec: " << cur.seq_Val << endl;
		os << "ack_Vec: " << cur.ack_Val << endl;
		os << "flag: " << (ulong)cur.flag_Val << endl;
		os << "len: " << (ulong)cur.len << endl << "data: " << hex;
		if(cur.len > 30)
		{
			for(int x = 0; x < 15; x++)	
				os << (int)cur.data[x];
			os << "...";
			for(int x = 0; x < 15; x++)	
				os << (int)cur.data[x];
		}
		else
		{
			for(int x = 0; x < min((int)cur.len, 30); x++)	
				os << (int)cur.data[x];
		}
		os << dec << endl;
		return os;
	}

	uchar* id_Vec;
	uchar* seq_Vec;
	uchar* ack_Vec;
	uint32_t id_Val;
	uint16_t seq_Val;
	uint16_t ack_Val;
	uchar sent_Cnt;
	uchar flag_Val;
	FLAG flag;
	uchar* data;
	uchar len;
};

class PacketCmp
{
public:
	bool operator()(const Packet* p1, const Packet* p2)
	{
		return p1->seq_Val > p2->seq_Val;
	}
};

int g_SockInput;
uchar g_Task;

sockaddr_in g_AddrOut;
sockaddr_in g_LocalAddr;

hostent* g_HostEnt;
servent* g_ServEnt;

vector<Packet*> g_Data;
vector<Packet*> g_OutData;

bool g_Connected;
bool g_Finished;
bool g_RST;

uint32_t g_ID;
uint16_t g_Mod_Hash;
uint16_t g_Last_Ack;

void die(const string& msg)
{
	cerr << msg << endl;
	exit(1);
}

void sendRST()
{
	cout << "Sending: RST" << endl;
	g_RST = true;
	Packet cur(g_ID, 0, 0, RST, NULL, 0);
	sendRawPacket(&cur);
}

void sendFIN_CLI(ulong ack)
{
	cout << "Sending: FIN" << endl;
	Packet cur(g_ID, 0, ack, FIN, NULL, 0);
	sendRawPacket(&cur);
}

void sendFIN_SRV(ulong ack)
{
	cout << "Sending: FIN" << endl;
	Packet cur(g_ID, ack, 0, FIN, NULL, 0);
	sendRawPacket(&cur);
}

void sendACK(ulong ack)
{
	Packet cur(g_ID, 0, ack, NONE, NULL, 0);
	sendRawPacket(&cur);
}

bool sendRawPacket(Packet* cur)
{
	cur->sent_Cnt++;
	unsigned int cnt = 9 + cur->len;
	uchar* buf = new uchar[cnt];
	
	memcpy(buf, cur->id_Vec, 4 * sizeof(uchar));
	memcpy(buf + 4, cur->seq_Vec, 2 * sizeof(uchar));
	memcpy(buf + 6, cur->ack_Vec, 2 * sizeof(uchar));
	buf[8] = cur->flag_Val;
	if(cur->len > 0 && cur->data)
		memcpy(buf + 9, cur->data, cur->len * sizeof(uchar));

	int ret = sendto(g_SockInput, buf, cnt, 0, (sockaddr*)&g_AddrOut, sizeof(g_AddrOut));
	delete[] buf;
	if(ret < 0 || ret != (int)cnt)
		return false;
	return true;
}

bool recvRawPacket(Packet*& cur)
{
	const int buf_size = 264;
	uchar buf[buf_size];

	fd_set socketSet;
	FD_ZERO(&socketSet);
	FD_SET(g_SockInput, &socketSet);

	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;

	if (select(g_SockInput + 1, &socketSet, NULL, NULL, &timeout)) 
	{
		int ret;
		socklen_t addLen = sizeof ((sockaddr*) & g_AddrOut);
		if((ret = recvfrom(g_SockInput, buf, buf_size, 0, (sockaddr*)& g_AddrOut, &addLen)) < 0)
			return false;
		cur = new Packet(buf, buf + 4, buf + 6, buf[8], buf + 9, ret - 9);
		return true;
	}
	return false;
}

bool recvDataPacket(Packet*& cur)
{
	bool ret = false;
	if(!recvRawPacket(cur))
		return false;

	if(cur->flag == FIN)
	{
		if(cur->len == 0)
		{
			g_Last_Ack = cur->seq_Val;
			g_Finished = true;
		}
		else
			sendRST();
	}		
	else if(cur->id_Val != g_ID)
		g_RST = true;
	else if(cur->flag == RST && cur->id_Val == g_ID)
		g_RST = true;
	else if(cur->flag == NONE)
		ret = true;
	return ret;
}

bool recvACK(Packet*& cur)
{
	if(recvDataPacket(cur))
	{
		if(cur->len == 0)
			return true;
		else
			delete cur;
	}
	return false;
}

bool recvSYN()
{
	bool ret = false;
	Packet* cur = NULL;
	if(recvRawPacket(cur))
	{
		// cout << "Received packet" << endl;
		if(cur->flag == SYN && cur->id_Val != 0 && cur->seq_Val == 0 && cur->ack_Val == 0 && cur->len == 1 && cur->data[0] == g_Task)
		{
			cout << "Received SYN" << endl;
			g_ID = cur->id_Val;
			ret = true;
		}	
		else if(cur->flag == SYN && cur->len == 0)
		{
			cout << "Received malformed SYN" << endl;
			g_RST = true;
			ret = false;
		}
		else if(cur->flag == RST)
		{
			cout << "Received RST" << endl;
			cout << cur->flag << endl;
			g_RST = true;
			ret = false;
		}
		else
		cout << "Received packet, but not SYN" << endl;
	}
	delete cur;
	return ret;
}

bool inQueue(uint16_t ack)
{
	vector<Packet*>::iterator it;
	for(it = g_Data.begin(); it != g_Data.end(); ++it)
		if((*it)->seq_Val == ack)
		{
			return true;
		}
	return false;
}

Packet* extractQueue(uint16_t ack)
{
	vector<Packet*>::iterator it;
	for(it = g_Data.begin(); it != g_Data.end(); ++it)
		if((*it)->seq_Val == ack)
			break;
	Packet* cur = *it;
	g_Data.erase(it);
	return cur;
}

bool connect()
{
	g_Connected = false;
	Packet packet_SYN(0, 0, 0, SYN, &g_Task, 1);
	while(!g_Connected)
	{
		cout << "Sending SYN" << endl;
		sendRawPacket(&packet_SYN);
		if(recvSYN())
			g_Connected = true;

		if(g_RST)
		{
			cout << "Received: RST" << endl;
			sendRST();
			return false;
		}

		if(packet_SYN.limitReached())
		{
			cout << "Limit reached while sending SYN packet" << endl;
			g_RST = true;
			sendRST();
		}
	}
	return g_Connected;
}

bool disconnect(uint16_t ack, bool cli)
{
	if(cli)
	{
		Packet cur = Packet(g_ID, 0, ack, FIN, NULL, 0);
		sendRawPacket(&cur);
		return true;
	}
		
	
	g_Finished = false;
	Packet packet_FIN = Packet(g_ID, ack, 0, FIN, NULL, 0);

	sendRawPacket(&packet_FIN);
	while(!g_Finished)
	{
		cout << "Sending FIN" << endl;
		Packet* cur;
		if(recvDataPacket(cur))
		{
			delete cur;
		}

		if(g_RST)
		{
			cout << "Received: RST" << endl;
			sendRST();
			return false;
		}

		if(packet_FIN.limitReached())
		{
			cout << "Limit reached while sending FIN packet" << endl;
			g_RST = true;
			sendRST();
		}
		sendRawPacket(&packet_FIN);
	}
	return g_Finished;
}

bool downloadImage()
{
	
	g_RST = false;
	g_ID = 0;
	g_Last_Ack = 0;

	
	if(!connect())
	{
		cout << "Connection failed" << endl;
		return false;
	}

	cout << "Connected succesfully" << endl;
	cout << "Receiving data" << endl << endl;

	uint16_t cur_Ack = 0;
	timeval timeStart;
	gettimeofday(&timeStart, NULL);

	while(true)
	{
		Packet* cur;
		if(recvDataPacket(cur))
		{
			if(inQueue(cur->seq_Val))
			{
				cout << "Packet already downloaded" << endl;
				delete cur;
			}
			else
			{
				g_Data.push_back(cur);
				cout << endl;
				cout << "***************" << endl;
				cout << *cur;
				cout << "***************" << endl << endl;
			}

			while(inQueue(cur_Ack))
			{
				Packet* cur = extractQueue(cur_Ack);
				g_OutData.push_back(cur);
				cur_Ack += cur->len;
			}
		}

		timeval timeEnd;
		gettimeofday(&timeEnd, NULL);
		long timeDifference = ((timeEnd.tv_sec - timeStart.tv_sec) * 1000) + ((timeEnd.tv_usec - timeStart.tv_usec) / 1000);
		if (timeDifference > g_Timeout) 
		{
			cout << "Frame timeout reached: " << g_Timeout << "ms" << endl;
			sendACK(cur_Ack);
			gettimeofday(&timeStart, NULL);
		}

		if(g_RST)
		{
			cout << "Received: RST" << endl << endl;
			cout << "Current queue state: " << endl;
			vector<Packet*>::iterator it;
			for(it = g_Data.begin(); it != g_Data.end(); ++it)
				cout << **it << endl;
			cout << endl;
			return false;
		}

		if(g_Finished && g_Last_Ack == cur_Ack)
		{
			cout << "Download completed" << endl;
			break;
		}
	}

	if(!disconnect(cur_Ack, true))
		die("Can't disconnect from server");
	
	cout << "Succesfully disconnected" << endl;

	ofstream out("downloaded.png");
	vector<Packet*>::iterator it;
	for(it = g_OutData.begin(); it != g_OutData.end(); ++it)
	{
		out.write((const char*)(*it)->data, (*it)->len);
		delete *it;
	}
	out.close();
	cout << "File \"downloaded.png\" succesfully written" << endl;
	return true;
}


bool loadData(const string& in)
{
	ifstream fw(in.c_str(), ios::in | ios::binary);
	if(!fw.is_open())
		die("Can't open input file");

	fw.seekg(0, ios::end); 
	long size = fw.tellg();
	fw.seekg(0, ios::beg);
	g_Mod_Hash = (uint16_t)size;
    uchar buf[255];
    uint16_t seq_Vec = 0;
	while(size > 0)
	{
		fw.read((char*)buf, 255); size -= 255;
		g_Data.push_back(new Packet(g_ID, seq_Vec, 0, NONE, buf, 255 + (size < 0 ? size : 0)));
		seq_Vec += 255 + (size < 0 ? size : 0);
	}
	return true;
}


bool uploadFirmware(const string& fw)
{
	g_RST = false;
	g_ID = 0;
	g_Last_Ack = 0;

	
	if(!connect())
		die("Connection failed");

	if(!loadData(fw))
		die("Can't load firmware");

	cout << "Uploading firmware" << endl;
	
	int tries = 0;
	uint64_t total_Sent = 0;
	uint16_t last_Ack = 0;
	uint16_t cur_Ack = 0;
	bool ack_Inc = false;
	timeval timeStart; gettimeofday(&timeStart, NULL);

	while(true)
	{
		Packet* cur;
		ack_Inc = false;

		if(g_RST)
		{
			cout << "Set RST flag, finishing" << endl;
			sendRST();
			break;
		}

		if(recvACK(cur))
		{
			if(cur->ack_Val == last_Ack)
			{
				tries++;
			}
			else
				tries = 0;

			cout << endl;
			cout << "***************" << endl;
			cout << *cur;
			cout << "***************" << endl << endl;
			

			long val = cur_Ack;
			val = val - cur->ack_Val;
			if(cur->ack_Val > cur_Ack && (cur->ack_Val - cur_Ack) <= g_Frame_Size)
			{
				total_Sent += ((cur->ack_Val - cur_Ack + 254) / 255);
				cout << endl;
				cur_Ack = cur->ack_Val;
				ack_Inc = true;
			}
			else if(cur_Ack >= g_Frame_Size && cur->ack_Val <= g_Frame_Size && (UINT16_MAX - cur_Ack + cur->ack_Val) <= g_Frame_Size)
			{
				total_Sent += ((UINT16_MAX - cur_Ack + cur->ack_Val + 254) / 255);
				cout << endl;
				cur_Ack = cur->ack_Val;
				ack_Inc = true;
			}

			cout << "Sent packets: " << total_Sent << endl;
			if(total_Sent >= g_Data.size())
			{
				cout << "Everything uploaded" << endl;
				cur_Ack += g_Data.back()->ack_Val;
				
					if(!disconnect(cur_Ack, false))
						die("Can't disconnect from client");
				break;
			}

			last_Ack = cur->ack_Val;
			delete cur;
		}

		timeval timeEnd; gettimeofday(&timeEnd, NULL);
		long timeDiff = ((timeEnd.tv_sec - timeStart.tv_sec) * 1000) + ((timeEnd.tv_usec - timeStart.tv_usec) / 1000);
		bool timeout = timeDiff > g_Timeout;
		if(ack_Inc || timeout)
		{
			cout << "Sending a whole frame" << endl;
			int x; vector<Packet*>::iterator it;
			vector<Packet*>::iterator end = g_Data.end();
			for(x = 0, it = (g_Data.begin() + total_Sent); it != end && x < g_Frame_Cnt; ++it, ++x)
			{
				sendRawPacket(*it);
				if((*it)->limitReached())
				{
					cout << "Packet sent 20 times." << endl;
					g_RST = true;
					break;
				}
			}
			timeStart = timeEnd;
		}
		else if(tries >= 3 && total_Sent < g_Data.size())
		{
			cout << "Received three packets with the same ACK" << endl;
			Packet* cur = *(g_Data.begin() + total_Sent);
			sendRawPacket(cur);
			timeStart = timeEnd;
			tries = 0;
		}
	}

	cout << "Succesfully disconnected" << endl;
	vector<Packet*>::iterator it;
	for(it = g_Data.begin(); it != g_Data.end(); ++it)
	{
		delete *it;
	}
	return true;
}

int main(int argc, char* argv[])
{

	if(argc < 2)
		die("Usage: ./server ip [firmware]");

	unsigned int in_Addr;
	if ((in_Addr = inet_addr(argv[1])) != INADDR_NONE)
		g_HostEnt = gethostbyaddr((char *)&in_Addr, sizeof(unsigned int), AF_INET);
	else
		g_HostEnt = gethostbyname(argv[1]);

	if(!g_HostEnt)
		die("Can't find host");

	if((g_SockInput = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		die("Socket returned negative value");

	bzero ((char *) &g_AddrOut, sizeof(g_AddrOut));
	g_AddrOut.sin_family = AF_INET;
	g_AddrOut.sin_port = htons((u_short)g_Port);
	memcpy(&(g_AddrOut.sin_addr), g_HostEnt->h_addr, g_HostEnt->h_length);

	bool ret;
	if(argc == 2)
	{
		g_Task = 0x01;
		ret = downloadImage();
	}
	else if(argc == 3)
	{
		g_Task = 0x02;
		ret = uploadFirmware(argv[2]);
	}
	else
		die("Wrong params count");

	close(g_SockInput);
	return ret ? 0 : 1;
}