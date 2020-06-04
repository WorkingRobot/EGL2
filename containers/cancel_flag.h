#pragma once

#include <atomic>

#define SAFE_FLAG_RETURN(value) if (flag.cancelled()) return value;

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
	std::atomic_bool value = false;
};