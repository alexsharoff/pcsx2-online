#include "stdafx.h"
#include "Session.h"
#include "SessionImpl.h"

using boost::asio::ip::udp;

bool Peer::operator==(const Peer& other)
{
	return remote_ep == other.remote_ep;
}

bool Peer::operator!=(const Peer& other)
{
	return !(*this == other);
}

Session::Session(): p_impl(new SessionImpl()) { }
Session::~Session() {
	delete p_impl;
}

void Session::start(unsigned short port) {
	p_impl->start(port);
}

void Session::stop() {
	p_impl->stop();
}
void Session::join() {
	p_impl->join();
}

void Session::send(const Endpoint& endPoint, const char* data, int dataLength) {
	p_impl->send(endPoint, data, dataLength);
}

void Session::setHandler(Handler* hdl)
{
	p_impl->setHandler(hdl);
}


bool Session::isStopped()
{
	return p_impl->_stop;
}