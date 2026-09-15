#pragma once

#include "Archive/Archive.h"

namespace slade
{
class NitemareArchive : public TreelessArchive
{
public:
	NitemareArchive() : TreelessArchive("nitemare") {}

	bool open(MemChunk& mc) override;
	bool write(MemChunk& mc, bool update = true) override;
	bool loadEntryData(ArchiveEntry* entry) override;

	static bool isNitemareArchive(MemChunk& mc);
	static bool isNitemareArchive(const string& filename);

private:
	enum class Kind { Unknown, Dat, Img, Map };
	Kind            kind_ = Kind::Unknown;
	vector<uint8_t> header_;
	vector<uint32_t> original_sizes_;
	vector<uint32_t> original_offsets_;
	vector<uint8_t> original_data_;
};
} // namespace slade
