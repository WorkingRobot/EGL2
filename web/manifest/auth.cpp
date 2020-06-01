#include "auth.h"

#include "../../Logger.h"
#include "../../web/http.h"

#include <rapidjson/document.h>
#include <sstream>

#ifndef LOG_SECTION
#define LOG_SECTION "Auth"
#endif

inline int random(int min, int max) //range : [min, max)
{
	static bool first = true;
	if (first)
	{
		srand(time(NULL)); //seeding for the first time only!
		first = false;
	}
	return min + rand() % ((max + 1) - min);
}

ManifestAuth::ManifestAuth(fs::path& cachePath) :
	CachePath(cachePath),
	ExpiresAt()
{
	UpdateIfExpired(true);
}

ManifestAuth::~ManifestAuth()
{
}

std::pair<std::string, std::string> ManifestAuth::GetLatestManifest()
{
	UpdateIfExpired();

	auto manifestConn = Client::CreateConnection();
	manifestConn->SetUrl("https://launcher-public-service-prod-m.ol.epicgames.com/launcher/api/public/assets/v2/platform/Windows/catalogItem/4fe75bbc5a674f4f9b356b5c90567da5/app/Fortnite/label/Live/");
	manifestConn->SetUsePost(true);

	auto authHeader = std::make_unique<char[]>(7 + AccessToken.size() + 1);
	sprintf(authHeader.get(), "bearer %s", AccessToken.c_str());
	manifestConn->AddRequestHeader("Authorization", authHeader.get());
	manifestConn->AddRequestHeader("Content-Type", "application/json");

	if (!Client::Execute(manifestConn, true)) {
		LOG_WARN("Retrying...");
		return GetLatestManifest();
	}

	if (manifestConn->GetResponseCode() != 200) {
		if (manifestConn->GetResponseCode() == 401) { // unauthorized
			UpdateIfExpired(true);
			return GetLatestManifest();
		}
		LOG_ERROR("Response code %d", manifestConn->GetResponseCode());
		LOG_WARN("Retrying...");
		return GetLatestManifest();
	}

	rapidjson::Document elements;
	elements.Parse(manifestConn->GetResponseBody().c_str());
	if (elements.HasParseError()) {
		LOG_ERROR("Getting manifest url: JSON Parse Error %d @ %zu", elements.GetParseError(), elements.GetErrorOffset());
		LOG_WARN("Retrying...");
		return GetLatestManifest();
	}
	rapidjson::Value& v = elements["elements"].GetArray()[0];

	rapidjson::Value& manifest = v["manifests"][random(0, v["manifests"].GetArray().Size() - 1)];

	rapidjson::Value& uri_val = manifest["uri"];
	if (!manifest.HasMember("queryParams")) {
		return std::make_pair(uri_val.GetString(), v["buildVersion"].GetString());
	}
	else {
		rapidjson::Value& queryParams = manifest["queryParams"];
		std::ostringstream oss;
		oss << uri_val.GetString() << "?";
		for (auto& itr : queryParams.GetArray()) {
			UrlEncode(itr["name"].GetString(), oss);
			oss << "=";
			UrlEncode(itr["value"].GetString(), oss);
			oss << "&";
		}
		oss.seekp(-1, std::ios_base::end); // remove last &
		oss << '\0';
		return std::make_pair(oss.str(), v["buildVersion"].GetString());
	}
}

std::string ManifestAuth::GetManifestId(const std::string& Url)
{
	auto UrlEnd = Url.find(".manifest");
	return std::string(Url.begin() + Url.find_last_of("/", UrlEnd) + 1, Url.begin() + UrlEnd);
}

bool ManifestAuth::IsManifestCached(const std::string& Url)
{
	return fs::status(CachePath / GetManifestId(Url)).type() == fs::file_type::regular;
}

Manifest ManifestAuth::GetManifest(const std::string& Url)
{
	rapidjson::Document manifestDoc;
	if (IsManifestCached(Url)) {
		LOG_DEBUG("Manifest is cached");
		auto fp = fopen((CachePath / GetManifestId(Url)).string().c_str(), "rb");
		fseek(fp, 0, SEEK_END);
		long manifestSize = ftell(fp);
		rewind(fp);
		auto manifestStr = std::make_unique<char[]>(manifestSize);
		fread(manifestStr.get(), 1, manifestSize, fp);
		fclose(fp);

		LOG_DEBUG("Parsing manifest");
		manifestDoc.Parse(manifestStr.get(), manifestSize);
	}
	else {
		LOG_DEBUG("Manifest is not cached");
		auto manifestConn = Client::CreateConnection();
		manifestConn->SetUrl(Url);

		if (!Client::Execute(manifestConn)) {
			LOG_WARN("Retrying...");
			return GetManifest(Url);
		}

		auto fp = fopen((CachePath / GetManifestId(Url)).string().c_str(), "wb");
		fwrite(manifestConn->GetResponseBody().data(), 1, manifestConn->GetResponseBody().size(), fp);
		fclose(fp);

		LOG_DEBUG("Parsing manifest");
		manifestDoc.Parse(manifestConn->GetResponseBody().c_str());
	}

	if (manifestDoc.HasParseError()) {
		LOG_ERROR("Reading manifest: JSON Parse Error %d @ %zu", manifestDoc.GetParseError(), manifestDoc.GetErrorOffset());
		LOG_DEBUG("Removing cached file");
		fs::remove(CachePath / GetManifestId(Url));
		LOG_WARN("Retrying...");
		return GetManifest(Url);
	}

	return Manifest(manifestDoc, Url);
}

inline int ParseInt(const char* value)
{
	return std::strtol(value, nullptr, 10);
}

void ManifestAuth::UpdateIfExpired(bool force)
{
	if (!force && ExpiresAt > time(nullptr)) {
		return;
	}
	LOG_DEBUG("Updating auth");

	auto tokenConn = Client::CreateConnection();

	tokenConn->SetUrl("https://account-public-service-prod03.ol.epicgames.com/account/api/oauth/token");
	tokenConn->SetUsePost(true);
	tokenConn->AddRequestHeader("Authorization", BASIC_FN_AUTH);
	tokenConn->AddRequestHeader("Content-Type", "application/x-www-form-urlencoded");

	tokenConn->SetRequestBody("grant_type=client_credentials");

	if (!Client::Execute(tokenConn)) {
		LOG_WARN("Retrying...");
		return UpdateIfExpired(true);
	}

	rapidjson::Document d;
	d.Parse(tokenConn->GetResponseBody().c_str());
	if (d.HasParseError()) {
		LOG_ERROR("Getting auth creds: JSON Parse Error %d @ %zu", d.GetParseError(), d.GetErrorOffset());
	}

	rapidjson::Value& token = d["access_token"];
	AccessToken = token.GetString();

	rapidjson::Value& expires_at = d["expires_at"];
	auto expires_str = expires_at.GetString();
	constexpr const auto expectedLength = sizeof("YYYY-MM-DDTHH:MM:SSZ") - 1;
	static_assert(expectedLength == 20, "Unexpected ISO 8601 date/time length");

	if (expires_at.GetStringLength() < expectedLength)
	{
		LOG_WARN("Could not parse expires_at value, using expires_in value");
		ExpiresAt = time(nullptr) + d["expires_in"].GetInt();
		return;
	}

	std::tm time = { 0 };
	time.tm_year = ParseInt(&expires_str[0]) - 1900;
	time.tm_mon = ParseInt(&expires_str[5]) - 1;
	time.tm_mday = ParseInt(&expires_str[8]);
	time.tm_hour = ParseInt(&expires_str[11]);
	time.tm_min = ParseInt(&expires_str[14]);
	time.tm_sec = ParseInt(&expires_str[17]);
	time.tm_isdst = 0;
	ExpiresAt = std::mktime(&time);
}
