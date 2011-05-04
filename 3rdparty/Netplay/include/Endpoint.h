#pragma once
#include "dllapi.h"
#include <string>

struct NETPLAY_API Endpoint {
	std::string ip;
	unsigned short port;
	bool operator==(const Endpoint& other);
	bool operator!=(const Endpoint& other);
};