#pragma once
#include "dllapi.h"
#include <iostream>
#include <memory>

class Packet
{
public:
	virtual bool fromStream(std::istream& s) = 0;
	virtual bool toStream(std::ostream& s) = 0;
};

class NETPLAY_API Frame : Packet {
public:
	Frame(int id = -1);
	Frame(int id, const char* data , int length );
	Frame(const Frame& copy);

	bool toStream(std::ostream& s);
	bool fromStream(std::istream& s);

	int id();
	void setId(int id) ;

	int size();
	void setData(const char* data, int length) ;
	char* data();

	unsigned char side();
	void setSide(unsigned char side);
protected:
	void cleanUp();
	unsigned int _id;
	unsigned char _length;
	unsigned char _side;
	std::auto_ptr<char> _data;
};
