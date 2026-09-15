// Nitemare 3-D resource support for SLADE
#include "Main.h"
#include "NitemareArchive.h"
#include "Archive/EntryType/EntryType.h"

using namespace slade;

namespace
{
constexpr size_t MapHeaderSize = 514;
constexpr size_t MapSize       = 64 * 64 * 2;

uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p)
{
	return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
		   | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
void put16(MemChunk& mc, uint16_t value)
{
	const uint8_t b[2] = { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8) };
	mc.write(b, 2);
}
void put32(MemChunk& mc, uint32_t value)
{
	const uint8_t b[4] = { static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
		static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24) };
	mc.write(b, 4);
}

bool detectMap(const uint8_t* data, size_t size)
{
	if (size < MapHeaderSize + MapSize)
		return false;
	const auto count = le16(data);
	return count > 0 && count <= 64 && size == MapHeaderSize + static_cast<size_t>(count) * MapSize;
}

bool detectImg(const uint8_t* data, size_t size)
{
	if (size < 18)
		return false;
	const auto first = le32(data + 4);
	if (first < 8 || first >= size || (first & 3u))
		return false;
	size_t pos = first;
	unsigned images = 0;
	while (pos < size)
	{
		if (size - pos < 10)
			return false;
		const size_t width = data[pos];
		const size_t height = data[pos + 1];
		if (!width || !height || width * height > size - pos - 10)
			return false;
		pos += 10 + width * height;
		++images;
	}
	return pos == size && images > 1;
}

bool detectDat(const uint8_t* data, size_t size)
{
	if (size < 12)
		return false;
	const auto first_offset = le32(data + 2);
	if (first_offset < 6 || first_offset > size)
		return false;
	for (size_t pos = 0; pos + 6 <= first_offset; pos += 6)
	{
		const auto length = le16(data + pos);
		const auto offset = le32(data + pos + 2);
		if (!length && !offset)
			continue;
		if (offset < first_offset || static_cast<uint64_t>(offset) + length > size)
			return false;
		if (offset + length == size)
			return true;
	}
	return false;
}

string datEntryName(const uint8_t* data, size_t size, unsigned index)
{
	if (size >= 4 && data[0] == 0x0a && data[2] == 1)
		return fmt::format("PCX{:04}.PCX", index);
	if (size >= 4 && !memcmp(data, "MThd", 4))
		return fmt::format("MUSIC{:04}.MID", index);
	return fmt::format("ENTRY{:04}.BIN", index);
}
} // namespace

bool NitemareArchive::open(MemChunk& mc)
{
	if (!mc.hasData()) return false;
	const auto* data = mc.data();
	const auto size = mc.size();
	if (detectMap(data, size)) kind_ = Kind::Map;
	else if (detectImg(data, size)) kind_ = Kind::Img;
	else if (detectDat(data, size)) kind_ = Kind::Dat;
	else return false;

	ArchiveModSignalBlocker blocker{ *this };
	header_.clear();
	original_sizes_.clear();
	original_offsets_.clear();
	original_data_.clear();
	if (kind_ == Kind::Map)
	{
		header_.assign(data, data + MapHeaderSize);
		const auto count = le16(data);
		for (unsigned i = 0; i < count; ++i)
		{
			auto entry = std::make_shared<ArchiveEntry>(fmt::format("MAP{:02}.N3M", i + 1), MapSize);
			entry->importMem(data + MapHeaderSize + i * MapSize, MapSize);
			EntryType::detectEntryType(*entry);
			entry->setState(ArchiveEntry::State::Unmodified);
			rootDir()->addEntry(entry);
			original_sizes_.push_back(MapSize);
		}
	}
	else if (kind_ == Kind::Img)
	{
		const auto first = le32(data + 4);
		header_.assign(data, data + first);
		size_t pos = first;
		unsigned i = 0;
		while (pos < size)
		{
			const size_t entry_size = 10 + static_cast<size_t>(data[pos]) * data[pos + 1];
			auto entry = std::make_shared<ArchiveEntry>(fmt::format("IMAGE{:04}.N3I", i), entry_size);
			entry->importMem(data + pos, entry_size);
			EntryType::detectEntryType(*entry);
			entry->setState(ArchiveEntry::State::Unmodified);
			rootDir()->addEntry(entry);
			original_sizes_.push_back(entry_size);
			pos += entry_size;
			++i;
		}
	}
	else
	{
		const auto first = le32(data + 2);
		header_.assign(data, data + first);
		original_data_.assign(data, data + size);
		for (unsigned i = 0; i * 6 + 6 <= first; ++i)
		{
			const auto entry_size = le16(data + i * 6);
			const auto offset = le32(data + i * 6 + 2);
			const auto name = entry_size ? datEntryName(data + offset, entry_size, i) : fmt::format("EMPTY{:04}.BIN", i);
			auto entry = std::make_shared<ArchiveEntry>(name, entry_size);
			if (entry_size) entry->importMem(data + offset, entry_size);
			EntryType::detectEntryType(*entry);
			entry->setState(ArchiveEntry::State::Unmodified);
			rootDir()->addEntry(entry);
			original_sizes_.push_back(entry_size);
			original_offsets_.push_back(offset);
			if (entry_size && offset + entry_size == size) break;
		}
	}
	blocker.unblock();
	setModified(false);
	return true;
}

bool NitemareArchive::write(MemChunk& mc, bool update)
{
	mc.clear();
	bool data_already_written = false;
	if (kind_ == Kind::Unknown) return false;
	if (kind_ == Kind::Map)
	{
		if (numEntries() == 0 || numEntries() > 64)
		{
			global::error = "Nitemare MAP must contain 1 to 64 maps"; return false;
		}
		for (unsigned i = 0; i < numEntries(); ++i)
			if (entryAt(i)->size() != MapSize)
			{
				global::error = "Every Nitemare map entry must be exactly 8192 bytes"; return false;
			}
		header_[0] = static_cast<uint8_t>(numEntries());
		header_[1] = static_cast<uint8_t>(numEntries() >> 8);
		mc.write(header_.data(), header_.size());
	}
	else if (kind_ == Kind::Img)
	{
		if (numEntries() != original_sizes_.size())
		{
			global::error = "Adding or removing Nitemare IMG records is not supported"; return false;
		}
		for (unsigned i = 0; i < numEntries(); ++i)
			if (entryAt(i)->size() != original_sizes_[i])
			{
				global::error = "Resizing Nitemare IMG records would invalidate its lookup tables"; return false;
			}
		mc.write(header_.data(), header_.size());
	}
	else
	{
		bool same_layout = numEntries() == original_sizes_.size();
		for (unsigned i = 0; same_layout && i < numEntries(); ++i)
			same_layout = entryAt(i)->size() == original_sizes_[i];
		if (same_layout)
		{
			mc.write(original_data_.data(), original_data_.size());
			for (unsigned i = 0; i < numEntries(); ++i)
				if (entryAt(i)->size())
				{
					mc.seek(original_offsets_[i], SEEK_SET);
					mc.write(entryAt(i)->rawData(), entryAt(i)->size());
				}
			mc.seek(0, SEEK_END);
			data_already_written = true;
			goto finish_entries;
		}
		if (numEntries() == 0 || numEntries() * 6 > header_.size())
		{
			global::error = "Too many entries for the original Nitemare DAT directory"; return false;
		}
		uint32_t offset = header_.size();
		for (unsigned i = 0; i < numEntries(); ++i)
		{
			if (entryAt(i)->size() > 65535)
			{
				global::error = "Nitemare DAT entries cannot exceed 65535 bytes"; return false;
			}
			put16(mc, static_cast<uint16_t>(entryAt(i)->size()));
			put32(mc, entryAt(i)->size() ? offset : 0);
			if (entryAt(i)->size()) offset += entryAt(i)->size();
		}
		if (mc.size() < header_.size())
		{
			const vector<uint8_t> padding(header_.size() - mc.size(), 0);
			mc.write(padding.data(), padding.size());
		}
	}
	finish_entries:
	for (unsigned i = 0; i < numEntries(); ++i)
	{
		auto* entry = entryAt(i);
		if (!data_already_written)
			if (entry->size()) mc.write(entry->rawData(), entry->size());
		if (update) entry->setState(ArchiveEntry::State::Unmodified);
	}
	return true;
}

bool NitemareArchive::loadEntryData(ArchiveEntry* entry)
{
	if (!checkEntry(entry)) return false;
	entry->setLoaded(); return true;
}

bool NitemareArchive::isNitemareArchive(MemChunk& mc)
{
	return mc.hasData() && (detectMap(mc.data(), mc.size()) || detectImg(mc.data(), mc.size()) || detectDat(mc.data(), mc.size()));
}

bool NitemareArchive::isNitemareArchive(const string& filename)
{
	MemChunk mc;
	return mc.importFile(filename) && isNitemareArchive(mc);
}
