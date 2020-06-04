#include "DeviceCodeAuth.h"

#ifndef LOG_SECTION
#define LOG_SECTION "DevCodeAuth"
#endif

#include "../../Logger.h"

#include <rapidjson/document.h>
#include <thread>

DeviceCodeAuth::DeviceCodeAuth(DevCodeCallback setupCb)
{
	std::string accessToken;
	{
		auto tokenConn = Client::CreateConnection();

		tokenConn->SetUrl("https://account-public-service-prod03.ol.epicgames.com/account/api/oauth/token");
		tokenConn->SetUsePost(true);
		tokenConn->AddRequestHeader("Authorization", BASIC_FN_AUTH);
		tokenConn->AddRequestHeader("Content-Type", "application/x-www-form-urlencoded");

		tokenConn->SetRequestBody("grant_type=client_credentials");

		if (!Client::Execute(tokenConn, cancel_flag())) {
			LOG_ERROR("Getting access token: failed");
			return;
		}

		rapidjson::Document d;
		d.Parse(tokenConn->GetResponseBody().c_str());
		if (d.HasParseError()) {
			LOG_ERROR("Getting access token: JSON Parse Error %d @ %zu", d.GetParseError(), d.GetErrorOffset());
			return;
		}

		accessToken = d["access_token"].GetString();
	}

	std::string deviceCode;
	{
		auto deviceConn = Client::CreateConnection();

		deviceConn->SetUrl("https://account-public-service-prod03.ol.epicgames.com/account/api/oauth/deviceAuthorization");
		deviceConn->SetUsePost(true);
		deviceConn->AddRequestHeader("Authorization", "bearer " + accessToken);
		
		if (!Client::Execute(deviceConn, cancel_flag())) {
			LOG_ERROR("Init device code token: failed");
			return;
		}

		rapidjson::Document d;
		d.Parse(deviceConn->GetResponseBody().c_str());
		if (d.HasParseError()) {
			LOG_ERROR("Init device code: JSON Parse Error %d @ %zu", d.GetParseError(), d.GetErrorOffset());
			return;
		}

		setupCb(d["verification_uri_complete"].GetString(), d["user_code"].GetString());
		deviceCode = d["device_code"].GetString();
	}

	while (1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));

		auto deviceConn = Client::CreateConnection();

		deviceConn->SetUrl("https://account-public-service-prod03.ol.epicgames.com/account/api/oauth/token");
		deviceConn->SetUsePost(true);
		deviceConn->AddRequestHeader("Authorization", BASIC_FN_AUTH);
		deviceConn->AddRequestHeader("Content-Type", "application/x-www-form-urlencoded");

		UrlForm form;
		form.emplace_back("grant_type", "device_code");
		form.emplace_back("device_code", deviceCode);
		deviceConn->SetRequestBody(EncodeUrlForm(form));

		if (!Client::Execute(deviceConn, cancel_flag(), true)) {
			LOG_ERROR("Attempt device code: failed");
			continue;
		}

		switch (deviceConn->GetResponseCode()) {
		case 200:
			OAuthResult = deviceConn->GetResponseBody();
			return;
		case 400:
			break;
		default:
			LOG_ERROR("Attempt device code: Response code %d", deviceConn->GetResponseCode());
			break;
		}
	}
}

DeviceCodeAuth::~DeviceCodeAuth()
{
	
}