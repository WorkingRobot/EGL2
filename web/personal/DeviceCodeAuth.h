#pragma once

#include "../http.h"

#include <functional>

typedef std::function<void(const std::string&, const std::string&)> DevCodeCallback;

class DeviceCodeAuth {
public:
	DeviceCodeAuth(DevCodeCallback setupCb);
	~DeviceCodeAuth();

	inline const std::string& GetResult() {
		return OAuthResult;
	}

private:
	std::string OAuthResult;
};