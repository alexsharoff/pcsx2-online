#include "stdafx.h"
#include "SessionImpl.h"
#include <boost/thread.hpp>
#include <unordered_map>

using boost::asio::ip::udp;

namespace endpoint_hash {
	using namespace boost::asio::ip;
	bool ep_eq::operator()(const udp::endpoint& s1, const udp::endpoint& s2) const
	{
		return s1==s2;
	}

	size_t ep_hash::operator()(const udp::endpoint& s1) const
	{
		unsigned long long val = s1.address().to_v4().to_ulong()+( 
			((unsigned long long)s1.port()) << 32);
		return ull_hash(val);
	}
}


enum ThreadPriority
{
	REALTIME,
	HIGH,
    ABOVE_NORMAL,
    NORMAL,
    BELOW_NORMAL,
    IDLE,
};

inline void applyPriority(boost::thread& thread, ThreadPriority priority)
{
#ifdef WIN32
    BOOL res;
    HANDLE th = thread.native_handle();

    switch (priority)
    {
    case REALTIME               : res = SetThreadPriority(th, THREAD_PRIORITY_TIME_CRITICAL);   break;
    case HIGH                   : res = SetThreadPriority(th, THREAD_PRIORITY_HIGHEST);                 break;
    case ABOVE_NORMAL   : res = SetThreadPriority(th, THREAD_PRIORITY_ABOVE_NORMAL);    break;
    case NORMAL                 : res = SetThreadPriority(th, THREAD_PRIORITY_NORMAL);                  break;
    case BELOW_NORMAL   : res = SetThreadPriority(th, THREAD_PRIORITY_BELOW_NORMAL);    break;
    case IDLE                   : res = SetThreadPriority(th, THREAD_PRIORITY_LOWEST);                  break;
    }
    if (res == FALSE)
    {
        int err = GetLastError();
    }
#endif
}



SessionImpl::SessionImpl() :
_io_service(), _socket(0),
_threadRecv(0), _threadSend(0) {
	clean();
	_hdl = 0;
	_stop = true;
}
SessionImpl::~SessionImpl() {
	stop();
	join();
	clean();
}

void SessionImpl::start(unsigned short port) {
	if(!_stop) {
		stop();
		join();
	}
	clean();
	peerHash.clear();

	_stop = false;
	_socket = new udp::socket(_io_service, udp::endpoint(udp::v4(), port));
	_threadRecv = new boost::thread(boost::bind(&SessionImpl::recvLoop, this));
	_threadSend = new boost::thread(boost::bind(&SessionImpl::sendLoop, this));
	applyPriority(*_threadRecv, HIGH);
	applyPriority(*_threadSend, HIGH);
}

void SessionImpl::stop() {
	_stop = true;
	if(_socket != 0) {
		_socket->close();
	}
	_cond.notify_one();
}
void SessionImpl::join() {
	if(_threadRecv != 0)
		_threadRecv->join();
	if(_threadSend != 0)
		_threadSend->join();
}

void SessionImpl::setHandler(Handler* hdl)
{
	_hdl = hdl;
}

void SessionImpl::send(const Endpoint& endPoint, const char* data, int dataLength) {
	using namespace std;

	if(_stop) {
		if(_hdl)
		_hdl->onError(std::runtime_error(
			"cannot perform send operations if session is not opened"),  0);
	}
	else {
		_peerMutex.lock();
		
		udp::endpoint ep = udp::endpoint(boost::asio::ip::address_v4::from_string(endPoint.ip), 
			endPoint.port);
		Peer* peer = getNodeForEp(ep);
		Header header = peer->stats.newHeader(dataLength);
		_peerMutex.unlock();

		_dataMutex.lock();
		memcpy(_tempBuffer, &header, sizeof(Header));
		if(dataLength != 0)
			memcpy(_tempBuffer+sizeof(header), data, dataLength);
		_sendQueue.set(_sendQueue.lastIndex() +1,ep);
		_sendStream.writeBlock(_tempBuffer, sizeof(Header) + dataLength);

		_cond.notify_one();
		_dataMutex.unlock();
	}
}

void SessionImpl::clean() {
	if(_socket != 0)
		delete _socket;
	if(_threadRecv != 0)
		delete _threadRecv;
	if(_threadSend != 0)
		delete _threadSend;
	_socket = 0;
	_threadRecv = 0;
	_threadSend = 0;
	_sendStream.clear();
	_sendQueue.clear();
}
void SessionImpl::sendLoop() {
	char buffer[BufferSize];
	int epIndex = 0;

	while(!_stop){
		boost::unique_lock<boost::mutex> lock(_dataMutex);
		int size =  _sendStream.readNextBlock(buffer, BufferSize);
		if(size == -1) {
			_cond.wait(lock);
			size =  _sendStream.readNextBlock(buffer, BufferSize);
		}
		if(size == -1) {
			lock.unlock();
			if(_hdl && !_stop)
			_hdl->onError(std::runtime_error("unable to read next block"), 0);
		}
		else {
			udp::endpoint ep = _sendQueue.get(epIndex);
			lock.unlock();
			epIndex++;
			boost::system::error_code ec;
			_socket->send_to(boost::asio::buffer(buffer,size ),ep, 0, ec);
			Peer* peer = getNodeForEp(ep);
			Header* header = (Header*)buffer;
			if(_hdl)
			_hdl->onSent(peer, header->id, buffer+sizeof(Header),
				header->bodyLength);
			if (ec) {
				if(_hdl && !_stop) {
					_hdl->onError(boost::system::system_error( ec ), &(getNodeForEp(ep)->remote_ep));
				}
			}
		}
	}
}

void SessionImpl::recvLoop() {
	
	using namespace std;
	char buffer[BufferSize];
	while(!_stop) {
		/*
		boost::asio::socket_base::bytes_readable command(true); 
		socket.io_control(command); 
		std::size_t bytes_readable = command.get(); 
		*/
		udp::endpoint remote_ep;
		boost::system::error_code ec;
		size_t recvdLength = _socket->receive_from(
			boost::asio::buffer(buffer, BufferSize),
			remote_ep,0,ec);
		if (ec)
		{
			if(_hdl && !_stop)
			_hdl->onError(boost::system::system_error( ec ), &(getNodeForEp(remote_ep)->remote_ep));
		}
		else
		{
			if(recvdLength >= sizeof(Header)) {
				Header* header = (Header*)buffer;
				_peerMutex.lock();
				Peer* peer = getNodeForEp(remote_ep);
				bool success = peer->stats.parseHeader(*header);
				_peerMutex.unlock();
				if(success) {
					if(recvdLength < (sizeof(Header) + header->bodyLength)) {
						if(_hdl)
						_hdl->onError(std::runtime_error(
							"body length declared in the header doesn't match the actual one"),
							&(getNodeForEp(remote_ep)->remote_ep));
					}
					else {
						if(_hdl)
						_hdl->onReceived(peer, header->id, buffer+sizeof(Header),
							header->bodyLength);
					}
				}
				else {
					if(_hdl)
					_hdl->onError(std::runtime_error("invalid header"),
						&(getNodeForEp(remote_ep)->remote_ep));
				}
			}
			else {
				if(_hdl)
				_hdl->onError(std::runtime_error("udp packet is too short to be valid"),
					&(getNodeForEp(remote_ep)->remote_ep));
			}
		}
	}
}

Peer* SessionImpl::getNodeForEp(const udp::endpoint& ep) {
	Peer* peer;
	if(peerHash.find(ep) != peerHash.end()) {
		peer = peerHash[ep];
	}
	else {
		peer = new Peer();
		peer->remote_ep.ip = ep.address().to_string();
		peer->remote_ep.port = ep.port();
		peerHash[ep] = peer;
	}
	return peer;
}