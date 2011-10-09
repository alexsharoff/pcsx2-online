#include "App.h"
#include <cstdint>
#include "shoryu\archive.h"
struct EmulatorSyncState
{
	EmulatorSyncState();
	char biosVersion[32];
	char discSerial[15];
	void serialize(shoryu::oarchive& a) const;
	void deserialize(shoryu::iarchive& a);
};
