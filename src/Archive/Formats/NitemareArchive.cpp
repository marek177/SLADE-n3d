// Nitemare 3-D resource support for SLADE
#include "Main.h"
#include "NitemareArchive.h"
#include "Archive/EntryType/EntryType.h"
#include "Graphics/Palette/Palette.h"
#include "Graphics/SImage/SImage.h"
#include "Graphics/SImage/SIFormat.h"

#include <filesystem>

using namespace slade;

namespace
{
constexpr size_t MapHeaderSize = 514;
constexpr size_t MapSize       = 64 * 64 * 2;
constexpr int    MapPreviewScale = 8;
constexpr uint32_t SoundRate = 10989;

uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p)
{
	return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
		   | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
void append16(vector<uint8_t>& data, uint16_t value)
{
	data.push_back(static_cast<uint8_t>(value));
	data.push_back(static_cast<uint8_t>(value >> 8));
}
void append32(vector<uint8_t>& data, uint32_t value)
{
	data.push_back(static_cast<uint8_t>(value));
	data.push_back(static_cast<uint8_t>(value >> 8));
	data.push_back(static_cast<uint8_t>(value >> 16));
	data.push_back(static_cast<uint8_t>(value >> 24));
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

Palette nitemarePalette(string_view archive_filename)
{
	Palette palette;
	for (unsigned i = 0; i < 256; ++i)
		palette.setColour(i, ColRGBA(i, i, i, 255));

	const auto directory = std::filesystem::path{ archive_filename }.parent_path();
	for (const auto* name : { "GAME.PAL", "game.pal" })
	{
		MemChunk pcx;
		if (!pcx.importFile((directory / name).string()) || pcx.size() < 769)
			continue;
		const auto palette_marker = pcx.size() - 769;
		if (pcx[palette_marker] == 12)
		{
			palette.loadMem(pcx.data() + palette_marker + 1, 768);
			break;
		}
	}
	return palette;
}

bool makeImagePng(const uint8_t* data, size_t size, Palette& palette, MemChunk& png)
{
	if (size < 11)
		return false;
	const int width = data[0];
	const int height = data[1];
	if (!width || !height || size != static_cast<size_t>(10 + width * height))
		return false;

	// Index 31 is the transparent sprite colour in Nitemare 3-D. Only make it
	// transparent when it dominates the image border, so wall textures retain
	// any legitimate pixels using the same palette index.
	size_t border_pixels = 0;
	size_t border_transparent = 0;
	for (int x = 0; x < width; ++x)
		for (int y : { 0, height - 1 })
		{
			++border_pixels;
			if (data[10 + x * height + y] == 31) ++border_transparent;
		}
	for (int y = 1; y + 1 < height; ++y)
		for (int x : { 0, width - 1 })
		{
			++border_pixels;
			if (data[10 + x * height + y] == 31) ++border_transparent;
		}
	const bool transparent_31 = border_pixels && border_transparent * 2 >= border_pixels;

	SImage image;
	image.create(width, height, SImage::Type::PalMask, &palette);
	for (int x = 0; x < width; ++x)
		for (int y = 0; y < height; ++y)
		{
			const auto pixel = data[10 + x * height + y];
			image.setPixel(x, y, pixel, transparent_31 && pixel == 31 ? 0 : 255);
		}
	return SIFormat::getFormat("png")->saveImage(image, png, &palette);
}

ColRGBA mapWallColour(uint8_t wall)
{
	if (!wall)
		return ColRGBA(24, 24, 28, 255);
	return ColRGBA(
		static_cast<uint8_t>(55 + (wall * 73u) % 176u),
		static_cast<uint8_t>(55 + (wall * 131u) % 176u),
		static_cast<uint8_t>(55 + (wall * 197u) % 176u),
		255);
}

bool makeMapPng(const uint8_t* data, size_t size, MemChunk& png)
{
	if (size != MapSize)
		return false;
	SImage image;
	image.create(64 * MapPreviewScale, 64 * MapPreviewScale, SImage::Type::RGBA);
	for (int y = 0; y < 64; ++y)
		for (int x = 0; x < 64; ++x)
		{
			const auto cell = static_cast<size_t>(y * 64 + x) * 2;
			const auto wall = data[cell];
			const auto object = data[cell + 1];
			const auto wall_colour = mapWallColour(wall);
			for (int py = 0; py < MapPreviewScale; ++py)
				for (int px = 0; px < MapPreviewScale; ++px)
				{
					auto colour = wall_colour;
					if (px == 0 || py == 0)
						colour = ColRGBA(colour.r / 2, colour.g / 2, colour.b / 2, 255);
					if (object && px >= 2 && px <= 5 && py >= 2 && py <= 5)
						colour = ColRGBA(255, static_cast<uint8_t>(32 + object % 96), 32, 255);
					image.setPixel(x * MapPreviewScale + px, y * MapPreviewScale + py, colour);
				}
		}
	return SIFormat::getFormat("png")->saveImage(image, png);
}

vector<uint8_t> makeWave(const uint8_t* samples, size_t size)
{
	vector<uint8_t> wave;
	const auto padding = static_cast<uint32_t>(size & 1u);
	wave.reserve(44 + size + padding);
	wave.insert(wave.end(), { 'R', 'I', 'F', 'F' });
	append32(wave, static_cast<uint32_t>(36 + size + padding));
	wave.insert(wave.end(), { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
	append32(wave, 16);
	append16(wave, 1); // PCM
	append16(wave, 1); // mono
	append32(wave, SoundRate);
	append32(wave, SoundRate); // 8-bit mono: one byte per sample
	append16(wave, 1);
	append16(wave, 8);
	wave.insert(wave.end(), { 'd', 'a', 't', 'a' });
	append32(wave, static_cast<uint32_t>(size));
	wave.insert(wave.end(), samples, samples + size);
	if (padding) wave.push_back(0);
	return wave;
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
	original_data_.assign(data, data + size);
	if (kind_ == Kind::Map)
	{
		header_.assign(data, data + MapHeaderSize);
		const auto count = le16(data);
		for (unsigned i = 0; i < count; ++i)
		{
			MemChunk preview;
			if (!makeMapPng(data + MapHeaderSize + i * MapSize, MapSize, preview)) return false;
			auto entry = std::make_shared<ArchiveEntry>(fmt::format("MAP{:02}.PNG", i + 1), preview.size());
			entry->importMemChunk(preview);
			entry->setType(EntryType::fromId("png"), 255);
			entry->setState(ArchiveEntry::State::Unmodified);
			rootDir()->addEntry(entry);
		}
	}
	else if (kind_ == Kind::Img)
	{
		auto palette = nitemarePalette(filename());
		const auto first = le32(data + 4);
		header_.assign(data, data + first);
		size_t pos = first;
		unsigned i = 0;
		while (pos < size)
		{
			const size_t entry_size = 10 + static_cast<size_t>(data[pos]) * data[pos + 1];
			MemChunk png;
			if (!makeImagePng(data + pos, entry_size, palette, png)) return false;
			auto entry = std::make_shared<ArchiveEntry>(fmt::format("IMAGE{:04}.PNG", i), png.size());
			entry->importMemChunk(png);
			entry->setType(EntryType::fromId("png"), 255);
			entry->setState(ArchiveEntry::State::Unmodified);
			rootDir()->addEntry(entry);
			pos += entry_size;
			++i;
		}
	}
	else
	{
		const bool sound_archive = strutil::equalCI(filename(false), "SND.DAT");
		const auto first = le32(data + 2);
		header_.assign(data, data + first);
		for (unsigned i = 0; i * 6 + 6 <= first; ++i)
		{
			const auto entry_size = le16(data + i * 6);
			const auto offset = le32(data + i * 6 + 2);
			const bool raw_sound = sound_archive && i >= 34 && entry_size;
			const auto name = raw_sound ? fmt::format("SOUND{:04}.WAV", i)
				: (entry_size ? datEntryName(data + offset, entry_size, i) : fmt::format("EMPTY{:04}.BIN", i));
			auto entry = std::make_shared<ArchiveEntry>(name);
			if (raw_sound)
			{
				auto wave = makeWave(data + offset, entry_size);
				entry->importMem(wave.data(), wave.size());
				entry->setType(EntryType::fromId("snd_wav"), 255);
			}
			else
			{
				if (entry_size) entry->importMem(data + offset, entry_size);
				EntryType::detectEntryType(*entry);
			}
			entry->setState(ArchiveEntry::State::Unmodified);
			rootDir()->addEntry(entry);
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
	if (original_data_.empty()) return false;
	mc.write(original_data_.data(), original_data_.size());
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
