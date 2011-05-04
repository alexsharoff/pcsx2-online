
class RawIos {
public:
	RawIos() {
		_file.open("logs\\ios.rep", std::ios_base::out | std::ios_base::trunc|
			std::ios_base::binary);
	}
	void writeByte(u8 byte) {
		_file << byte;
	}
	void next() {
		_file << '\n';
	}
private:
	std::fstream _file;
} raw;