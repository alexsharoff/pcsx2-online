#include "PrecompiledHeader.h"
#include "EmulatorState.h"

EmulatorSyncState::EmulatorSyncState()
{
	memset(biosVersion, 0, sizeof(biosVersion));
	memset(discSerial, 0, sizeof(discSerial));
}

void EmulatorSyncState::serialize(shoryu::oarchive& a) const
{
	a.write((char*)discSerial, sizeof(discSerial));
	a.write((char*)biosVersion, sizeof(biosVersion));
}
void EmulatorSyncState::deserialize(shoryu::iarchive& a)
{
	a.read((char*)discSerial, sizeof(discSerial));
	a.read((char*)biosVersion, sizeof(biosVersion));
}
