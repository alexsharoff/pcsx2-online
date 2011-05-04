#include "Session.h"
#include <vector>
#include <boost/thread.hpp>
#include "Frame.h"
#include <memory>

class PingPacket : Packet {
public:
	PingPacket();
	bool toStream(std::ostream& s);
	bool fromStream(std::istream& s);
protected:
	char _gpid;
};

class InputPacket : Packet {
public:
	InputPacket();
	~InputPacket();
	bool toStream(std::ostream& s);
	bool fromStream(std::istream& s);

	int getFrameCount();
	Frame getFrame(int i);

	void appendFrame(Frame frame);
protected:
	void cleanUp();
	char _gpid;
	std::vector<const Frame> _frames;
};


enum NetplayMode
{
	None = 0,
	Normal = 1,
	Watch = 2,
	Replay = 3
};

class NetplayImpl : Handler
{
	friend class Netplay;
public:
	NetplayImpl();

	bool start(int myPort);
	bool host(int timeoutMs);
	bool connect(const char* remoteIp, int remotePort, int timeoutMs);
	int watch(const char* ip, int port, int timeoutMs);

	void endSession();

	const std::vector<Peer> getPeers();

	bool saveReplay (const char* fileName);

	bool playReplay (const char* fileName);

	int getInput(char* data, int frame, int side, int timeout = -1);
	void setInput(const char* data, int length, int frame, int side);

	Frame getInput(int frame, int side, int timeout = -1);
	void setInput(Frame f, int frame, int side);

	void sendInput(const Endpoint& ep, int side);

	int getPort();

	void setPort(int port);

	void onSent(const Peer* peer, int, const char* data, size_t len);
	void onReceived(const Peer* peer, int, const char* data, size_t len);
	void onError(const std::exception& exc, const Endpoint* ep);

	void send(const Endpoint& ep, char* data, int length);
	int recv(const Endpoint& ep, char* data, int length, int timeout);

	unsigned long long lastResponce();
protected:
	char _recvBuffer[1024]; //lolbuffer
	int _recvBufferLength;
	bool prepare();
	std::auto_ptr<Session> _session;
	int _port;
	std::vector<Peer> _peers;
	boost::condition_variable _cond_receive;
	boost::condition_variable _cond_recvraw;
	boost::mutex _mutex;
	unsigned long long _lastResponce;
	unsigned long long _sessionStart;
	std::vector<const Frame> _input[2];

	struct framePredicate {
		framePredicate(NetplayImpl* np, int frameId, int side) : 
	_id(frameId), _side(side), _np(np) {}
	bool operator()() {
		return (_np->_input[_side].size() >_id) && 
			(_np->_input[_side].at(_id).id() == _id);
	}
	private:
		int _id, _side;
		NetplayImpl* _np;
	};

	int _pos[2];
};