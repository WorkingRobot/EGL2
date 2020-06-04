#pragma once

#include "../../containers/cancel_flag.h"

#include <memory>
#include <curlion.h>

class Client {
public:
	Client();
	~Client();

	std::error_condition StartConnection(const std::shared_ptr<curlion::Connection>& connection);
	std::error_condition AbortConnection(const std::shared_ptr<curlion::Connection>& connection);

	static inline std::shared_ptr<curlion::HttpConnection> CreateConnection() {
		auto conn = std::make_shared<curlion::HttpConnection>();
		//conn->SetVerbose(true);
		//conn->SetProxy("127.0.0.1:8888");
		//conn->SetVerifyCertificate(false);
		return conn;
	}

	static bool Execute(const std::shared_ptr<curlion::HttpConnection>& connection, cancel_flag& flag, bool allowNon200 = false);

private:
	static FILE* CreateTempFile();

	void* io_service;
	std::unique_ptr<curlion::ConnectionManager> connection_manager;
};