#pragma once

#define FILE_CONFIG_MAGIC 0xE6219B27
#define FILE_CONFIG_VERSION (uint16_t)SettingsVersion::Latest

#include "../storage/storage.h"

enum class SettingsVersion : uint16_t {
	// Initial Version
	Initial,

	// Removes GameDir and MountDrive
	// Adds CommandArgs
	SimplifyPathsAndCmdLine,

	LatestPlusOne,
	Latest = LatestPlusOne - 1
};

struct SETTINGS {
	char CacheDir[_MAX_PATH + 1];
	uint8_t CompressionMethod;
	uint8_t CompressionLevel;
	bool VerifyCache;
	bool EnableGaming;
	char CommandArgs[1024 + 1];
};

bool SettingsRead(SETTINGS* Settings, FILE* File);
void SettingsWrite(SETTINGS* Settings, FILE* File);

bool SettingsValidate(SETTINGS* Settings);
uint32_t SettingsGetStorageFlags(SETTINGS* Settings);