#pragma once
#include <string>
#include "Protocol.h"
#include <boost/signals.hpp>
#include "Endpoint.h"

struct Peer {
public:
	Peer();
	PeerStatistics stats;
	Endpoint remote_ep;
	bool valid;
	bool operator==(const Peer& other);
	bool operator!=(const Peer& other);
	//дополнительна€ информаци€ о пире.
	// акие данные он публикует и какие запрашивает

};

class Handler
{
public:
	virtual void onSent(const Peer* peer, int, const char* data, size_t len) = 0;
	virtual void onReceived(const Peer* peer, int, const char* data, size_t len) = 0;
	virtual void onError(const std::exception& exc, const Endpoint* ep) = 0;
};

class Session {
public:
	Session();
	~Session();

	void start(unsigned short port);

	void stop();
	void join();

	void send(const Endpoint& ep, const char* data, int dataLength);
	void setHandler(Handler* hdl);

	bool isStopped();
private:
	class SessionImpl* p_impl;
};
