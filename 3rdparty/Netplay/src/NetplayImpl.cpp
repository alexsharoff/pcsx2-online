#include "NetplayImpl.h"
#include "Session.h"
#include "Utilities.h"
#include <boost/thread.hpp>
#include <algorithm>


#define PING 1
#define INPUT 2

PingPacket::PingPacket() { _gpid = PING; }
bool PingPacket::toStream(std::ostream& s)
{
	s.write(&_gpid, sizeof(_gpid));
	return true;
}
bool PingPacket::fromStream(std::istream& s)
{
	int pos = s.tellg();
	char gpid;
	s.read(&gpid, sizeof(gpid));
	if(gpid != _gpid)
	{
		s.seekg(pos);
		return false;
	}
	else
		return true;
}

InputPacket::InputPacket() { _gpid = INPUT; }
InputPacket::~InputPacket() { cleanUp(); }
bool InputPacket::toStream(std::ostream& s)
{
	unsigned int frameCount = _frames.size();
	s.write(&_gpid, sizeof(_gpid));
	s.write((char*)&frameCount, sizeof(frameCount));

	if(_frames.size())
		for(int i = 0; i < _frames.size(); i++)
			_frames[i].toStream(s);
	return true;
}
bool InputPacket::fromStream(std::istream& s)
{

	int pos = s.tellg();
	unsigned int frameCount;
	char gpid;
	cleanUp();
	if( (s.read(&gpid, sizeof(gpid)) && 
		s.read((char*)&frameCount, sizeof(frameCount)))
		&& gpid == _gpid)
	{
		while(frameCount)
		{
			Frame f;
			if(f.fromStream(s))
			{
				_frames.push_back(f);
			}
			else
				break;
			frameCount--;
		}
		if(!frameCount)
			pos = -1;
	}
	if(pos != -1)
	{
		cleanUp();
		s.seekg(pos);
		return false;
	}

	return true;
}

int InputPacket::getFrameCount() { return _frames.size(); }
Frame InputPacket::getFrame(int i)
{
	if( i < _frames.size())
		return _frames[i];
	return 0;
}

void InputPacket::appendFrame(Frame frame)
{
	_frames.push_back(frame);
}
void InputPacket::cleanUp()
{
	/*for(int i = 0; i < _frames.size(); i++) 
	delete _frames[i];*/
	_frames.clear();
}

NetplayImpl::NetplayImpl() : _session(new Session())
{
	_session->setHandler(this);
	_recvBufferLength = 0;
}

bool NetplayImpl::host(int timeoutMs)
{
	boost::unique_lock<boost::mutex> lock(_mutex);
	if(!_cond_receive.timed_wait(
		lock, boost::posix_time::milliseconds(timeoutMs)))
			return false;
	
	PingPacket ping;
	std::stringstream ss;
	ping.toStream(ss);
	while( !_session->isStopped()) {
		if(getticks() - _lastResponce > timeoutMs)
			return false;
		
		if(_peers.size() > 0)
		{
			_session->send(_peers[0].remote_ep, ss.str().c_str(), ss.str().size());
			if (!(_peers[0].stats.sendReached() < 60 &&
				_peers[0].stats.recvReached() < 60))
				return true;
		}
		sleep(17);
	}
	return false;
}

bool NetplayImpl::connect( const char* remoteIp, int remotePort, int timeoutMs)
{
	std::stringstream ss;
	PingPacket().toStream(ss);
	Endpoint ep;
	ep.ip = remoteIp;
	ep.port = remotePort;
	while(!_session->isStopped()) {
		if(getticks() - _lastResponce > timeoutMs)
			return false;
		
		_session->send(ep, ss.str().c_str(), ss.str().size());
		if(_peers.size() > 0)
		{
			if(_peers[0].remote_ep != ep)
				return false;
			_session->send(ep, ss.str().c_str(), ss.str().size());
			if (!(_peers[0].stats.sendReached() < 60 &&
				_peers[0].stats.recvReached() < 60))
				return true;
		}
		sleep(17);
	}
	return false;
}

int NetplayImpl::watch(const char* ip, int port, int timeoutMs)
{
	return 0;
}

void NetplayImpl::endSession()
{
	boost::unique_lock<boost::mutex> lock(_mutex);
	_session->stop();
	_session->join();
}

const std::vector<Peer> NetplayImpl::getPeers()
{
	return _peers;
}

bool NetplayImpl::saveReplay (const char* fileName)
{
	return 0;
}

bool NetplayImpl::playReplay (const char* fileName)
{
	return 0;
}

Frame NetplayImpl::getInput(int frame, int side, int timeout )
{
	boost::unique_lock<boost::mutex> lock(_mutex);

	if( (timeout > 0) && _cond_receive.timed_wait( lock, 
		boost::posix_time::milliseconds(timeout), 
		framePredicate(this, frame, side)))
		return _input[side].at(frame);

	if(timeout == 0)
	{
		_cond_receive.wait(lock, framePredicate(this, frame, side));
		return _input[side].at(frame);
	}

	if(_pos[side] >= frame)
		return _input[side].at(frame);
	else
		return Frame();
}
void NetplayImpl::setInput(Frame f, int frame, int side)
{
	_mutex.lock();
	f.setSide(side);
	f.setId(frame);
	if( frame >= _input[side].size())
		_input[side].resize(_input[side].size()*2 + 60*60);
	_input[side][frame] = f;
	if(frame > _pos[side])
		_pos[side] = frame;
	_mutex.unlock();
	_cond_receive.notify_all();
}

int NetplayImpl::getInput(char* data, int frame, int side, int timeout)
{
	return -1;
}
void NetplayImpl::setInput(const char* data, int length, int frame, int side)
{
}

void NetplayImpl::sendInput(const Endpoint& ep, int side)
{
	std::stringstream ss;
	InputPacket packet;
	int i = _pos[side];
	_mutex.lock();
	while(true) {
		if(_input[side][i].id() == i)
			packet.appendFrame(_input[side][i]);
		if( (packet.getFrameCount() == 3) || (i == 0))
			break;
		i--;
	}
	if(packet.getFrameCount() != 0)
	{
		packet.toStream(ss);
		_session->send(ep, ss.str().c_str(), ss.str().length());
	}
	_mutex.unlock();
}



int NetplayImpl::getPort()
{
	return _port;
}

void NetplayImpl::setPort(int port)
{
	_port = port;
}

void NetplayImpl::onSent(const Peer* peer, int, const char* data, size_t len)
{
	using namespace std;
}
void NetplayImpl::onReceived(const Peer* peer, int, const char* data, size_t len)
{
	using namespace std;
	if(len > 0) {
		std::stringstream ss(std::string(data, data+len));

		PingPacket ping;
		InputPacket input;
		if(ping.fromStream(ss)) {
			if( std::find(_peers.begin(), _peers.end(), *peer) == _peers.end())
			{
				//ограничение на количество пиров?
				_peers.push_back(*peer);
			}

			_lastResponce = getticks();
			_cond_receive.notify_all();
			return;
		}
		if(input.fromStream(ss)) {
			if( _peers.size()==0 || _peers[0] != *peer)
				return;
			else
				_lastResponce = getticks();
			for(int i = 0; i < input.getFrameCount(); i++)
			{
				Frame f = input.getFrame(i);
				if(f.side() > 1)
				{
					//ERROR
				}
				else
				{
					setInput(f, f.id(), f.side());
					return;
				}
			}
		}
		
		if(_peers.size()!= 0 && _peers[0] == *peer)
		{
			memcpy(_recvBuffer, data, len);
			_recvBufferLength = len;
			_lastResponce = getticks();
			_cond_recvraw.notify_all();
			return;
		}
	}
}
void NetplayImpl::onError(const std::exception& exc, const Endpoint* ep)
{
	using namespace std;
	//cout << "Netplay log: from " << ep->ip << ":" << ep->port << " - " << exc.what() << endl;
}
bool NetplayImpl::prepare()
{
	_input[0].clear();
	_input[1].clear();
	_pos[0] = -1;
	_pos[1] = -1;
	_lastResponce = getticks();
	_sessionStart = getticks();
	return true;
}

bool NetplayImpl::start(int myPort)
{
	try {
		_session->start(myPort);
	}
	catch(...) {
		_session->stop();
		_session->join();
		return false;
	}
	return prepare();
}

void NetplayImpl::send(const Endpoint& ep, char* data, int length)
{
	_session->send(ep, data, length);
}

int NetplayImpl::recv(const Endpoint& ep, char* data, int length, int timeout)
{
	boost::unique_lock<boost::mutex> lock(_mutex);
	_cond_recvraw.timed_wait(lock, boost::posix_time::milliseconds(timeout));
	
	if(_recvBufferLength > length)
		return -1;
	if(_recvBufferLength)
		memcpy(data, _recvBuffer, _recvBufferLength);
	return _recvBufferLength;
}

unsigned long long NetplayImpl::lastResponce()
{
	return _lastResponce;
}