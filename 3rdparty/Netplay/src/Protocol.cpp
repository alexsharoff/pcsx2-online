#include "stdafx.h"
#include "CircularBuffer.h"
#include "Utilities.h"
#include "Protocol.h"
#include "Header.h"
#include <sstream>
#include <bitset>
#include "ProtocolImpl.h"



PeerStatistics::PeerStatistics() {
	p_impl = new PeerStatisticsImpl();
}
PeerStatistics::~PeerStatistics() {
	delete p_impl;
}
unsigned int PeerStatistics::rtt() {
	return p_impl->rtt();
}
unsigned int PeerStatistics::maxRtt() {
	return p_impl->maxRtt();
}
unsigned int PeerStatistics::minRtt() {
	return p_impl->minRtt();
}
unsigned int PeerStatistics::sendCount() {
	return p_impl->sendCount();
}
unsigned int PeerStatistics::recvOverlapped() {
	return p_impl->recvOverlapped();
}
unsigned int PeerStatistics::recvCount() {
	return p_impl->recvCount();
}
unsigned int PeerStatistics::recvReached() {
	return p_impl->recvReached();
}
unsigned int PeerStatistics::sendReached() {
	return p_impl->sendReached();
}

bool PeerStatistics::parseHeader(const Header& header) {
	return p_impl->parseHeader(header);
}
Header PeerStatistics::newHeader(int bodyLength) {
	return p_impl->newHeader(bodyLength);
}