#pragma once

#define LOG_DEBUG(str, ...) Logger::Log(Logger::LogLevel::DEBUG,  LOG_SECTION, str, __VA_ARGS__)
#define LOG_INFO(str, ...)  Logger::Log(Logger::LogLevel::INFO,   LOG_SECTION, str, __VA_ARGS__)
#define LOG_WARN(str, ...)  Logger::Log(Logger::LogLevel::WARN,   LOG_SECTION, str, __VA_ARGS__)
#define LOG_ERROR(str, ...) Logger::Log(Logger::LogLevel::ERROR_, LOG_SECTION, str, __VA_ARGS__)
#define LOG_FATAL(str, ...) Logger::Log(Logger::LogLevel::FATAL,  LOG_SECTION, str, __VA_ARGS__)

#include <functional>
#include <memory>
#include <thread>

class Logger {
public:
	enum class LogLevel : uint8_t {
		UNKNOWN,
		DEBUG,
		INFO,
		WARN,
		ERROR_,
		FATAL
	};

	static bool Setup();

	inline static uint32_t GetThreadId() {
		auto id = std::this_thread::get_id();
		return *(uint32_t*)&id;
	}

	template<typename... Args>
	static void Log(LogLevel level, const char* section, const char* str, Args... args) {
		auto size = snprintf(nullptr, 0, str, args...) + 1;
		auto buf = std::make_unique<char[]>(size);
		snprintf(buf.get(), size, str, args...);
		printf("%s%s - %d %s: %s\n%s", Logger::LevelAsColor(level), Logger::LevelAsString(level), GetThreadId(), section, buf.get(), Logger::ResetColor);
		if (Callback) {
			Callback(level, section, buf.get());
		}
	}

	static constexpr const char* LevelAsString(LogLevel level) {
		switch (level)
		{
		case LogLevel::DEBUG:
			return "Debug";
		case LogLevel::INFO:
			return "Info ";
		case LogLevel::WARN:
			return "Warn ";
		case LogLevel::ERROR_:
			return "Error";
		case LogLevel::FATAL:
			return "Fatal";
		default:
			return "Unkwn";
		}
	}

	static constexpr const char* LevelAsColor(LogLevel level) {
		switch (level)
		{
		case LogLevel::DEBUG:
			return "\33[0;37m";
		case LogLevel::INFO:
			return "\33[0;92m";
		case LogLevel::WARN:
			return "\33[0;93m";
		case LogLevel::ERROR_:
			return "\33[0;91m";
		case LogLevel::FATAL:
			return "\33[0;31m";
		default:
			return "\33[0;95m";
		}
	}

	static constexpr const char* ResetColor = "\33[0m";

	using callback = std::function<void(LogLevel, const char*, const char*)>;
	static inline callback Callback = nullptr;
};