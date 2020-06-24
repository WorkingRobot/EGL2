#pragma once

#define FILE_CONFIG_MAGIC 0xE6219B27
#define FILE_CONFIG_VERSION (uint16_t)SettingsVersion::Version13

#include "../storage/storage.h"

enum class SettingsVersion : uint16_t {
	// Initial Version
	Initial,

	// Removes GameDir and MountDrive
	// Adds CommandArgs
	SimplifyPathsAndCmdLine,

	// Adds ThreadCount, BufferCount, and UpdateInterval
	// Removes VerifyCache and EnableGaming
	Version13,

	Oodle,

	LatestPlusOne,
	Latest = LatestPlusOne - 1
};

enum class SettingsCompressionMethod : uint8_t {
	Decompressed,
	Zstandard,
	LZ4,
	OodleSelkie
};

enum class SettingsCompressionLevel : uint8_t {
	Fastest,
	Fast,
	Normal,
	Slow,
	Slowest
};

enum class SettingsUpdateInterval : uint8_t {
	Second1,
	Second5,
	Second10,
	Second30,
	Minute1,
	Minute5,
	Minute10,
	Minute30,
	Hour1
};

struct SETTINGS {
	char CacheDir[_MAX_PATH + 1];
	SettingsCompressionMethod CompressionMethod;
	SettingsCompressionLevel CompressionLevel;
	SettingsUpdateInterval UpdateInterval;

	// Advanced
	uint16_t BufferCount;
	uint16_t ThreadCount;
	char CommandArgs[1024 + 1];
};

bool SettingsRead(SETTINGS* Settings, FILE* File);
void SettingsWrite(SETTINGS* Settings, FILE* File);

SETTINGS SettingsDefault();
bool SettingsValidate(SETTINGS* Settings);
std::chrono::milliseconds SettingsGetUpdateInterval(SETTINGS* Settings);
uint32_t SettingsGetStorageFlags(SETTINGS* Settings);