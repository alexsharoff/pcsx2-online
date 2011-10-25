#include "PrecompiledHeader.h"
#include "Replay.h"
#include <fstream>
#include <sstream>
#include <streambuf>
#include <boost/shared_array.hpp>

Replay::Replay() : _playback_frame(0), _mode(None) {}

static const char Header[] = "PCSX2REPV1";

bool Replay::LoadFromFile(const wxString& path)
{
	wxFile file(path);
	bool validFile = false;
	if(file.IsOpened())
	{
		size_t size;
		if(file.Read((char*)&size,sizeof(size)) != sizeof(size))
			return false;
		block_type uncompressed(size);
		size = file.Length() - sizeof(size);
		block_type compressed(size);
		if(file.Read(compressed.data(), size) != size)
			return false;

		if(!Utilities::Uncompress(compressed, uncompressed))
			return false;

		std::istringstream ss(std::string(uncompressed.begin(), uncompressed.end()));
		char header_test[sizeof(Header)];
		if(!ss.read(header_test, sizeof(Header)))
			return false;
		if(memcmp(header_test, Header, sizeof(Header)))
			return false;
		if(!ss.read(_state.biosVersion, sizeof(_state.biosVersion)))
			return false;
		if(!ss.read(_state.discSerial, sizeof(_state.discSerial)))
			return false;
		if(!ss.read((char*)&size, sizeof(size)))
			return false;
		_data.clear();
		_data.resize(size);
		if(size)
		{
			if(!ss.read((char*)_data.data(), size))
				return false;
		}
		if(!ss.read((char*)&size, sizeof(size)))
			return false;
		_input.clear();
		_input.resize(size);
		if(size)
		{
			for(size_t i = 0; i < _input.size(); i++)
			{
				if(!ss.read((char*)&size, sizeof(size)))
					return false;
				if(size)
				{
					for(size_t j = 0; j < size; j++)
					{
						Message m;
						if(!ss.read(m.input, sizeof(m.input)))
							return false;
						_input[i].push_back(m);
					}
				}
			}
		}
	}
	else
		throw std::exception("Cannot open file");
	return true;
}
const Replay& Replay::SaveToFile(const wxString& path) const
{
	wxFile file(path, wxFile::write);
	if(file.IsOpened())
	{
		std::stringstream ss;
		ss.write(Header, sizeof(Header));
		ss.write(_state.biosVersion, sizeof(_state.biosVersion));
		ss.write(_state.discSerial, sizeof(_state.discSerial));
		size_t size = _data.size();
		ss.write((const char*)&size, sizeof(size));
		ss.write((const char*)_data.data(), size);
		size = _input.size();
		ss.write((const char*)&size, sizeof(size));
		for(size_t i = 0; i < _input.size(); i++)
		{
			size = _input[i].size();
			ss.write((const char*)&size, sizeof(size));
			for(size_t j = 0; j < size; j++)
				ss.write(_input[i][j].input, sizeof(_input[i][j].input));
		}
		std::string data = ss.str();
		block_type uncompressed(data.begin(), data.end());
		block_type compressed;
		if(!Utilities::Compress(uncompressed, compressed))
			throw std::exception("Unable to compress data");
		size = uncompressed.size();
		file.Write((char*)&size,sizeof(size));
		file.Write(compressed.data(), compressed.size());
	}
	else
		throw std::exception("Cannot open file");
	return *this;
}
ReplayMode Replay::Mode() const
{
	return _mode;
}
Replay& Replay::Mode(ReplayMode mode)
{
	_mode = mode;
	return *this;
}
const EmulatorSyncState& Replay::SyncState() const
{
	return _state;
}
Replay& Replay::SyncState(const EmulatorSyncState& state)
{
	if(_mode != Recording)
		throw std::exception("Write operation while not in Recording mode");
	_state = state;
	return *this;
}
const Replay::block_type& Replay::Data()
{
	return _data;
}
Replay& Replay::Data(const block_type& data)
{
	if(_mode != Recording)
		throw std::exception("Write operation while not in Recording mode");
	_data = data;
	return *this;
}
const Message& Replay::Read(int side) const
{
	if(_mode != Playback)
		throw std::exception("Read operation while not in Playback mode");
	return _input[side][_playback_frame];
}
Replay& Replay::Write(u8 side, const Message& msg)
{
	if(_mode != Recording)
		throw std::exception("Write operation while not in Recording mode");
	if(_input.size() <= side)
		_input.resize(side+1);
	_input[side].push_back(msg);
	return *this;
}
u64 Replay::Length() const
{
	auto elem = std::min_element(_input.begin(), _input.end(), [&](const input_type& a, const input_type& b){
		return a.size() < b.size();
	});
	input_container::iterator end;
	if(elem != end)
		return elem->size();
	else
		return 0;
}
int Replay::Sides() const
{
	return _input.size();
}
Replay& Replay::Rewind()
{
	if(_mode != Playback)
		throw std::exception("Rewind operation while not in Playback mode");
	_playback_frame = 0;
	return *this;
}
Replay& Replay::Seek(u64 position)
{
	if(_mode != Playback)
		throw std::exception("Seek operation while not in Playback mode");
	_playback_frame = position;
	return *this;
}
Replay& Replay::NextFrame()
{
	if(_mode != Playback)
		throw std::exception("NextFrame operation while not in Playback mode");
	++_playback_frame;
	return *this;
}