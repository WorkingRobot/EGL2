#include "manifest.h"
#include "http.h"
#include <rapidjson/document.h>

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <numeric>
#include <inttypes.h>

namespace fs = std::filesystem;

typedef struct _MANIFEST_CHUNK {
	char Guid[16];
	uint64_t Hash;
	char ShaHash[20];
	uint8_t Group;
	uint64_t Size;
} MANIFEST_CHUNK;

typedef struct _MANIFEST_CHUNK_PART {
	std::shared_ptr<MANIFEST_CHUNK> Chunk;
	uint32_t Offset;
	uint32_t Size;
} MANIFEST_CHUNK_PART;

typedef std::unique_ptr<MANIFEST_CHUNK_PART[]> MANIFEST_CHUNK_PARTS;
typedef struct _MANIFEST_FILE {
	char FileName[128];
	char ShaHash[20];
	MANIFEST_CHUNK_PARTS ChunkParts;
	uint32_t ChunkPartCount;
} MANIFEST_FILE;

enum class EFeatureLevel : int32_t
{
	// The original version.
	Original = 0,
	// Support for custom fields.
	CustomFields,
	// Started storing the version number.
	StartStoringVersion,
	// Made after data files where renamed to include the hash value, these chunks now go to ChunksV2.
	DataFileRenames,
	// Manifest stores whether build was constructed with chunk or file data.
	StoresIfChunkOrFileData,
	// Manifest stores group number for each chunk/file data for reference so that external readers don't need to know how to calculate them.
	StoresDataGroupNumbers,
	// Added support for chunk compression, these chunks now go to ChunksV3. NB: Not File Data Compression yet.
	ChunkCompressionSupport,
	// Manifest stores product prerequisites info.
	StoresPrerequisitesInfo,
	// Manifest stores chunk download sizes.
	StoresChunkFileSizes,
	// Manifest can optionally be stored using UObject serialization and compressed.
	StoredAsCompressedUClass,
	// These two features were removed and never used.
	UNUSED_0,
	UNUSED_1,
	// Manifest stores chunk data SHA1 hash to use in place of data compare, for faster generation.
	StoresChunkDataShaHashes,
	// Manifest stores Prerequisite Ids.
	StoresPrerequisiteIds,
	// The first minimal binary format was added. UObject classes will no longer be saved out when binary selected.
	StoredAsBinaryData,
	// Temporary level where manifest can reference chunks with dynamic window size, but did not serialize them. Chunks from here onwards are stored in ChunksV4.
	VariableSizeChunksWithoutWindowSizeChunkInfo,
	// Manifest can reference chunks with dynamic window size, and also serializes them.
	VariableSizeChunks,
	// Manifest stores a unique build id for exact matching of build data.
	StoresUniqueBuildId,

	// !! Always after the latest version entry, signifies the latest version plus 1 to allow the following Latest alias.
	LatestPlusOne,
	// An alias for the actual latest version value.
	Latest = (LatestPlusOne - 1),
	// An alias to provide the latest version of a manifest supported by file data (nochunks).
	LatestNoChunks = StoresChunkFileSizes,
	// An alias to provide the latest version of a manifest supported by a json serialized format.
	LatestJson = StoresPrerequisiteIds,
	// An alias to provide the first available version of optimised delta manifest saving.
	FirstOptimisedDelta = StoresUniqueBuildId,

	// JSON manifests were stored with a version of 255 during a certain CL range due to a bug.
	// We will treat this as being StoresChunkFileSizes in code.
	BrokenJsonVersion = 255,
	// This is for UObject default, so that we always serialize it.
	Invalid = -1
};

typedef std::unique_ptr<MANIFEST_FILE[]> MANIFEST_FILE_LIST;
typedef std::unique_ptr<std::shared_ptr<MANIFEST_CHUNK>[]> MANIFEST_CHUNK_LIST;
auto hash = [](const char* n) { return (*((uint64_t*)n)) ^ (*(((uint64_t*)n) + 1)); };
auto equal = [](const char* a, const char* b) {return !memcmp(a, b, 16); };
typedef std::unordered_map<char*, uint32_t, decltype(hash), decltype(equal)> MANIFEST_CHUNK_LOOKUP;
typedef struct _MANIFEST {
	EFeatureLevel FeatureLevel;
	bool bIsFileData;
	uint32_t AppID;
	std::string AppName;
	std::string BuildVersion;
	std::string LaunchExe;
	std::string LaunchCommand;
	//std::set<std::string> PrereqIds;
	//std::string PrereqName;
	//std::string PrereqPath;
	//std::string PrereqArgs;
	MANIFEST_FILE_LIST FileManifestList;
	uint32_t FileCount;
	MANIFEST_CHUNK_LIST ChunkManifestList;
	uint32_t ChunkCount;

	std::string CloudDirHost;
	std::string CloudDirPath;
} MANIFEST;

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

inline int HexToBytes(const char* hex, char* output) {
	int NumBytes = 0;
	while (*hex)
	{
		output[NumBytes] = GetByteValue(*hex++) << 4;
		output[NumBytes] += GetByteValue(*hex++);
		++NumBytes;
	}
	return NumBytes;
}

inline void urlencode(const const char* s, std::ostringstream& e)
{
	static const char lookup[] = "0123456789abcdef";
	for (int i = 0, ix = strlen(s); i < ix; i++)
	{
		const char& c = s[i];
		if ((48 <= c && c <= 57) ||//0-9
			(65 <= c && c <= 90) ||//abc...xyz
			(97 <= c && c <= 122) || //ABC...XYZ
			(c == '-' || c == '_' || c == '.' || c == '~')
			)
		{
			e << c;
		}
		else
		{
			e << '%';
			e << lookup[(c & 0xF0) >> 4];
			e << lookup[(c & 0x0F)];
		}
	}
}

inline void GetDownloadFile(rapidjson::Value& manifest, std::string* id) {
	auto uriStr = std::string(manifest["uri"].GetString());
	*id = std::string(uriStr.begin() + uriStr.find_last_of('/') + 1, uriStr.end());
}

inline void GetDownloadUrl(rapidjson::Value& manifest, std::string* host, std::string* uri) {
	rapidjson::Value& uri_val = manifest["uri"];
	Uri url_v = Uri::Parse(uri_val.GetString());
	*host = url_v.Host;
	if (!manifest.HasMember("queryParams")) {
		*uri = url_v.Path;
	}
	else {
		rapidjson::Value& queryParams = manifest["queryParams"];
		std::ostringstream oss;
		oss << url_v.Path << "?";
		for (auto& itr : queryParams.GetArray()) {
			urlencode(itr["name"].GetString(), oss);
			oss << "=";
			urlencode(itr["value"].GetString(), oss);
			oss << "&";
		}
		oss.seekp(-1, std::ios_base::end); // remove last &
		oss << '\0';
		*uri = oss.str();
	}
}

inline int random(int min, int max) //range : [min, max)
{
	static bool first = true;
	if (first)
	{
		srand(time(NULL)); //seeding for the first time only!
		first = false;
	}
	return min + rand() % ((max + 1) - min);
}

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

bool ManifestGrab(const char* elementsResponse, fs::path CacheFolder, MANIFEST** PManifest) {
	std::string host, url;
	fs::path cachePath;
	{
		rapidjson::Document elements;
		elements.Parse(elementsResponse);
		if (elements.HasParseError()) {
			printf("%d %zu\n", elements.GetParseError(), elements.GetErrorOffset());
		}
		rapidjson::Value& v = elements["elements"].GetArray()[0];
		
		//rapidjson::Value& hash = v["hash"]; (hash is unused atm)
		//char* ManifestHash = new char[hash.GetStringLength() / 2];
		//HexToBytes(hash.GetString(), ManifestHash);

		rapidjson::Value& manifest = v["manifests"][random(0, v["manifests"].GetArray().Size() - 1)];
		GetDownloadUrl(manifest, &host, &url);
		std::string id;
		GetDownloadFile(manifest, &id);
		if (!CacheFolder.empty()) {
			cachePath = CacheFolder / id;
		}
	}
	httplib::Client c(host);
	
	rapidjson::Document manifestDoc;
	printf("getting manifest\n");
	if (!CacheFolder.empty() && fs::status(cachePath).type() == fs::file_type::regular) {
		auto fp = fopen(cachePath.string().c_str(), "rb");
		fseek(fp, 0, SEEK_END);
		long manifestSize = ftell(fp);
		rewind(fp);
		auto manifestStr = new char[manifestSize + 1];
		fread(manifestStr, 1, manifestSize, fp);
		fclose(fp);
		manifestStr[manifestSize] = '\0';
		manifestDoc.Parse(manifestStr);
		delete[] manifestStr;
	}
	else {
		bool manifestStrReserved = false;
		std::vector<char> manifestStr;
		manifestStr.reserve(8192);
		c.Get(url.c_str(),
			[&](const char* data, uint64_t data_length) {
				manifestStr.insert(manifestStr.end(), data, data + data_length);
				return true;
			},
			[&](uint64_t len, uint64_t total) {
				if (!manifestStrReserved) {
					manifestStr.reserve(total);
					printf("reserved for %llu\n", total);
					manifestStrReserved = true;
				}
				static int n = 0;
				if (!(n++ % 400)) {
					printf("\r%lld / %lld bytes => %.1f%% complete",
						len, total,
						float(len * 100) / total);
				}
				return true;
			});

		if (!CacheFolder.empty()) {
			auto fp = fopen(cachePath.string().c_str(), "wb");
			fwrite(manifestStr.data(), 1, manifestStr.size(), fp);
			fclose(fp);
		}

		manifestStr.push_back('\0');
		manifestDoc.Parse(manifestStr.data());
	}
	printf("\n");
	printf("parsing\n");
	printf("parsed\n");

	if (manifestDoc.HasParseError()) {
		printf("JSON Parse Error %d @ %zu\n", manifestDoc.GetParseError(), manifestDoc.GetErrorOffset());
	}

	MANIFEST* Manifest = new MANIFEST();
	HashToBytes(manifestDoc["ManifestFileVersion"].GetString(), (char*)&Manifest->FeatureLevel);
	Manifest->bIsFileData = manifestDoc["bIsFileData"].GetBool();
	HashToBytes(manifestDoc["AppID"].GetString(), (char*)&Manifest->AppID);
	Manifest->AppName = manifestDoc["AppNameString"].GetString();
	Manifest->BuildVersion = manifestDoc["BuildVersionString"].GetString();
	Manifest->LaunchExe = manifestDoc["LaunchExeString"].GetString();
	Manifest->LaunchCommand = manifestDoc["LaunchCommand"].GetString();

	Manifest->CloudDirHost = host;
#define CHUNK_DIR(dir) "/Chunks" dir "/"
	const char* ChunksDir;
	if (Manifest->FeatureLevel < EFeatureLevel::DataFileRenames) {
		ChunksDir = CHUNK_DIR("");
	}
	else if (Manifest->FeatureLevel < EFeatureLevel::ChunkCompressionSupport) {
		ChunksDir = CHUNK_DIR("V2");
	}
	else if (Manifest->FeatureLevel < EFeatureLevel::VariableSizeChunksWithoutWindowSizeChunkInfo) {
		ChunksDir = CHUNK_DIR("V3");
	}
	else {
		ChunksDir = CHUNK_DIR("V4");
	}
#undef CHUNK_DIR
	Manifest->CloudDirPath = url.substr(0, url.find_last_of('/')) + ChunksDir;

	MANIFEST_CHUNK_LOOKUP ChunkManifestLookup; // used to speed up lookups instead of doing a linear search over everything
	{
		rapidjson::Value& HashList = manifestDoc["ChunkHashList"];
		rapidjson::Value& ShaList = manifestDoc["ChunkShaList"];
		rapidjson::Value& GroupList = manifestDoc["DataGroupList"];
		rapidjson::Value& SizeList = manifestDoc["ChunkFilesizeList"];

		Manifest->ChunkCount = HashList.MemberCount();
		Manifest->ChunkManifestList = std::make_unique<std::shared_ptr<MANIFEST_CHUNK>[]>(Manifest->ChunkCount);
		ChunkManifestLookup.reserve(Manifest->ChunkCount);

		printf("%d chunks\n", Manifest->ChunkCount);
		int i = 0;
		for (rapidjson::Value::ConstMemberIterator hashItr = HashList.MemberBegin(), shaItr = ShaList.MemberBegin(), groupItr = GroupList.MemberBegin(), sizeItr = SizeList.MemberBegin();
			i != Manifest->ChunkCount; ++i, ++hashItr, ++shaItr, ++groupItr, ++sizeItr)
		{
			auto chunk = std::make_shared<MANIFEST_CHUNK>();
			HexToBytes(hashItr->name.GetString(), chunk->Guid);
			HashToBytes(hashItr->value.GetString(), (char*)&chunk->Hash);
			HashToBytes(sizeItr->value.GetString(), (char*)&chunk->Size);
			HexToBytes(shaItr->value.GetString(), (char*)&chunk->ShaHash);
			chunk->Group = atoi(groupItr->value.GetString());

			Manifest->ChunkManifestList[i] = chunk;
			ChunkManifestLookup[chunk->Guid] = i;
		}
	}

	{
		rapidjson::Value& FileList = manifestDoc["FileManifestList"];
		Manifest->FileCount = FileList.Size();
		Manifest->FileManifestList = std::make_unique<MANIFEST_FILE[]>(Manifest->FileCount);

		int i = 0;
		for (auto& fileManifest : FileList.GetArray()) {
			auto& file = Manifest->FileManifestList[i++];
			strcpy(file.FileName, fileManifest["Filename"].GetString());
			HashToBytes(fileManifest["FileHash"].GetString(), (char*)&file.ShaHash);
			file.ChunkPartCount = fileManifest["FileChunkParts"].Size();
			file.ChunkParts = std::make_unique<MANIFEST_CHUNK_PART[]>(file.ChunkPartCount);

			int j = 0;
			for (auto& fileChunk : fileManifest["FileChunkParts"].GetArray()) {
				MANIFEST_CHUNK_PART part;
				char guidBuffer[16];
				HexToBytes(fileChunk["Guid"].GetString(), guidBuffer);
				part.Chunk = Manifest->ChunkManifestList[ChunkManifestLookup[guidBuffer]];
				HashToBytes(fileChunk["Offset"].GetString(), (char*)&part.Offset);
				HashToBytes(fileChunk["Size"].GetString(), (char*)&part.Size);
				file.ChunkParts[j++] = part;
			}
		}
	}

	*PManifest = Manifest;
	return true;
}

uint64_t ManifestDownloadSize(MANIFEST* Manifest) {
	return std::accumulate(Manifest->ChunkManifestList.get(), Manifest->ChunkManifestList.get() + Manifest->ChunkCount, 0ull,
		[](uint64_t sum, const std::shared_ptr<MANIFEST_CHUNK>& curr) {
			return sum + curr->Size;
		});
}

uint64_t ManifestInstallSize(MANIFEST* Manifest) {
	return std::accumulate(Manifest->FileManifestList.get(), Manifest->FileManifestList.get() + Manifest->FileCount, 0ull,
		[](uint64_t sum, MANIFEST_FILE& file) {
			return std::accumulate(file.ChunkParts.get(), file.ChunkParts.get() + file.ChunkPartCount, sum,
				[](uint64_t sum, MANIFEST_CHUNK_PART& part) {
					return sum + part.Size;
				});
		});
}

void ManifestGetFiles(MANIFEST* Manifest, MANIFEST_FILE** PFileList, uint32_t* PFileCount, uint16_t* PStrideSize) {
	*PFileList = Manifest->FileManifestList.get();
	*PFileCount = Manifest->FileCount;
	*PStrideSize = sizeof(MANIFEST_FILE);
}

void ManifestGetChunks(MANIFEST* Manifest, std::shared_ptr<MANIFEST_CHUNK>** PChunkList, uint32_t* PChunkCount) {
	*PChunkList = Manifest->ChunkManifestList.get();
	*PChunkCount = Manifest->ChunkCount;
}

MANIFEST_FILE* ManifestGetFile(MANIFEST* Manifest, const char* Filename) {
	for (int i = 0; i < Manifest->FileCount; ++i) {
		if (!strcmp(Filename, Manifest->FileManifestList[i].FileName)) {
			return &Manifest->FileManifestList[i];
		}
	}
	return nullptr;
}

void ManifestGetCloudDir(MANIFEST* Manifest, char* CloudDirHostBuffer, char* CloudDirPathBuffer) {
	strcpy(CloudDirHostBuffer, Manifest->CloudDirHost.c_str());
	strcpy(CloudDirPathBuffer, Manifest->CloudDirPath.c_str());
}

void ManifestGetLaunchInfo(MANIFEST* Manifest, char* ExeBuffer, char* CommandBuffer) {
	strcpy(ExeBuffer, Manifest->LaunchExe.c_str());
	strcpy(CommandBuffer, Manifest->LaunchCommand.c_str());
}

void ManifestDelete(MANIFEST* Manifest) {
	delete Manifest;
}

typedef struct _MANIFEST_AUTH {
	std::string AccessToken;
	time_t ExpiresAt;
} MANIFEST_AUTH;

inline int ParseInt(const char* value)
{
	return std::strtol(value, nullptr, 10);
}

void UpdateManifest(MANIFEST_AUTH* Auth) {
	httplib::SSLClient client("account-public-service-prod03.ol.epicgames.com");
	static const httplib::Headers headers = {
		{ "Authorization", "basic MzhkYmZjMzE5NjAyNGQ1OTgwMzg2YTM3YjdjNzkyYmI6YTYyODBiODctZTQ1ZS00MDliLTk2ODEtOGYxNWViN2RiY2Y1" }
	};
	static const httplib::Params params = {
		{ "grant_type", "client_credentials" }
	};
	rapidjson::Document d;
	d.Parse(client.Post("/account/api/oauth/token", headers, params)->body.c_str());

	rapidjson::Value& token = d["access_token"];
	Auth->AccessToken = token.GetString();

	rapidjson::Value& expires_at = d["expires_at"];
	auto expires_str = expires_at.GetString();
	constexpr const size_t expectedLength = sizeof("YYYY-MM-DDTHH:MM:SSZ") - 1;
	static_assert(expectedLength == 20, "Unexpected ISO 8601 date/time length");

	if (expires_at.GetStringLength() < expectedLength)
	{
		return;
	}

	std::tm time = { 0 };
	time.tm_year = ParseInt(&expires_str[0]) - 1900;
	time.tm_mon = ParseInt(&expires_str[5]) - 1;
	time.tm_mday = ParseInt(&expires_str[8]);
	time.tm_hour = ParseInt(&expires_str[11]);
	time.tm_min = ParseInt(&expires_str[14]);
	time.tm_sec = ParseInt(&expires_str[17]);
	time.tm_isdst = 0;
	const int millis = expires_at.GetStringLength() > 20 ? ParseInt(&expires_str[20]) : 0;
	Auth->ExpiresAt = std::mktime(&time) * 1000 + millis;
}

bool ManifestAuthGrab(MANIFEST_AUTH** PManifestAuth) {
	MANIFEST_AUTH* Auth = new MANIFEST_AUTH;
	UpdateManifest(Auth);
	*PManifestAuth = Auth;
	return true;
}

void ManifestAuthDelete(MANIFEST_AUTH* ManifestAuth) {
	delete ManifestAuth;
}

bool ManifestAuthGetManifest(MANIFEST_AUTH* ManifestAuth, fs::path CachePath, MANIFEST** PManifest) {
	if (ManifestAuth->ExpiresAt < time(NULL)) {
		UpdateManifest(ManifestAuth);
	}

	httplib::SSLClient client("launcher-public-service-prod-m.ol.epicgames.com");
	char* authHeader = new char[7 + ManifestAuth->AccessToken.size() + 1];
	sprintf(authHeader, "bearer %s", ManifestAuth->AccessToken.c_str());

	httplib::Headers headers = {
		{ "Authorization", authHeader  }
	};
	ManifestGrab(client.Post("/launcher/api/public/assets/v2/platform/Windows/catalogItem/4fe75bbc5a674f4f9b356b5c90567da5/app/Fortnite/label/Live/", headers, "", "application/json")->body.c_str(), CachePath, PManifest);
	delete[] authHeader;
	return true;
}

void ManifestFileGetName(MANIFEST_FILE* File, char* FilenameBuffer) {
	strcpy(FilenameBuffer, File->FileName);
}

void ManifestFileGetChunks(MANIFEST_FILE* File, MANIFEST_CHUNK_PART** PChunkPartList, uint32_t* PChunkPartCount, uint16_t* PStrideSize) {
	*PChunkPartList = File->ChunkParts.get();
	*PChunkPartCount = File->ChunkPartCount;
	*PStrideSize = sizeof(MANIFEST_CHUNK_PART);
}

bool ManifestFileGetChunkIndex(MANIFEST_FILE* File, uint64_t Offset, uint32_t* ChunkIndex, uint32_t* ChunkOffset) {
	for (int i = 0; i < File->ChunkPartCount; ++i) {
		if (Offset < File->ChunkParts[i].Size) {
			*ChunkIndex = i;
			*ChunkOffset = Offset;
			return true;
		}
		Offset -= File->ChunkParts[i].Size;
	}
	return false;
}

uint64_t ManifestFileGetFileSize(MANIFEST_FILE* File) {
	return std::accumulate(File->ChunkParts.get(), File->ChunkParts.get() + File->ChunkPartCount, 0ull,
		[](uint64_t sum, const MANIFEST_CHUNK_PART& curr) {
			return sum + curr.Size;
		});
}

char* ManifestFileGetSha1(MANIFEST_FILE* File) {
	return File->ShaHash;
}

MANIFEST_CHUNK* ManifestFileChunkGetChunk(MANIFEST_CHUNK_PART* ChunkPart) {
	return ChunkPart->Chunk.get();
}

void ManifestFileChunkGetData(MANIFEST_CHUNK_PART* ChunkPart, uint32_t* POffset, uint32_t* PSize) {
	*POffset = ChunkPart->Offset;
	*PSize = ChunkPart->Size;
}

char* ManifestChunkGetGuid(MANIFEST_CHUNK* Chunk) {
	return Chunk->Guid;
}

char* ManifestChunkGetSha1(MANIFEST_CHUNK* Chunk) {
	return Chunk->ShaHash;
}

// Example input UrlBuffer: https://epicgames-download1.akamaized.net/Builds/Fortnite/CloudDir/ChunksV3/
// Make sure UrlBuffer has some extra space in front as well
void ManifestChunkAppendUrl(MANIFEST_CHUNK* Chunk, char* UrlBuffer) {
	sprintf(UrlBuffer, "%s%02d/%016llX_%016llX%016llX.chunk", UrlBuffer, Chunk->Group, Chunk->Hash, ntohll(*(uint64_t*)Chunk->Guid), ntohll(*(uint64_t*)(Chunk->Guid + 8)));
}