#include "settings.h"

#include <winsock.h>

template <typename T>
inline T ReadValue(FILE* File) {
	char val[sizeof(T)];
	fread(val, sizeof(T), 1, File);
	return *(T*)val;
}

// ptrLength does not include the \0 at the end (ptrLength of 0 still includes \0)
inline bool ReadString(char* ptr, uint16_t ptrLength, FILE* File) {
	auto stringSize = ntohs(ReadValue<uint16_t>(File));
	if (stringSize > ptrLength) {
		return false;
	}
	fread(ptr, 1, stringSize, File);
	ptr[stringSize] = '\0';
	return true;
}

bool SettingsRead(SETTINGS* Settings, FILE* File) {
	rewind(File);

	if (ntohl(ReadValue<uint32_t>(File)) != FILE_CONFIG_MAGIC) {
		return false;
	}
	if (ntohs(ReadValue<uint16_t>(File)) != FILE_CONFIG_VERSION) {
		return false;
	}

	ReadString(Settings->CacheDir, _MAX_PATH, File);
	ReadString(Settings->GameDir, _MAX_PATH, File);

	Settings->MountDrive = ReadValue<char>(File);

	Settings->CompressionMethod = ReadValue<int8_t>(File);
	Settings->CompressionLevel = ReadValue<int8_t>(File);

	Settings->VerifyCache = ReadValue<bool>(File);
	Settings->EnableGaming = ReadValue<bool>(File);

	return true;
}

template <typename T>
inline void WriteValue(T val, FILE* File) {
	fwrite(&val, sizeof(T), 1, File);
}

inline void WriteString(char* ptr, int ptrLength, FILE* File) {
	WriteValue<uint16_t>(htons(ptrLength), File);
	fwrite(ptr, 1, ptrLength, File);
}

void SettingsWrite(SETTINGS* Settings, FILE* File)
{
	rewind(File);

	WriteValue<uint32_t>(htonl(FILE_CONFIG_MAGIC), File);
	WriteValue<uint16_t>(htons(FILE_CONFIG_VERSION), File);

	WriteString(Settings->CacheDir, strlen(Settings->CacheDir), File);
	WriteString(Settings->GameDir, strlen(Settings->GameDir), File);

	WriteValue<char>(Settings->MountDrive, File);

	WriteValue<int8_t>(Settings->CompressionMethod, File);
	WriteValue<int8_t>(Settings->CompressionLevel, File);

	WriteValue<bool>(Settings->VerifyCache, File);
	WriteValue<bool>(Settings->EnableGaming, File);
}

bool SettingsValidate(SETTINGS* Settings) {
	if (!fs::is_directory(Settings->CacheDir)) {
		return false;
	}
	if (Settings->EnableGaming && !fs::is_directory(Settings->GameDir)) {
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