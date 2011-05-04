#pragma once
#include "Header.h"
#include "dllapi.h"

class NETPLAY_API PeerStatistics {
	friend class SessionImpl;
public:
    PeerStatistics();
	~PeerStatistics();
    unsigned int rtt();
    unsigned int maxRtt();
    unsigned int minRtt();
    unsigned int sendCount();
    unsigned int recvOverlapped();
    unsigned int recvCount();
    unsigned int recvReached();
    unsigned int sendReached();
protected:
    bool parseHeader(const Header& header);
    Header newHeader(int bodyLength);
	class PeerStatisticsImpl* p_impl;
};
