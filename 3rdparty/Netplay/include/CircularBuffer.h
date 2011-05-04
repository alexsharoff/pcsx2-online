#pragma once

template<typename Type, int Size>
class CircularBuffer {
public:
	CircularBuffer() {
		clear();
	}
	int lastIndex() {
		return m_bufferIndex;
	}
	void clear() {
		memset(m_bufferFlags, 0, Size);
		m_bufferIndex = -1;
	}
	bool isSet(int i) {
		if( !isOutOfBounds(i)) {
			return m_bufferFlags[i%Size];
		}
		return false;
	}
	Type& get(int i) {
		if(!isOutOfBounds(i) && m_bufferFlags[i%Size]) {
			return m_buffer[i%Size];
		}
		else
			throw std::out_of_range("CircularBuffer.get: Index is out of range");
	}
	void set(int i, Type obj) {
		if(i < 0)
			return;

		int delta = i - m_bufferIndex;
		if(delta > 0) {
			if(delta >=Size) {
				memset(m_bufferFlags, 0, Size);
			}
			else if (delta > 1) {
				int bufferIndexMod = m_bufferIndex%Size;
				int indexMod = i%Size;
				if(indexMod < bufferIndexMod) {
					memset(m_bufferFlags + bufferIndexMod+1, 0, Size-bufferIndexMod);
					if(indexMod > 0)
						memset(m_bufferFlags, 0, indexMod);
				}
				else {
					memset(m_bufferFlags+bufferIndexMod+1, 0, indexMod - bufferIndexMod -1);
				}
			}
			m_bufferIndex = i;
		}
		m_buffer[i%Size] = obj;
		m_bufferFlags[i%Size] = true;
	}
	bool isOutOfBounds(int i) {
		if(i > m_bufferIndex || m_bufferIndex - i >= Size)
			return true;
		return false;
	}
	int size() {
		return Size;
	}
private:
	Type m_buffer[Size];
	bool m_bufferFlags[Size];
	int m_bufferIndex;
};
