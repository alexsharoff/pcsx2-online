#include "PrecompiledHeader.h"
#include "Utilities.h"
#include "zlib\zlib.h"

size_t Utilities::GetMCDSize(uint port, uint slot)
{
	PS2E_McdSizeInfo info;
	SysPlugins.McdGetSizeInfo(port,slot,info);
	return info.McdSizeInSectors*(info.SectorSize + info.EraseBlockSizeInSectors);
}

Utilities::block_type Utilities::ReadMCD(uint port, uint slot)
{
	block_type mcd;
	size_t size = GetMCDSize(port, slot);
	mcd.resize(size);
	SysPlugins.McdRead(0,0, mcd.data(), 0, size);
	return mcd;
}
void Utilities::WriteMCD(uint port, uint slot, const Utilities::block_type& block)
{
	size_t size = GetMCDSize(port, slot);
	if(block.size() != size)
		throw std::exception("invalid block size");
	for(size_t p = 0; p < size; p+= 528*16)
		SysPlugins.McdEraseBlock(0,0,p);

	SysPlugins.McdSave(port,slot, block.data(), 0, size);
}
bool Utilities::Compress(const Utilities::block_type& uncompressed,
	Utilities::block_type& compressed)
{
	uLongf size = uncompressed.size();
	compressed.resize(size);
	int r = compress2(compressed.data(), &size, uncompressed.data(), size, Z_BEST_COMPRESSION);
	if(r != Z_OK)
		return false;
	if(size < compressed.size())
		compressed.resize(size);
	return true;
}
bool Utilities::Uncompress(const Utilities::block_type& compressed,
	Utilities::block_type& uncompressed)
{
	uLongf size = uncompressed.size();
	int r = uncompress((Bytef*)uncompressed.data(), &size, (Bytef*)compressed.data(), compressed.size());
	if(r != Z_OK)
		return false;
	if(size < uncompressed.size())
		uncompressed.resize(size);
	return true;
}
