#include <vector>
#include "Session.h"
#include "Endpoint.h"
#include "Netplay.h"
#include "NetplayImpl.h"

Netplay::Netplay()
{
	p_impl = new NetplayImpl();
}

Netplay::~Netplay()
{
	delete p_impl;
}

bool Netplay::host( int timeoutMs)
{
	return p_impl->host(timeoutMs);
}

bool Netplay::connect(const char* remoteIp, int remotePort, int timeoutMs)
{
	return p_impl->connect(remoteIp, remotePort, timeoutMs);
}


bool Netplay::stop()
{
	p_impl->endSession();
	return true;
}

const std::vector<Peer> Netplay::peers()
{
	return p_impl->getPeers();
}

bool Netplay::saveReplay (const char* fileName)
{
	return p_impl->saveReplay(fileName);
}
bool Netplay::readReplay (const char* fileName)
{
	return p_impl->playReplay(fileName);
}

int Netplay::getInput(char* data, int frame, int side, int timeout)
{
	return p_impl->getInput(data, frame, side, timeout);
}
void Netplay::setInput(const char* data, int length, int frame, int side)
{
	return p_impl->setInput(data, length, frame, side);
}
bool Netplay::sendInput(const Endpoint& ep, int side)
{
	p_impl->sendInput(ep, side);
	return true;
}
bool Netplay::isStarted()
{
	return !p_impl->_session->isStopped();
}
bool Netplay::start(int myPort)
{
	return p_impl->start(myPort);
}

bool Netplay::send(const Endpoint& ep, char* data, int length)
{
	p_impl->send( ep, data, length );
	return true;
}
int Netplay::recv(const Endpoint& ep, char* data, int length, int timeout)
{
	return p_impl->recv( ep, data, length, timeout );
}

unsigned long long Netplay::lastResponceTime()
{
	return p_impl->lastResponce();
}