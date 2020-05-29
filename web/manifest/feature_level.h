#pragma once

#include <cstdint>

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