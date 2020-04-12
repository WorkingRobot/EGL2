#pragma once

#include "../storage/storage.h"

#define FILE_CONFIG_MAGIC 0xE6219B27
#define FILE_CONFIG_VERSION 0

struct SETTINGS {
	char CacheDir[_MAX_PATH + 1];
	char GameDir[_MAX_PATH + 1];
	char MountDrive;
	uint8_t CompressionMethod;
	uint8_t CompressionLevel;
	bool VerifyCache;
	bool EnableGaming;
};

bool SettingsRead(SETTINGS* Settings, FILE* File);
void SettingsWrite(SETTINGS* Settings, FILE* File);

bool SettingsValidate(SETTINGS* Settings);
uint32_t SettingsGetStorageFlags(SETTINGS* Settings);