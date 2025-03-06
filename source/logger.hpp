#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <print>
#include <string>
#include <chrono>
#include <format>
#include <functional>
#include <mutex>

enum class LogLevel { INFO, ERROR };

class Logger {
public:
    static void log(LogLevel level, const std::string& message, bool verbose = false);
    static void setProgressCallback(std::function<void(double, double, double)> cb);
    static void reportProgress(double percent, double speed, double eta);
    static void resetProgress();

    static bool verboseMode;

private:
    static std::mutex logMutex;
    static std::function<void(double, double, double)> progressCallback;
    static double lastProgress;
    static bool progressComplete;
    static std::chrono::steady_clock::time_point lastUpdateTime;
    static double lastUploadedBytes;
};

#endif
