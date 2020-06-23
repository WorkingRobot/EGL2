#pragma once

// All this stuff was grabbed from scouring the web
// I don't even know where I'd find a copy of the SDK publicly

// The .lib file was from: https://stackoverflow.com/a/16127548

#include <cstdint>

extern "C"
{

typedef uint32_t U32;
typedef uint64_t U64;
typedef int32_t S32;
typedef S32 SINTa; // i think that's what this is?

enum OodleLZ_Compressor {
	OodleLZ_Compressor_Invalid = -1,

	OodleLZ_Compressor_LZH,
	OodleLZ_Compressor_LZHLW,
	OodleLZ_Compressor_LZNIB,
	OodleLZ_Compressor_None,
	OodleLZ_Compressor_LZB16,
	OodleLZ_Compressor_LZBLW,
	OodleLZ_Compressor_LZA,
	OodleLZ_Compressor_LZNA,
	OodleLZ_Compressor_Kraken,
	OodleLZ_Compressor_Mermaid,
	OodleLZ_Compressor_BitKnit,
	OodleLZ_Compressor_Selkie,
	OodleLZ_Compressor_Hydra,
	OodleLZ_Compressor_Leviathan,
};

enum OodleLZ_CompressionLevel {
	OodleLZ_CompressionLevel_HyperFast4 = -4,
	OodleLZ_CompressionLevel_HyperFast3,
	OodleLZ_CompressionLevel_HyperFast2,
	OodleLZ_CompressionLevel_HyperFast1,
	OodleLZ_CompressionLevel_None,
	OodleLZ_CompressionLevel_SuperFast,
	OodleLZ_CompressionLevel_VeryFast,
	OodleLZ_CompressionLevel_Fast,
	OodleLZ_CompressionLevel_Normal,
	OodleLZ_CompressionLevel_Optimal1,
	OodleLZ_CompressionLevel_Optimal2,
	OodleLZ_CompressionLevel_Optimal3,
	OodleLZ_CompressionLevel_Optimal4,
	OodleLZ_CompressionLevel_Optimal5,
	// OodleLZ_CompressionLevel_TooHigh,

	OodleLZ_CompressionLevel_Min = OodleLZ_CompressionLevel_HyperFast4,
	OodleLZ_CompressionLevel_Max = OodleLZ_CompressionLevel_Optimal5
};

enum OodleLZ_FuzzSafe {
	OodleLZ_FuzzSafe_No,
	OodleLZ_FuzzSafe_Yes
};

enum OodleLZ_CheckCRC {
	OodleLZ_CheckCRC_No,
	OodleLZ_CheckCRC_Yes
};

enum OodleLZ_Verbosity {
	OodleLZ_Verbosity_None,
	OodleLZ_Verbosity_Max = 3
};

enum OodleLZ_Decode_ThreadPhase {
	OodleLZ_Decode_ThreadPhase1 = 0x1,
	OodleLZ_Decode_ThreadPhase2 = 0x2,

	OodleLZ_Decode_Unthreaded = OodleLZ_Decode_ThreadPhase1 | OodleLZ_Decode_ThreadPhase2,
};


#define OodleSDKVersion 0x2E080630
#define OodleVersion "2.8.6" // i guess??
#define RADCOPYRIGHT "Oodle " OodleVersion " Copyright (C) 1994-2020, RAD Game Tools, Inc."

#define OODLELZ_BLOCK_LEN 0x40000
#define OODLELZ_LOCALDICTIONARYSIZE_MAX 1<<30
#define OODLELZ_FAILED 0 // i think so?
#define OODLEAPI __stdcall

// opts[5]: dictionarySize limit
// opts[6]: spaceSpeedTradeoffBytes
// opts[12]: jobify
struct OodleLZ_CompressOptions {
	int minMatchLen;
	bool makeLongRangeMatcher;
	int spaceSpeedTradeoffBytes; // [6]
	int seekChunkLen;
	bool seekChunkReset;
	int dictionarySize; // [5]
	int maxLocalDictionarySize;
	int matchTableSizeLog2;
	bool sendQuantumCRCs;
};

struct OodleConfigValues {
	int m_OodleLZ_BackwardsCompatible_MajorVersion;
};

bool OODLEAPI Oodle_CheckVersion(U32 sdk_version, U32* dll_version);

void OODLEAPI Oodle_SetUsageWarnings(bool ignore);

void OODLEAPI Oodle_LogHeader();

void OODLEAPI OodleCore_Plugin_Printf_Default(bool debug, const char* filename, uint32_t line_num, const char* format, ...);

decltype(OodleCore_Plugin_Printf_Default)* OODLEAPI OodleCore_Plugins_SetPrintf(decltype(OodleCore_Plugin_Printf_Default) new_printf);

void OODLEAPI Oodle_GetConfigValues(OodleConfigValues* config);

void OODLEAPI Oodle_SetConfigValues(OodleConfigValues* config);

const OodleLZ_CompressOptions* OODLEAPI OodleLZ_CompressOptions_GetDefault(OodleLZ_Compressor compressor, OodleLZ_CompressionLevel level);

void OODLEAPI OodleLZ_CompressOptions_Validate(OodleLZ_CompressOptions* options);

const char* OODLEAPI OodleLZ_Compressor_GetName(OodleLZ_Compressor compressor);

const char* OODLEAPI OodleLZ_CompressionLevel_GetName(OodleLZ_CompressionLevel level);

SINTa OODLEAPI OodleLZ_GetCompressedBufferSizeNeeded(OodleLZ_Compressor compressor, SINTa size);

SINTa OODLEAPI OodleLZDecoder_MemorySizeNeeded(OodleLZ_Compressor compressor, SINTa size);

SINTa OODLEAPI OodleLZ_Compress(OodleLZ_Compressor compressor, const char* src, SINTa src_size, char* dst, OodleLZ_CompressionLevel level, OodleLZ_CompressOptions* opts, const char* context, void* unused, void* scratch, SINTa scratch_size);

SINTa OODLEAPI OodleLZ_Decompress(const char* src, SINTa src_size, char* dst, SINTa dst_size, OodleLZ_FuzzSafe fuzz, OodleLZ_CheckCRC crc, OodleLZ_Verbosity verbosity, char* context, U64 e, void* callback, void* callback_ctx, void* scratch, SINTa scratch_size);

}