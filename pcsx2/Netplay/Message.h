#pragma once
#include "App.h"
#include "EmulatorState.h"
#include "shoryu\archive.h"

struct Message
{
	Message();
	char input[6];
	bool end_session;
	void serialize(shoryu::oarchive& a) const;
	void deserialize(shoryu::iarchive& a);
};