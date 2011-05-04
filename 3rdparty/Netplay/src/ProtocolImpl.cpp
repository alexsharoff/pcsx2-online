#include "stdafx.h"
#include "CircularBuffer.h"
#include "Utilities.h"
#include "Protocol.h"
#include "ProtocolImpl.h"
#include <sstream>
#include <bitset>
#include <boost/asio.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/functional/hash.hpp>
#include <boost/thread.hpp>

PeerStatisticsImpl::PeerStatisticsImpl() {
	_rtt = 0;
	_maxRtt = 0;
	_minRtt = 15000;

	_recvOverlapped = 0;
	_sendOverlapped = 0;
	_recvReached = 0;
	_sendReached = 0;
	_sendCount = 0;
	_recvCount = 0;

	_sendStamp = 0;
	_correctionStamp = 0;
}

unsigned int PeerStatisticsImpl::rtt() {
	return _rtt;
}
unsigned int PeerStatisticsImpl::maxRtt() {
	return _maxRtt;
}
unsigned int PeerStatisticsImpl::minRtt() {
	return _minRtt;
}
unsigned int PeerStatisticsImpl::sendCount() {
	return _sendCount;
}
unsigned int PeerStatisticsImpl::recvOverlapped() {
	return _recvOverlapped;
}
unsigned int PeerStatisticsImpl::recvCount() {
	return _recvCount;
}
unsigned int PeerStatisticsImpl::recvReached() {
	return _recvReached;
}
unsigned int PeerStatisticsImpl::sendReached() {
	return _sendReached;
}

bool PeerStatisticsImpl::parseHeader(const Header& header) {
	if(header.proto != PROTOCOLID) {
		return false;
	}
	else
		if(!_recvBuffer.isSet(header.id)) {
			_recvReached++;
			if(_recvCount+1 != header.id) {
				_recvOverlapped++;
			}
			if(header.id > _recvCount) {
				_recvCount = header.id;
			}
			_recvBuffer.set(header.id, _recvReached);
			parseAck(header.ack);
			parseRttData(header.rttData);
			return true;
		}
		return false;
}
Header PeerStatisticsImpl::newHeader(int bodyLength) {
	_sendCount++;
	Header header;
	header.id = _sendCount;
	header.ack = getAck();
	writeRttData(header.rttData);
	header.bodyLength = bodyLength;
	return header;
}

void PeerStatisticsImpl::parseRttData(const char* data) {
	

	unsigned long long rttStamp;
	memcpy(&rttStamp, data, 8);
	memcpy(&_sendStamp, data+8, 8);
	_correctionStamp = getticks();

	/*using namespace std;
	stringstream ss;
	ss << "incoming stamps: my=" << rttStamp << "; opponent=" << _sendStamp;
	if(protoLog)
	protoLog(ss.str().c_str(), false);*/

	if(rttStamp != 0) {
		unsigned long long rtt = getticks() - rttStamp;
		_rtt = (unsigned int)ceil(0.3*_rtt + 0.7*rtt);
		if(rtt < _minRtt)
			_minRtt = (unsigned int)rtt;
		if(rtt > _maxRtt)
			_maxRtt = (unsigned int)rtt;
	}
}
void PeerStatisticsImpl::writeRttData(char* data) {
	using namespace std;
	/*stringstream ss;
	ss << "outgoing stamps: my=" << rttStamp << "; opponent=" << _sendStamp << 
		"; corrected=" << sendStampCorrected;
	if(protoLog)
	protoLog(ss.str().c_str(), false);*/
	
	unsigned long long sendStampCorrected = 0;
	if(_sendStamp != 0)
	{
		sendStampCorrected = _sendStamp + (getticks() - _correctionStamp);
	}
	unsigned long long rttStamp = getticks();
	memcpy(data, &sendStampCorrected,8);
	memcpy(data+8, &rttStamp, 8);
}
void PeerStatisticsImpl::parseAck(unsigned long long ackData) {
	int first = ackData & 0xFFFFFFFF;
	unsigned long ackLong = (ackData >> 32) & 0xFFFFFFFF;
	std::bitset<32> ack(ackLong);
	for(int i = 0; i < 32; i++) {
		if(ack[i] && !_sendReachedBuffer.isSet(first+i)) {
			_sendReachedBuffer.set(first+i, true);
			_sendReached++;
			if(_sendReachedBuffer.isSet(first+i+1)) {
				_sendOverlapped++;
			}
		}
	}
}
unsigned long long PeerStatisticsImpl::getAck() {
	using namespace std;
	if(_recvBuffer.lastIndex() != -1) {
		bitset<32> ack(0);
		int first = _recvBuffer.lastIndex()-31;
		if(first < 0)
			first = 0;
		for(int i = first ;i <= _recvBuffer.lastIndex(); i++) {
			if(_recvBuffer.isSet(i)) {
				ack.set( i-first );
			}
		}
		unsigned long long value;
		value = first;
		unsigned long long ackLong = ack.to_ulong();
		value += (ackLong <<32);
		return value;
	}
	return 0;
}