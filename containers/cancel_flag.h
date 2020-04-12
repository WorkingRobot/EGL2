#pragma once

#include <atomic>

class cancel_flag {
public:
	bool cancelled() {
		return value.load();
	}

	bool cancelled() const {
		return value.load();
	}

	bool cancelled() volatile {
		return value.load();
	}

	bool cancelled() const volatile {
		return value.load();
	}

	void cancel() {
		value.store(true);
	}

	void cancel() volatile {
		value.store(true);
	}

protected:
	std::atomic<bool> value = false;
};