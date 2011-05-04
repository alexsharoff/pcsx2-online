#include "Endpoint.h"

	bool Endpoint::operator==(const Endpoint& other)
	{
		return (port == other.port) && (ip == other.ip);
	}
	bool Endpoint::operator!=(const Endpoint& other)
	{
		return !(*this == other);
	}
