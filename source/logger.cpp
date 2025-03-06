#include "logger.hpp"
#include <chrono>

bool Logger::verboseMode = false;
std::mutex Logger::logMutex;
std::function<void(double, double, double)> Logger::progressCallback = nullptr;
double Logger::lastProgress = -1.0;
bool Logger::progressComplete = false;
std::chrono::steady_clock::time_point Logger::lastUpdateTime = std::chrono::steady_clock::now();
double Logger::lastUploadedBytes = 0.0;

void Logger::log(LogLevel level, const std::string& message, bool verbose) {
    if (!verbose || verboseMode) {
        std::lock_guard<std::mutex> lock(logMutex);
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::string timestamp = std::format("[{:%Y-%m-%d %H:%M:%S}] ", std::chrono::system_clock::from_time_t(time));
        std::string prefix = (level == LogLevel::ERROR) ? "\033[31m[ERROR]\033[0m " : "\033[32m[INFO]\033[0m ";
        std::print(stderr, "{}{}{}\n", timestamp, prefix, message);
    }
}

void Logger::setProgressCallback(std::function<void(double, double, double)> cb) {
    std::lock_guard<std::mutex> lock(logMutex);
    progressCallback = cb;
}

void Logger::reportProgress(double percent, double speed, double eta) {
    if (progressCallback && verboseMode && percent >= 0 && percent <= 100 && !progressComplete) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (lastProgress < 0 || percent >= lastProgress + 1.0 || percent == 100.0) {
            progressCallback(percent, speed, eta);
            lastProgress = percent;
            if (percent == 100.0) progressComplete = true;
        }
    }
}

void Logger::resetProgress() {
    std::lock_guard<std::mutex> lock(logMutex);
    lastProgress = -1.0;
    progressComplete = false;
    lastUpdateTime = std::chrono::steady_clock::now();
    lastUploadedBytes = 0.0;
}
