#include "PersonalAuth.h"

#ifndef LOG_SECTION
#define LOG_SECTION "PersonalAuth"
#endif

#include "../../Logger.h"

#include <filesystem>
#include <fstream>
#include <rapidjson/document.h>
#include <ShlObj_core.h>
#include <wx/base64.h>

namespace fs = std::filesystem;

struct EGSData {
	std::string Email;
	std::string FirstName;
	std::string LastName;
	std::string DisplayName;
	std::string RefreshToken;
};

inline bool ReadEGSData(std::string_view data, EGSData& out) {
	size_t posErr = 0;
	auto buf = wxBase64Decode(data.data(), data.size(), wxBase64DecodeMode_Strict, &posErr);
	if (posErr) {
		LOG_ERROR("Reading EGS data: B64 decode error at %zu", posErr);
		return false;
	}

	rapidjson::Document dataDoc;
	dataDoc.Parse((char*)buf.GetData(), buf.GetDataLen());
	if (dataDoc.HasParseError()) {
		LOG_ERROR("Reading EGS data: JSON Parse Error %d @ %zu", dataDoc.GetParseError(), dataDoc.GetErrorOffset());
		return false;
	}

	auto& obj = dataDoc.GetArray()[0];
	out.Email = obj["Email"].GetString();
	out.FirstName = obj["Name"].GetString();
	out.LastName = obj["LastName"].GetString();
	out.DisplayName = obj["DisplayName"].GetString();
	out.RefreshToken = obj["Token"].GetString();
	return true;
}

inline bool ReadEGSData(EGSData& out) {
	fs::path EGSSettingsPath;
	{
		PWSTR appDataFolder;
		if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &appDataFolder) != S_OK) {
			LOG_ERROR("Reading EGS data: local app data doesn't exist???");
			return false;
		}
		EGSSettingsPath = fs::path(appDataFolder) / "EpicGamesLauncher" / "Saved" / "Config" / "Windows" / "GameUserSettings.ini";
		CoTaskMemFree(appDataFolder);
	}

	if (fs::status(EGSSettingsPath).type() != fs::file_type::regular) {
		LOG_ERROR("Reading EGS data: EGS probably isn't installed (%s doesn't exist)", EGSSettingsPath.string().c_str());
		return false;
	}

	std::ifstream SFile(EGSSettingsPath);
	if (SFile.good()) {
		std::string line;
		while (std::getline(SFile, line)) {
			if (line == "[RememberMe]") {
				if (std::getline(SFile, line)) {
					if (line == "Enable=True") {
						if (std::getline(SFile, line)) {
							if (line.starts_with("Data=")) {
								return ReadEGSData(std::string_view(line).substr(5), out);
							}
						}
					}
				}
			}
		}
	}

	return false;
}

PersonalAuth::PersonalAuth(const fs::path& filePath, DevCodeCallback devSetupCb) :
	FilePath(filePath),
	DevSetupCb(devSetupCb)
{
	DeviceAuthData auth;
	LOG_INFO("Reading device auth");
	if (ReadDeviceAuth(filePath, auth)) {
		LOG_DEBUG("Using device auth");
		if (UseDeviceAuth(auth)) {
			LOG_DEBUG("Used device auth");
			return;
		}
		LOG_ERROR("Could not use device auth");
	}
	else {
		LOG_ERROR("Could not read device auth");
	}

	/*
	This is commented out because the refresh token *needs* to be used with the
	launcherAppClient2 auth header. This auth can't create device auths, so I can't use it.

	EGSData data;
	LOG_INFO("Reading EGS data");
	if (ReadEGSData(data)) {
		LOG_INFO("Using refresh token");
		if (UseRefreshToken(data.RefreshToken)) {
			LOG_INFO("Creating device auth");
			if (CreateDeviceAuth(auth)) {
				LOG_INFO("Saving device auth");
				if (SaveDeviceAuth(filePath, auth)) {
					LOG_DEBUG("Saved device auth");
					return;
				}
				else {
					LOG_ERROR("Could not save device auth");
				}
			}
			else {
				LOG_ERROR("Could not create device auth");
			}
			return;
		}
		else {
			LOG_ERROR("Could not use refresh token");
		}
	}
	else {
		LOG_ERROR("Could not read EGS data");
	}
	*/

	LOG_INFO("Using device code auth");
	DeviceCodeAuth devAuth(devSetupCb);

	auto& result = devAuth.GetResult();
	if (result.empty()) {
		LOG_FATAL("Could not use device code auth");
		return;
	}
	LOG_INFO("Using device code resp");
	if (UseOAuthResp(result)) {
		LOG_INFO("Creating device auth");
		if (CreateDeviceAuth(auth)) {
			LOG_INFO("Saving device auth");
			if (SaveDeviceAuth(filePath, auth)) {
				LOG_DEBUG("Saved device auth");
				return;
			}
			else {
				LOG_ERROR("Could not save device auth");
			}
		}
		else {
			LOG_ERROR("Could not create device auth");
		}
	}
	else {
		LOG_ERROR("Could not parse OAuth data from device code resp");
	}
}

PersonalAuth::~PersonalAuth()
{

}

bool PersonalAuth::GetExchangeCode(std::string& code)
{
	UpdateIfExpired();

	auto tokenConn = Client::CreateConnection();

	tokenConn->SetUrl("https://account-public-service-prod03.ol.epicgames.com/account/api/oauth/exchange");
	tokenConn->AddRequestHeader("Authorization", "bearer " + AccessToken);
	tokenConn->Start();

	if (tokenConn->GetResponseCode() != 200) {
		LOG_ERROR("Getting exchange code: Response code %d", tokenConn->GetResponseCode());
		return false;
	}

	rapidjson::Document d;
	d.Parse(tokenConn->GetResponseBody().c_str());
	if (d.HasParseError()) {
		LOG_ERROR("Getting exchange code: JSON Parse Error %d @ %zu", d.GetParseError(), d.GetErrorOffset());
		return false;
	}

	code = d["code"].GetString();
	return true;
}

void PersonalAuth::Recreate()
{
	PersonalAuth auth(FilePath, DevSetupCb);
	AccountId = auth.AccountId;
	DisplayName = auth.DisplayName;
	AccessToken = auth.AccessToken;
	RefreshToken = auth.RefreshToken;
	ExpiresAt = auth.ExpiresAt;
}

inline int ParseInt(const char* value)
{
	return std::strtol(value, nullptr, 10);
}

void PersonalAuth::UpdateIfExpired(bool force)
{
	if (!force && ExpiresAt > time(nullptr)) {
		return;
	}
	LOG_DEBUG("Updating auth");
	if (!UseRefreshToken(RefreshToken)) {
		LOG_DEBUG("Refresh token expired, reauthorizing");
		Recreate();
	}
}

bool PersonalAuth::UseOAuthResp(const std::string& data)
{
	rapidjson::Document d;
	d.Parse(data.c_str());
	if (d.HasParseError()) {
		LOG_ERROR("Getting OAuth resp: JSON Parse Error %d @ %zu", d.GetParseError(), d.GetErrorOffset());
		return false;
	}

	AccountId = d["account_id"].GetString();
	DisplayName = d["displayName"].GetString();
	AccessToken = d["access_token"].GetString();
	RefreshToken = d["refresh_token"].GetString();

	rapidjson::Value& expires_at = d["expires_at"];
	auto expires_str = expires_at.GetString();
	constexpr const auto expectedLength = sizeof("YYYY-MM-DDTHH:MM:SSZ") - 1;
	static_assert(expectedLength == 20, "Unexpected ISO 8601 date/time length");

	if (expires_at.GetStringLength() < expectedLength)
	{
		LOG_WARN("Could not parse expires_at value, using expires_in value");
		ExpiresAt = time(nullptr) + d["expires_in"].GetInt();
	}
	else {
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
	return true;
}

bool PersonalAuth::UseRefreshToken(const std::string& token)
{
	auto refreshConn = Client::CreateConnection();
	refreshConn->SetUrl("https://account-public-service-prod.ol.epicgames.com/account/api/oauth/token");
	refreshConn->SetUsePost(true);
	refreshConn->AddRequestHeader("Authorization", BASIC_FN_AUTH);
	refreshConn->AddRequestHeader("Content-Type", "application/x-www-form-urlencoded");

	UrlForm form;
	form.emplace_back("grant_type", "refresh_token");
	form.emplace_back("refresh_token", token);
	refreshConn->SetRequestBody(EncodeUrlForm(form));

	refreshConn->Start();

	if (refreshConn->GetResponseCode() != 200) {
		LOG_ERROR("Use refresh token: Response code %d", refreshConn->GetResponseCode());
		return false;
	}

	return UseOAuthResp(refreshConn->GetResponseBody());
}

bool PersonalAuth::UseDeviceAuth(const DeviceAuthData& auth)
{
	auto deviceConn = Client::CreateConnection();
	deviceConn->SetUrl("https://account-public-service-prod.ol.epicgames.com/account/api/oauth/token");
	deviceConn->SetUsePost(true);
	deviceConn->AddRequestHeader("Authorization", BASIC_FN_AUTH);
	deviceConn->AddRequestHeader("Content-Type", "application/x-www-form-urlencoded");

	UrlForm form;
	form.emplace_back("grant_type", "device_auth");
	form.emplace_back("account_id", auth.AccountId);
	form.emplace_back("device_id", auth.DeviceId);
	form.emplace_back("secret", auth.Secret);
	deviceConn->SetRequestBody(EncodeUrlForm(form));

	deviceConn->Start();

	if (deviceConn->GetResponseCode() != 200) {
		LOG_ERROR("Use device auth: Response code %d", deviceConn->GetResponseCode());
		return false;
	}

	return UseOAuthResp(deviceConn->GetResponseBody());
}

bool PersonalAuth::CreateDeviceAuth(DeviceAuthData& auth)
{
	auto deviceConn = Client::CreateConnection();
	deviceConn->SetUrl(wxString::Format("https://account-public-service-prod.ol.epicgames.com/account/api/public/account/%s/deviceAuth", AccountId));
	deviceConn->AddRequestHeader("Authorization", "bearer " + AccessToken);
	deviceConn->AddRequestHeader("Content-Type", "");
	deviceConn->SetUsePost(true);
	deviceConn->Start();

	if (deviceConn->GetResponseCode() != 200) {
		LOG_ERROR("Create device auth: Response code %d", deviceConn->GetResponseCode());
		return false;
	}

	rapidjson::Document d;
	d.Parse(deviceConn->GetResponseBody().c_str());
	if (d.HasParseError()) {
		LOG_ERROR("Create device auth: JSON Parse Error %d @ %zu", d.GetParseError(), d.GetErrorOffset());
		return false;
	}

	auth.AccountId = d["accountId"].GetString();
	auth.DeviceId = d["deviceId"].GetString();
	auth.Secret = d["secret"].GetString();
}

bool PersonalAuth::SaveDeviceAuth(const fs::path& filePath, const DeviceAuthData& auth)
{
	auto decSize = auth.AccountId.size() + auth.DeviceId.size() + auth.Secret.size() + 3;
	auto decData = std::unique_ptr<char[]>(new char[decSize]);
	strcpy(decData.get(), auth.AccountId.c_str());
	strcpy(decData.get() + auth.AccountId.size() + 1, auth.DeviceId.c_str());
	strcpy(decData.get() + auth.AccountId.size() + auth.DeviceId.size() + 2, auth.Secret.c_str());

	DATA_BLOB dataIn, dataOut;
	dataIn.cbData = decSize;
	dataIn.pbData = (BYTE*)decData.get();

	if (!CryptProtectData(&dataIn, NULL, NULL, NULL, NULL, 0, &dataOut)) {
		LOG_ERROR("Could not encrypt device auth");
		return false;
	}

	auto credF = fopen(filePath.string().c_str(), "wb");
	if (!credF) {
		LocalFree(dataOut.pbData);
		LOG_ERROR("Could not open device auth file");
		return false;
	}
	fwrite(dataOut.pbData, dataOut.cbData, 1, credF);
	fclose(credF);
	LocalFree(dataOut.pbData);
	return true;
}

bool PersonalAuth::ReadDeviceAuth(const fs::path& filePath, DeviceAuthData& auth)
{
	auto credF = fopen(filePath.string().c_str(), "rb");
	if (!credF) {
		LOG_ERROR("Could not open device auth file");
		return false;
	}
	fseek(credF, 0, SEEK_END);
	auto encSize = ftell(credF);
	fseek(credF, 0, SEEK_SET);
	auto encData = std::unique_ptr<char[]>(new char[encSize]);
	fread(encData.get(), encSize, 1, credF);
	fclose(credF);

	DATA_BLOB dataIn, dataOut;
	dataIn.cbData = encSize;
	dataIn.pbData = (BYTE*)encData.get();

	if (!CryptUnprotectData(&dataIn, NULL, NULL, NULL, NULL, 0, &dataOut)) {
		LOG_ERROR("Could not decrypt device auth");
		return false;
	}

	auto decData = (char*)dataOut.pbData;
	auth.AccountId = decData;
	decData += strlen(decData) + 1;
	auth.DeviceId = decData;
	decData += strlen(decData) + 1;
	auth.Secret = decData;

	LocalFree(dataOut.pbData);
	return true;
}
