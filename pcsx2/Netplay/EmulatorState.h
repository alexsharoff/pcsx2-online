#include "App.h"
#include <cstdint>
#include "shoryu\archive.h"
struct EmulatorSyncState
{
	EmulatorSyncState();
	char biosVersion[32];
	char discSerial[15];
	uint64_t mcd1CRC;
	uint64_t mcd2CRC;
	void serialize(shoryu::oarchive& a) const;
	void deserialize(shoryu::iarchive& a);
};