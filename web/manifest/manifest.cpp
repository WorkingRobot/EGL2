#include "manifest.h"

#include <numeric>
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
			HashToBytes(sizeItr->value.GetString(), (char*)&chunk->Size);
			HexToBytes(shaItr->value.GetString(), (char*)&chunk->ShaHash);
			chunk->Group = atoi(groupItr->value.GetString());

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

Manifest::~Manifest()
{
}

uint64_t Manifest::GetDownloadSize()
{
	return std::accumulate(ChunkManifestList.begin(), ChunkManifestList.end(), 0ull,
		[](uint64_t sum, const std::shared_ptr<Chunk>& curr) {
			return sum + curr->Size;
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
