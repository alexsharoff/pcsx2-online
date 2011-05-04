#pragma once
#include "CircularBuffer.h"

template<int Size>
class CircularStream {
public:
    CircularStream() {
        _readPos = -1;
    }
	void clear() {
		_readPos = -1;
		_chunks.clear();
	}
    int readNextBlock(char* block, int blockLen) {
        if(! (_chunks.isSet(_readPos + 1))) {
            return -1;
        }
        chunk ch = _chunks.get(_readPos + 1);
        if(ch.len <= blockLen) {
            _readPos++;
            memcpy(block, _buffer+ch.pos, ch.len);
            return ch.len;
        }
        else {
            return -1;
        }
    }
    bool writeBlock(char* block, int blockLen) {
        if(blockLen > Size) {
            return false;
        }
        int writePos = 0;
        if(_chunks.lastIndex() > 0) {
            writePos = _chunks.get(_chunks.lastIndex()).pos +
                    _chunks.get(_chunks.lastIndex()).len;
        }
		if(writePos+blockLen > Size) {
            writePos = 0;
        }
        chunk ch;
        ch.pos = writePos;
        ch.len = blockLen;
        memcpy(_buffer+writePos, block, blockLen);
        int last = _chunks.lastIndex()+1;
        _chunks.set(last, ch);
        return true;
    }
private:
    int _readPos;

    char _buffer[Size];
    struct chunk {
        int pos;
        int len;
    };
    CircularBuffer<chunk,Size> _chunks;
};
