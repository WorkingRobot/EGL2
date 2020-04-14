#pragma once

#include <memory>
#include <filesystem>
namespace fs = std::filesystem;

// Grabbing and parsing manifest

typedef struct _MANIFEST MANIFEST;
typedef struct _MANIFEST_FILE MANIFEST_FILE;
typedef struct _MANIFEST_CHUNK MANIFEST_CHUNK;
typedef struct _MANIFEST_CHUNK_PART MANIFEST_CHUNK_PART;

uint64_t ManifestDownloadSize(MANIFEST* Manifest);
uint64_t ManifestInstallSize(MANIFEST* Manifest);
void ManifestGetFiles(MANIFEST* Manifest, MANIFEST_FILE** PFileList, uint32_t* PFileCount, uint16_t* PStrideSize);
void ManifestGetChunks(MANIFEST* Manifest, std::shared_ptr<MANIFEST_CHUNK>** PChunkList, uint32_t* PChunkCount);
MANIFEST_FILE* ManifestGetFile(MANIFEST* Manifest, const char* Filename);
void ManifestGetCloudDir(MANIFEST* Manifest, char* CloudDirHostBuffer, char* CloudDirPathBuffer);
void ManifestGetLaunchInfo(MANIFEST* Manifest, char* ExeBuffer, char* CommandBuffer);
void ManifestDelete(MANIFEST* Manifest);

// Authentication

typedef struct _MANIFEST_AUTH MANIFEST_AUTH;
bool ManifestAuthGrab(MANIFEST_AUTH** PManifestAuth);
void ManifestAuthDelete(MANIFEST_AUTH* ManifestAuth);
bool ManifestAuthGetManifest(MANIFEST_AUTH* ManifestAuth, fs::path CachePath, MANIFEST** PManifest);

// Files

void ManifestFileGetName(MANIFEST_FILE* File, char* FilenameBuffer);
void ManifestFileGetChunks(MANIFEST_FILE* File, MANIFEST_CHUNK_PART** PChunkPartList, uint32_t* PChunkPartCount, uint16_t* PStrideSize);
bool ManifestFileGetChunkIndex(MANIFEST_FILE* File, uint64_t Offset, uint32_t* ChunkIndex, uint32_t* ChunkOffset);
uint64_t ManifestFileGetFileSize(MANIFEST_FILE* File);
char* ManifestFileGetSha1(MANIFEST_FILE* File);

// File Chunks

MANIFEST_CHUNK* ManifestFileChunkGetChunk(MANIFEST_CHUNK_PART* ChunkPart);
void ManifestFileChunkGetData(MANIFEST_CHUNK_PART* ChunkPart, uint32_t* POffset, uint32_t* PSize);

// Chunks

char* ManifestChunkGetGuid(MANIFEST_CHUNK* Chunk);
char* ManifestChunkGetSha1(MANIFEST_CHUNK* Chunk);
void ManifestChunkAppendUrl(MANIFEST_CHUNK* Chunk, char* UrlBuffer);
