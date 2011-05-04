#pragma once
#include <vector>
#include "Session.h"
#include "Endpoint.h"
#include "dllapi.h"
#include "Utilities.h"

class NETPLAY_API Netplay
{
public:
	Netplay();
	~Netplay();

	int protocolId();
	bool setProtocolId(int id);

	const char* playerName();
	bool setPlayerName(const char* name);

	bool start(int myPort);
	bool stop();
	bool isStarted();
	int port();

	bool host(int timeoutMs);
	bool connect(const char* remoteIp, int remotePort, int timeoutMs);
	const std::vector<Peer> peers();

	bool saveReplay (const char* fileName);
	bool readReplay (const char* fileName);

	bool setDefaultInput(const char* data, int length);
	int defaultInput(char* data);
	int inputDelay();
	bool setInputDelay(int delay);
	int getInput(char* data, int frame, int side, int timeout = 0);
	void setInput(const char* data, int length, int frame, int side);
	bool sendInput(const Endpoint& ep, int side);
	
	//guaranteed send
	bool send(const Endpoint& ep, char* data, int length);
	int recv(const Endpoint& ep, char* data, int length, int timeout);

	unsigned long long lastResponceTime();
protected:
	class NetplayImpl* p_impl;
};