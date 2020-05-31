#pragma once

// fortniteIOSGameClient
// the only reason this is IOS and not PC is that PC can't create device auths :(
#define BASIC_FN_AUTH "basic MzQ0NmNkNzI2OTRjNGE0NDg1ZDgxYjc3YWRiYjIxNDE6OTIwOWQ0YTVlMjVhNDU3ZmI5YjA3NDg5ZDMxM2I0MWE="

#include "http/Client.h"

#include <vector>
#include <sstream>
#include <string>

inline void UrlEncode(const std::string& s, std::ostringstream& e)
{
	static constexpr const char lookup[] = "0123456789abcdef";
	for (int i = 0, ix = s.size(); i < ix; i++)
	{
		const char& c = s.data()[i];
		if ((48 <= c && c <= 57) ||//0-9
			(65 <= c && c <= 90) ||//abc...xyz
			(97 <= c && c <= 122) || //ABC...XYZ
			(c == '-' || c == '_' || c == '.' || c == '~')
			)
		{
			e << c;
		}
		else
		{
			e << '%';
			e << lookup[(c & 0xF0) >> 4];
			e << lookup[(c & 0x0F)];
		}
	}
}

typedef std::vector<std::pair<std::string, std::string>> UrlForm;
inline std::string EncodeUrlForm(const UrlForm& formData) {
	std::ostringstream oss;
	for (auto& itr : formData) {
		UrlEncode(itr.first, oss);
		oss << "=";
		UrlEncode(itr.second, oss);
		oss << "&";
	}
	oss.seekp(-1, std::ios_base::end);
	return oss.str().erase(oss.tellp());
}