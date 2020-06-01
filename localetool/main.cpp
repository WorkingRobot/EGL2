/*
Format:

STRING FORMAT:
uint16_t prefixed length (in bytes, not characters)
string data (utf8?)

FILE FORMAT:
just a bunch of strings back to back, parser knows the locale of the string and how many are per locale

*/

#define LOCTEXT_FOLDER "../../../localetool/locales/"
#define LOCDATA_FILE   "../../../locales.dat"

#include "../gui/Localization.h"

#include <codecvt>
#include <fstream>
#include <locale>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <sstream>
#include <string>
#include <vector>
#include <zstd.h>

std::wstring strconv(const char* utf8str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> wconv;
	return wconv.from_bytes(utf8str);
}

// length in characters, not bytes
inline void WriteString(std::ostringstream& ostr, const wchar_t* data, uint16_t length) {
	ostr.write((char*)&length, 2);
	ostr.write((char*)data, length * 2);
}

#define LS(name) #name,
static constexpr const char* jsonKeys[] = { LOCALESTRINGS };
#undef LS

inline void WriteLocale(std::ostringstream& ostr, FILE* localePtr) {
	char readBuffer[8192];
	rapidjson::FileReadStream is(localePtr, readBuffer, sizeof(readBuffer));
	rapidjson::Document d;
	d.ParseStream(is);
	if (d.HasParseError()) {
		printf("COULD NOT PARSE: JSON Parse Error %d @ %zu", d.GetParseError(), d.GetErrorOffset());
		return;
	}

	for (int i = 0; i < (int)LocaleString::Count; ++i) {
		if (!d.HasMember(jsonKeys[i])) {
			printf("DOES NOT HAVE %s\n", jsonKeys[i]);
		}
		auto& v = d[jsonKeys[i]];
		auto val = strconv(v.GetString());
		WriteString(ostr, val.c_str(), val.size());
	}
}

inline void WriteLocale(const char* filename, const char* lang, std::ostringstream& ostr) {
	auto inpF = fopen(filename, "rb");
	if (inpF) {
		printf("Parsing %s\n", lang);
		WriteLocale(ostr, inpF);
		fclose(inpF);
	}
	else {
		printf("COULD NOT OPEN %s\n", lang);
	}
}

int main(int argc, char* argv[]) {
	std::string odata;
	{
		std::ostringstream ostr;
#define LS(name) WriteLocale(LOCTEXT_FOLDER #name ".json", #name, ostr);
		LOCALETYPES
#undef LS
		odata = ostr.str();
	}

	auto obuf = std::unique_ptr<char[]>(new char[ZSTD_COMPRESSBOUND(odata.size())]);
	auto osize = ZSTD_compress(obuf.get(), ZSTD_COMPRESSBOUND(odata.size()), odata.data(), odata.size(), ZSTD_maxCLevel());

	std::ofstream outF(LOCDATA_FILE, std::ios::out | std::ios::binary | std::ios::trunc);
	auto dsize = (uint32_t)odata.size();
	outF.write((char*)&dsize, sizeof(uint32_t));
	outF.write(obuf.get(), osize);
	outF.close();
}