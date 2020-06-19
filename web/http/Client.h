#pragma once

#include "../../containers/cancel_flag.h"

#include <chrono>
#include <curlion.h>
#include <memory>

class Client {
public:
	Client();
	~Client();

	std::error_condition StartConnection(const std::shared_ptr<curlion::Connection>& connection);
	std::error_condition AbortConnection(const std::shared_ptr<curlion::Connection>& connection);
	
	static inline void SetPoolSize(long poolSize) {
		PoolSize = poolSize ? poolSize : DefaultPoolSize;
	}

	static inline std::shared_ptr<curlion::HttpConnection> CreateConnection() {
		auto conn = std::make_shared<curlion::HttpConnection>();
		curl_easy_setopt(conn->GetHandle(), CURLOPT_MAXCONNECTS, PoolSize.load());
		//conn->SetProxy("127.0.0.1:8888");
		//conn->SetVerifyCertificate(false);
		return conn;
	}

	static bool Execute(const std::shared_ptr<curlion::HttpConnection>& connection, cancel_flag& flag, bool allowNon200 = false);

private:
	static constexpr long DefaultPoolSize = 5; // curl default
	static inline std::atomic_long PoolSize = DefaultPoolSize;

	static FILE* CreateTempFile();

	void* io_service;
	std::unique_ptr<curlion::ConnectionManager> connection_manager;
};