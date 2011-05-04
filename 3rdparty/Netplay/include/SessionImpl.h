#pragma once
#include <boost/thread.hpp>
#include <unordered_map>
#include "CircularBuffer.h"
#include "CircularStream.h"
#include "Protocol.h"
#include "Utilities.h"
#include "Session.h"
#include "Endpoint.h"
#include <boost/asio.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/functional/hash.hpp>
#include <boost/thread.hpp>

namespace endpoint_hash {
	using namespace boost::asio::ip;
	struct ep_eq
	{
		inline bool operator()(const udp::endpoint& s1,
			const udp::endpoint& s2) const;
	};

	struct ep_hash
	{
		boost::hash<unsigned long long> ull_hash;
		inline size_t operator()(const udp::endpoint& s1) const;
	};
}

#define BufferSize 1024

class SessionImpl {
	friend class Session;
public:
	SessionImpl();
	~SessionImpl();

	void start(unsigned short port);

	void stop();
	void join();

	void send(const Endpoint& ep, const char* data, int dataLength);
	void setHandler(Handler* hdl);
private:
	Handler* _hdl;

	std::tr1::unordered_map<boost::asio::ip::udp::endpoint, Peer*,
		endpoint_hash::ep_hash, endpoint_hash::ep_eq> peerHash;
	void clean();
	void sendLoop();

	void recvLoop();

	Peer* getNodeForEp(const boost::asio::ip::udp::endpoint& ep);
	bool _stop;

	boost::asio::io_service _io_service;
	boost::asio::ip::udp::socket* _socket;

	boost::mutex _dataMutex;
	boost::mutex _peerMutex;

	boost::condition_variable _cond;

	boost::thread* _threadRecv;
	boost::thread* _threadSend;

	CircularStream<BufferSize> _sendStream;
	CircularBuffer<boost::asio::ip::udp::endpoint, BufferSize/sizeof(Header)> _sendQueue;

	char _tempBuffer[BufferSize];
};
