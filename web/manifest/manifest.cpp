#include "manifest.h"

#ifndef LOG_SECTION
#define LOG_SECTION "Manifest"
#endif

#include "../../Logger.h"

#include <libdeflate.h>
#include <numeric>
#include <memory>
#include <unordered_set>
#include <sstream>
#include <unordered_map>

inline const void HashToBytes(const char* data, char* output) {
	int size = strlen(data) / 3;
	char buf[4];
	buf[3] = '\0';
	for (int i = 0; i < size; i++)
	{
		buf[0] = data[i * 3];
		buf[1] = data[i * 3 + 1];
		buf[2] = data[i * 3 + 2];
		output[i] = atoi(buf);
	}
}

inline const uint8_t GetByteValue(const char Char)
{
	if (Char >= '0' && Char <= '9')
	{
		return Char - '0';
	}
	else if (Char >= 'A' && Char <= 'F')
	{
		return (Char - 'A') + 10;
	}
	return (Char - 'a') + 10;
}

inline const int HexToBytes(const char* hex, char* output) {
	int NumBytes = 0;
	while (*hex)
	{
		output[NumBytes] = GetByteValue(*hex++) << 4;
		output[NumBytes] += GetByteValue(*hex++);
		++NumBytes;
	}
	return NumBytes;
}

auto guidHash = [](const char* n) { return (*((uint64_t*)n)) ^ (*(((uint64_t*)n) + 1)); };
auto guidEqual = [](const char* a, const char* b) {return !memcmp(a, b, 16); };
typedef std::unordered_map<char*, uint32_t, decltype(guidHash), decltype(guidEqual)> MANIFEST_CHUNK_LOOKUP;

Manifest::Manifest(const rapidjson::Document& jsonData, const std::string& url)
{
	HashToBytes(jsonData["ManifestFileVersion"].GetString(), (char*)&FeatureLevel);
	bIsFileData = jsonData["bIsFileData"].GetBool();
	HashToBytes(jsonData["AppID"].GetString(), (char*)&AppID);
	AppName = jsonData["AppNameString"].GetString();
	BuildVersion = jsonData["BuildVersionString"].GetString();
	LaunchExe = jsonData["LaunchExeString"].GetString();
	LaunchCommand = jsonData["LaunchCommand"].GetString();

#define CHUNK_DIR(dir) "/Chunks" dir "/"
	const char* ChunksDir;
	if (FeatureLevel < EFeatureLevel::DataFileRenames) {
		ChunksDir = CHUNK_DIR("");
	}
	else if (FeatureLevel < EFeatureLevel::ChunkCompressionSupport) {
		ChunksDir = CHUNK_DIR("V2");
	}
	else if (FeatureLevel < EFeatureLevel::VariableSizeChunksWithoutWindowSizeChunkInfo) {
		ChunksDir = CHUNK_DIR("V3");
	}
	else {
		ChunksDir = CHUNK_DIR("V4");
	}
#undef CHUNK_DIR
	CloudDir = url.substr(0, url.find_last_of('/')) + ChunksDir;

	MANIFEST_CHUNK_LOOKUP ChunkManifestLookup; // used to speed up lookups instead of doing a linear search over everything
	{
		const rapidjson::Value& HashList = jsonData["ChunkHashList"];
		const rapidjson::Value& ShaList = jsonData["ChunkShaList"];
		const rapidjson::Value& GroupList = jsonData["DataGroupList"];
		const rapidjson::Value& SizeList = jsonData["ChunkFilesizeList"];

		ChunkManifestList.reserve(HashList.MemberCount());
		ChunkManifestLookup.reserve(HashList.MemberCount());

		int i = 0;
		for (rapidjson::Value::ConstMemberIterator hashItr = HashList.MemberBegin(), shaItr = ShaList.MemberBegin(), groupItr = GroupList.MemberBegin(), sizeItr = SizeList.MemberBegin();
			i != HashList.MemberCount(); ++i, ++hashItr, ++shaItr, ++groupItr, ++sizeItr)
		{
			auto& chunk = ChunkManifestList.emplace_back(std::make_shared<Chunk>());
			HexToBytes(hashItr->name.GetString(), chunk->Guid);
			HashToBytes(hashItr->value.GetString(), (char*)&chunk->Hash);
			HexToBytes(shaItr->value.GetString(), (char*)&chunk->ShaHash);
			chunk->Group = atoi(groupItr->value.GetString());
			chunk->WindowSize = 1048576; // https://github.com/EpicGames/UnrealEngine/blob/f8f4b403eb682ffc055613c7caf9d2ba5df7f319/Engine/Source/Runtime/Online/BuildPatchServices/Private/Data/ChunkData.cpp#L246 (default constructor)
			HashToBytes(sizeItr->value.GetString(), (char*)&chunk->FileSize);

			ChunkManifestLookup[chunk->Guid] = i;
		}
	}

	{
		const rapidjson::Value& FileList = jsonData["FileManifestList"];
		FileManifestList.reserve(FileList.Size());

		for (auto& fileManifest : FileList.GetArray()) {
			File file;
			file.FileName = fileManifest["Filename"].GetString();
			HashToBytes(fileManifest["FileHash"].GetString(), (char*)&file.ShaHash);
			file.ChunkParts.reserve(fileManifest["FileChunkParts"].Size());

			for (auto& fileChunk : fileManifest["FileChunkParts"].GetArray()) {
				auto& part = file.ChunkParts.emplace_back();
				char guidBuffer[16];
				HexToBytes(fileChunk["Guid"].GetString(), guidBuffer);
				part.Chunk = ChunkManifestList[ChunkManifestLookup[guidBuffer]];
				HashToBytes(fileChunk["Offset"].GetString(), (char*)&part.Offset);
				HashToBytes(fileChunk["Size"].GetString(), (char*)&part.Size);
			}
			FileManifestList.emplace_back(file);
		}
	}
}

inline uint32_t ReadUInt32(FILE* fp) {
	uint32_t data;
	fread(&data, sizeof(data), 1, fp);
	return data;
}
inline uint32_t ReadUInt32(std::istream& stream) {
	uint32_t data;
	stream.read((char*)&data, 4);
	return data;
}
inline uint64_t ReadUInt64(std::istream& stream) {
	uint64_t data;
	stream.read((char*)&data, 8);
	return data;
}
inline std::string ReadFString(std::istream& stream) {
	auto size = ReadUInt32(stream);
	std::string ret(size, '\0');
	stream.read(ret.data(), size);
	return ret;
}
template<class T, class ReadT>
inline std::vector<T> ReadContainer(std::istream& stream, ReadT readT) {
	auto size = ReadUInt32(stream);
	std::vector<T> ret;
	ret.reserve(size);
	for (int i = 0; i < size; ++i) {
		ret.emplace_back(readT(stream));
	}
	return ret;
}

inline uint16_t htons(const uint16_t v) {
	return (v >> 8) | (v << 8);
}
inline uint32_t htonl(const uint32_t v) {
	return htons(v >> 16) | (htons((uint16_t)v) << 16);
}

Manifest::Manifest(FILE* fp)
{
	// https://github.com/EpicGames/UnrealEngine/blob/f8f4b403eb682ffc055613c7caf9d2ba5df7f319/Engine/Source/Runtime/Online/BuildPatchServices/Private/Data/ManifestData.cpp#L575
	auto Magic = ReadUInt32(fp);
	if (Magic != 0x44BEC00C) {
		LOG_ERROR("Parsed manifest has invalid magic %08x", Magic);
		return;
	}
	auto HeaderSize = ReadUInt32(fp);
	auto DataSizeUncompressed = ReadUInt32(fp);
	auto DataSizeCompressed = ReadUInt32(fp);
	fseek(fp, 20, SEEK_CUR); // SHAHash, maybe check later, but I can't be bothered
	uint8_t StoredAs;
	fread(&StoredAs, 1, 1, fp);
	auto Version = (EFeatureLevel)ReadUInt32(fp);

	fseek(fp, HeaderSize, SEEK_SET); // make sure ptr is past header

	auto data = std::make_unique<char[]>(DataSizeUncompressed);
	if (StoredAs & 0x01) // compressed
	{
		auto compData = std::make_unique<char[]>(DataSizeCompressed);
		fread(compData.get(), DataSizeCompressed, 1, fp);
		auto decompressor = libdeflate_alloc_decompressor(); // TODO: use ctxmanager for this
		auto result = libdeflate_zlib_decompress(decompressor, compData.get(), DataSizeCompressed, data.get(), DataSizeUncompressed, NULL);
		libdeflate_free_decompressor(decompressor);
	}
	else {
		fread(data.get(), DataSizeUncompressed, 1, fp);
	}

	if (StoredAs & 0x02) // encrypted
	{
		LOG_ERROR("Parsed manifest is encrypted");
		return; // no support yet, i have never seen this used in practice
	}

	std::istringstream manifestData(std::string(data.get(), DataSizeUncompressed));

	fpos_t curPos = manifestData.tellg();
	{ // FManifestMeta
		auto DataSize = ReadUInt32(manifestData);
		uint8_t DataVersion;
		manifestData.read((char*)&DataVersion, 1);

		if (DataVersion >= 0) {
			FeatureLevel = (EFeatureLevel)ReadUInt32(manifestData);
			manifestData.read((char*)&bIsFileData, 1);
			AppID = ReadUInt32(manifestData);
			AppName = ReadFString(manifestData);
			BuildVersion = ReadFString(manifestData);
			LaunchExe = ReadFString(manifestData);
			LaunchCommand = ReadFString(manifestData);
			auto PrereqIds = ReadContainer<std::string>(manifestData, ReadFString);
			auto PrereqName = ReadFString(manifestData);
			auto PrereqPath = ReadFString(manifestData);
			auto PrereqArgs = ReadFString(manifestData);
		}

		if (DataVersion >= 1) {
			auto BuildId = ReadFString(manifestData);
		}

		manifestData.seekg(curPos + DataSize);
	}

	curPos = manifestData.tellg();
	MANIFEST_CHUNK_LOOKUP ChunkManifestLookup; // used to speed up lookups instead of doing a linear search over everything
	{ // FChunkDataList
		auto DataSize = ReadUInt32(manifestData);
		uint8_t DataVersion;
		manifestData.read((char*)&DataVersion, 1);

		auto count = ReadUInt32(manifestData);
		ChunkManifestList.reserve(count);
		ChunkManifestLookup.reserve(count);
		for (int i = 0; i < count; ++i) {
			ChunkManifestList.emplace_back(std::make_shared<Chunk>());
		}

		if (DataVersion >= 0) {
			for (int i = 0; i < count; ++i) {
				manifestData.read(ChunkManifestList[i]->Guid, 16);
				auto ptr = (uint32_t*)ChunkManifestList[i]->Guid;
				*ptr = htonl(*ptr); ++ptr;
				*ptr = htonl(*ptr); ++ptr;
				*ptr = htonl(*ptr); ++ptr;
				*ptr = htonl(*ptr);
				ChunkManifestLookup[ChunkManifestList[i]->Guid] = i;
			}
			for (auto& c : ChunkManifestList) { c->Hash = ReadUInt64(manifestData); }
			for (auto& c : ChunkManifestList) { manifestData.read(c->ShaHash, 20); }
			for (auto& c : ChunkManifestList) { manifestData.read((char*)&c->Group, 1); }
			for (auto& c : ChunkManifestList) { c->WindowSize = ReadUInt32(manifestData); }
			for (auto& c : ChunkManifestList) { c->FileSize = ReadUInt64(manifestData); }
		}

		manifestData.seekg(curPos + DataSize);
	}

	curPos = manifestData.tellg();
	{ // FFileManifestList
		auto DataSize = ReadUInt32(manifestData);
		uint8_t DataVersion;
		manifestData.read((char*)&DataVersion, 1);

		auto count = ReadUInt32(manifestData);
		FileManifestList.resize(count);

		if (DataVersion >= 0) {
			for (auto& f : FileManifestList) { f.FileName = ReadFString(manifestData); }
			for (auto& f : FileManifestList) { ReadFString(manifestData); } // SymlinkTarget
			for (auto& f : FileManifestList) { manifestData.read(f.ShaHash, 20); }
			manifestData.seekg(sizeof(uint8_t) * count, std::ios::cur); // FileMetaFlags
			for (auto& f : FileManifestList) { ReadContainer<std::string>(manifestData, ReadFString); } // InstallTags
			for (auto& f : FileManifestList) {
				f.ChunkParts = ReadContainer<ChunkPart>(manifestData, [this, &ChunkManifestLookup](std::istream& stream) {
					stream.seekg(4, std::ios::cur); // DataSize

					ChunkPart ret;
					char guidBuffer[16];
					stream.read(guidBuffer, 16);
					auto ptr = (uint32_t*)guidBuffer;
					*ptr = htonl(*ptr); ++ptr;
					*ptr = htonl(*ptr); ++ptr;
					*ptr = htonl(*ptr); ++ptr;
					*ptr = htonl(*ptr);
					ret.Chunk = ChunkManifestList[ChunkManifestLookup[guidBuffer]];
					ret.Offset = ReadUInt32(stream);
					ret.Size = ReadUInt32(stream);
					return ret;
				});
			}
		}

		manifestData.seekg(curPos + DataSize);
	}

	curPos = manifestData.tellg();
	{ // FCustomFields
		auto DataSize = ReadUInt32(manifestData);
		uint8_t DataVersion;
		manifestData.read((char*)&DataVersion, 1);

		auto count = ReadUInt32(manifestData);
		std::vector<std::pair<std::string, std::string>> Fields;
		Fields.resize(count);

		if (DataVersion >= 0) {
			for (auto& f : Fields) { f.first = ReadFString(manifestData); }
			for (auto& f : Fields) { f.second = ReadFString(manifestData); }
		}

		manifestData.seekg(curPos + DataSize);

		LOG_DEBUG("%d extra manifest fields:", Fields.size());
		for (auto& f : Fields) {
			LOG_DEBUG("\"%s\": \"%s\"", f.first.c_str(), f.second.c_str());
		}
	}
}

Manifest::~Manifest()
{

}

uint64_t Manifest::GetDownloadSize()
{
	return std::accumulate(ChunkManifestList.begin(), ChunkManifestList.end(), 0ull,
		[](uint64_t sum, const std::shared_ptr<Chunk>& curr) {
			return sum + curr->FileSize;
		});
}

uint64_t Manifest::GetInstallSize()
{
	return std::accumulate(FileManifestList.begin(), FileManifestList.end(), 0ull,
		[](uint64_t sum, File& file) {
			return std::accumulate(file.ChunkParts.begin(), file.ChunkParts.end(), sum,
				[](uint64_t sum, ChunkPart& part) {
					return sum + part.Size;
				});
		});
}
