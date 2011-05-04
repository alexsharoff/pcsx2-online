#pragma once
#include "CircularBuffer.h"
#include "CircularStream.h"
#include "Header.h"
#include "Protocol.h"

class PeerStatisticsImpl {
	friend class PeerStatistics;
public:
    PeerStatisticsImpl();
    unsigned int rtt();
    unsigned int maxRtt();
    unsigned int minRtt();
    unsigned int sendCount();
    unsigned int recvOverlapped();
    unsigned int recvCount();
    unsigned int recvReached();
    unsigned int sendReached();
private:
    bool parseHeader(const Header& header);
    Header newHeader(int bodyLength);

    void parseRttData(const char* data);
    void writeRttData(char* data);
    void parseAck(unsigned long long ackData) ;
    unsigned long long getAck();
    CircularBuffer<int, 32> _recvBuffer;
    CircularBuffer<bool, 32> _sendReachedBuffer;

    unsigned int _sendCount;
    unsigned int _recvCount;

    unsigned int _recvReached;
    unsigned int _sendReached;

    unsigned int _recvOverlapped;
    unsigned int _sendOverlapped;

    unsigned int _rtt;
    unsigned int _maxRtt;
    unsigned int _minRtt;

    unsigned long long _sendStamp;
    unsigned long long _correctionStamp;
};
