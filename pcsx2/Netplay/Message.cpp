#include "PrecompiledHeader.h"
#include "Message.h"

const char defaultInput[] = {0xff, 0xff, 0x7f, 0x7f, 0x7f, 0x7f};
Message::Message()
{
	end_session = false;
	std::copy(defaultInput, defaultInput + sizeof(defaultInput), input);
}
void Message::serialize(shoryu::oarchive& a) const
{
	a.write((char*)input, sizeof(input));
	a.write((char*)&end_session, sizeof(bool));
}
void Message::deserialize(shoryu::iarchive& a)
{
	a.read(input, sizeof(input));
	a.read((char*)&end_session, sizeof(bool));
}
