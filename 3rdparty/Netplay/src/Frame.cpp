#include "Frame.h"
#include <memory>


Frame::Frame(const Frame& copy) : _id(copy._id), _side(copy._side),
				_length(copy._length),_data(new char[copy._length])
{
	char*p = copy._data.get();
	std::copy(p, p+copy._length, _data.get());
}

Frame::Frame(int id)
{
	_id = id;
	_length = 0;
	_side = 0;
}

Frame::Frame(int id , const char* data , int length ) : _data(new char[length])
{
	_id = id;
	std::copy(data, data+length, _data.get());
	_length = length;
	_side = 0;
}

bool Frame::toStream(std::ostream& s)
{
	s.write((char*)&_id, sizeof(_id));
	s.write((char*)&_side, sizeof(_side));
	s.write((char*)&_length, sizeof(_length));
	s.write(_data.get(), _length);

	return true;
}
bool Frame::fromStream(std::istream& s)
{
	int pos = s.tellg();
	cleanUp();
	if( s.read((char*)&_id, sizeof(_id)) && 
		s.read((char*)&_side, sizeof(_side)) &&
		s.read((char*)&_length, sizeof(_length)) )
	{
		_data = std::auto_ptr<char>(new char[_length]);
		if(s.read(_data.get(), _length))
			pos = -1;
	}

	if(pos != -1)
	{
		s.seekg(pos);
		cleanUp();
		return false;
	}
	return true;
}

int Frame::id() { return _id; }
void Frame::setId(int id) { _id = id; }

int Frame::size() { return _length; }

void Frame::setData(const char* data, int length) 
{
	cleanUp();
	if(length != _length)
		_data = std::auto_ptr<char>(new char[_length]);
	_length = length;
	memcpy(_data.get(), data, _length);
}

char* Frame::data()
{
	return _data.get();
}

unsigned char Frame::side() { return _side; }

void Frame::setSide(unsigned char side) { _side = side; }

void Frame::cleanUp()
{
	_data.release();
	_length = _id = 0;
}
