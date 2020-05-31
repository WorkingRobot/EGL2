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

#include <fstream>
#include <sstream>
#include <string>
#include <zstd.h>

enum class Locale : uint8_t {
	EN,		// English
	FR,		// French
	ES,		// Spanish
	DE,		// German
	AR,		// Arabic
	RU,		// Russian
	IT,		// Italian
	PT_BR,	// Brazilian Portuguese
	PL,		// Polish
	// If you want to help with translating EGL2 into other languages, dm me @AsrielD#6969
	Count
};

inline void WriteString(std::ostringstream& ostr, const char* data, uint16_t length) {
	ostr.write((char*)&length, 2);
	ostr.write((char*)data, length);
}

inline void FindAndReplaceAll(std::string& data, const std::string toSearch, const std::string replaceStr)
{
	// Get the first occurrence
	size_t pos = data.find(toSearch);

	// Repeat till end is reached
	while (pos != std::string::npos)
	{
		// Replace this occurrence of Sub String
		data.replace(pos, toSearch.size(), replaceStr);
		// Get the next occurrence from the current position
		pos = data.find(toSearch, pos + replaceStr.size());
	}
}

inline void WriteLocale(std::ostringstream& ostr, std::ifstream& localePtr) {
	std::string line;
	while (std::getline(localePtr, line)) {
		FindAndReplaceAll(line, "§", "\n");
		WriteString(ostr, line.data(), line.size());
	}
}

#define WRITE_LOCALE(name) \
{ \
	std::ifstream inpF(LOCTEXT_FOLDER #name ".txt", std::ios::in); \
	if (inpF.good()) { \
		WriteLocale(ostr, inpF); \
		inpF.close(); \
	} \
	else { \
		printf("COULD NOT OPEN " #name "\n"); \
	} \
}

int main(int argc, char* argv[]) {
	std::string odata;
	{
		std::ostringstream ostr;
		WRITE_LOCALE(en);
		WRITE_LOCALE(fr);
		WRITE_LOCALE(es);
		WRITE_LOCALE(de);
		WRITE_LOCALE(ar);
		WRITE_LOCALE(ru);
		WRITE_LOCALE(it);
		WRITE_LOCALE(pt_br);
		WRITE_LOCALE(pl);
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