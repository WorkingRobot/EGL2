#include "settings.h"

#include <winsock.h>

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

		Settings->CompressionMethod = ReadValue<int8_t>(File);
		Settings->CompressionLevel = ReadValue<int8_t>(File);

		Settings->VerifyCache = ReadValue<bool>(File);
		Settings->EnableGaming = ReadValue<bool>(File);

		strcpy(Settings->CommandArgs, "");
		return true;
	case SettingsVersion::SimplifyPathsAndCmdLine:
		ReadString(Settings->CacheDir, File);

		Settings->CompressionMethod = ReadValue<int8_t>(File);
		Settings->CompressionLevel = ReadValue<int8_t>(File);

		Settings->VerifyCache = ReadValue<bool>(File);
		Settings->EnableGaming = ReadValue<bool>(File);

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

	WriteValue<int8_t>(Settings->CompressionMethod, File);
	WriteValue<int8_t>(Settings->CompressionLevel, File);

	WriteValue<bool>(Settings->VerifyCache, File);
	WriteValue<bool>(Settings->EnableGaming, File);

	WriteString(Settings->CommandArgs, File);
}

bool SettingsValidate(SETTINGS* Settings) {
	if (!fs::is_directory(Settings->CacheDir)) {
		return false;
	}
	return true;
}

uint32_t SettingsGetStorageFlags(SETTINGS* Settings) {
    uint32_t StorageFlags = 0;
    if (Settings->VerifyCache) {
        StorageFlags |= StorageVerifyHashes;
    }
    switch (Settings->CompressionMethod)
    {
    case 0:
        StorageFlags |= StorageCompressed;
        break;
    case 1:
        StorageFlags |= StorageDecompressed;
        break;
    case 2:
        StorageFlags |= StorageCompressLZ4;
        break;
    case 3:
        StorageFlags |= StorageCompressZlib;
        break;
    }
    switch (Settings->CompressionLevel)
    {
    case 0:
        StorageFlags |= StorageCompressFastest;
        break;
    case 1:
        StorageFlags |= StorageCompressFast;
        break;
    case 2:
        StorageFlags |= StorageCompressNormal;
        break;
    case 3:
        StorageFlags |= StorageCompressSlow;
        break;
    case 4:
        StorageFlags |= StorageCompressSlowest;
        break;
    }
	return StorageFlags;
}