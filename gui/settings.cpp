#include "settings.h"

template <typename T>
inline T ReadValue(FILE* File) {
	char val[sizeof(T)];
	fread(val, sizeof(T), 1, File);
	return *(T*)val;
}

// pointer should have 1 more byte of space for the \0
template<uint16_t ptrSize>
inline bool ReadString(char(&ptr)[ptrSize], FILE* File) {
	auto stringSize = ntohs(ReadValue<uint16_t>(File));

	if (stringSize > ptrSize - 1) {
		return false;
	}
	fread(ptr, 1, stringSize, File);
	ptr[stringSize] = '\0';
	return true;
}

inline bool ReadVersion(SETTINGS* Settings, FILE* File, SettingsVersion Version) {
	switch (Version)
	{
	case SettingsVersion::Initial:
		ReadString(Settings->CacheDir, File);

		char EmptyBuf[_MAX_PATH + 1];
		ReadString(EmptyBuf, File); // was GameDir

		ReadValue<char>(File); // was MountDrive

		switch (ReadValue<int8_t>(File))
		{
		case 0: // downloaded
		case 3: // explicit zlib
			Settings->CompressionMethod = SettingsCompressionMethod::Zstandard;
			break;
		case 1: // decompressed
			Settings->CompressionMethod = SettingsCompressionMethod::Decompressed;
			break;
		case 2: // lz4
		default:
			Settings->CompressionMethod = SettingsCompressionMethod::LZ4;
			break;
		}

		Settings->CompressionLevel = (SettingsCompressionLevel)ReadValue<int8_t>(File);

		ReadValue<bool>(File); // was VerifyCache
		ReadValue<bool>(File); // was EnableGaming
		return true;
	case SettingsVersion::SimplifyPathsAndCmdLine:
		ReadString(Settings->CacheDir, File);

		switch (ReadValue<int8_t>(File))
		{
		case 0: // downloaded
		case 3: // explicit zlib
			Settings->CompressionMethod = SettingsCompressionMethod::Zstandard;
			break;
		case 1: // decompressed
			Settings->CompressionMethod = SettingsCompressionMethod::Decompressed;
			break;
		case 2: // lz4
		default:
			Settings->CompressionMethod = SettingsCompressionMethod::LZ4;
			break;
		}

		Settings->CompressionLevel = (SettingsCompressionLevel)ReadValue<int8_t>(File);

		ReadValue<bool>(File); // was VerifyCache
		ReadValue<bool>(File); // was EnableGaming

		ReadString(Settings->CommandArgs, File);
		return true;
	case SettingsVersion::Version13:
		ReadString(Settings->CacheDir, File);

		Settings->CompressionMethod = ReadValue<SettingsCompressionMethod>(File);
		Settings->CompressionLevel = ReadValue<SettingsCompressionLevel>(File);
		Settings->UpdateInterval = ReadValue<SettingsUpdateInterval>(File);

		Settings->BufferCount = ReadValue<uint16_t>(File);
		Settings->ThreadCount = ReadValue<uint16_t>(File);

		ReadString(Settings->CommandArgs, File);
		return true;
	default:
		return false;
	}
}

bool SettingsRead(SETTINGS* Settings, FILE* File) {
	rewind(File);

	if (ntohl(ReadValue<uint32_t>(File)) != FILE_CONFIG_MAGIC) {
		return false;
	}

	return ReadVersion(Settings, File, (SettingsVersion)ntohs(ReadValue<uint16_t>(File)));
}

template <typename T>
inline void WriteValue(T val, FILE* File) {
	fwrite(&val, sizeof(T), 1, File);
}

inline void WriteString(char* ptr, FILE* File) {
	uint16_t ptrSize = strlen(ptr);
	WriteValue<uint16_t>(htons(ptrSize), File);
	fwrite(ptr, 1, ptrSize, File);
}

void SettingsWrite(SETTINGS* Settings, FILE* File)
{
	rewind(File);

	WriteValue<uint32_t>(htonl(FILE_CONFIG_MAGIC), File);
	WriteValue<uint16_t>(htons(FILE_CONFIG_VERSION), File);

	WriteString(Settings->CacheDir, File);

	WriteValue<SettingsCompressionMethod>(Settings->CompressionMethod, File);
	WriteValue<SettingsCompressionLevel>(Settings->CompressionLevel, File);
	WriteValue<SettingsUpdateInterval>(Settings->UpdateInterval, File);

	WriteValue<uint16_t>(Settings->BufferCount, File);
	WriteValue<uint16_t>(Settings->ThreadCount, File);

	WriteString(Settings->CommandArgs, File);
}

SETTINGS SettingsDefault() {
	return {
		.CacheDir = "",
		.CompressionMethod = SettingsCompressionMethod::LZ4,
		.CompressionLevel = SettingsCompressionLevel::Slow,
		.UpdateInterval = SettingsUpdateInterval::Minute1,
		.BufferCount = 128,
		.ThreadCount = 64,
		.CommandArgs = "-NOTEXTURESTREAMING -USEALLAVAILABLECORES"
	};
}

bool SettingsValidate(SETTINGS* Settings) {
	if (!fs::is_directory(Settings->CacheDir)) {
		return false;
	}
	return true;
}

std::chrono::milliseconds SettingsGetUpdateInterval(SETTINGS* Settings)
{
	switch (Settings->UpdateInterval)
	{
	case SettingsUpdateInterval::Second1:
		return std::chrono::milliseconds(1 * 1000);
	case SettingsUpdateInterval::Second5:
		return std::chrono::milliseconds(5 * 1000);
	case SettingsUpdateInterval::Second10:
		return std::chrono::milliseconds(10 * 1000);
	case SettingsUpdateInterval::Second30:
		return std::chrono::milliseconds(30 * 1000);
	case SettingsUpdateInterval::Minute1:
		return std::chrono::milliseconds(1 * 60 * 1000);
	case SettingsUpdateInterval::Minute5:
		return std::chrono::milliseconds(5 * 60 * 1000);
	case SettingsUpdateInterval::Minute10:
		return std::chrono::milliseconds(10 * 60 * 1000);
	case SettingsUpdateInterval::Minute30:
		return std::chrono::milliseconds(30 * 60 * 1000);
	case SettingsUpdateInterval::Hour1:
		return std::chrono::milliseconds(60 * 60 * 1000);
	}
}

uint32_t SettingsGetStorageFlags(SETTINGS* Settings) {
    uint32_t StorageFlags = StorageVerifyHashes;
    switch (Settings->CompressionMethod)
    {
	case SettingsCompressionMethod::LZ4:
        StorageFlags |= StorageLZ4;
        break;
	case SettingsCompressionMethod::Zstandard:
        StorageFlags |= StorageZstd;
        break;
	case SettingsCompressionMethod::Decompressed:
        StorageFlags |= StorageDecompressed;
        break;
    }
    switch (Settings->CompressionLevel)
    {
	case SettingsCompressionLevel::Fastest:
        StorageFlags |= StorageCompressFastest;
        break;
	case SettingsCompressionLevel::Fast:
        StorageFlags |= StorageCompressFast;
        break;
	case SettingsCompressionLevel::Normal:
        StorageFlags |= StorageCompressNormal;
        break;
	case SettingsCompressionLevel::Slow:
        StorageFlags |= StorageCompressSlow;
        break;
	case SettingsCompressionLevel::Slowest:
        StorageFlags |= StorageCompressSlowest;
        break;
    }
	return StorageFlags;
}