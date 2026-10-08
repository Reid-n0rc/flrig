// fake debug.h: LOG_* go to fake_log() (fakes.cxx), which counts messages
// and those that mention "discard" (patch 8, T4). Nothing is printed.
#pragma once
void fake_log(const char *fmt, ...);
#define LOG_DEBUG(...) fake_log(__VA_ARGS__)
#define LOG_INFO(...)  fake_log(__VA_ARGS__)
#define LOG_WARN(...)  fake_log(__VA_ARGS__)
#define LOG_ERROR(...) fake_log(__VA_ARGS__)
#define LOG_QUIET(...) fake_log(__VA_ARGS__)
#define LOG_VERBOSE(...) fake_log(__VA_ARGS__)
#define LOG_PERROR(...) ((void)0)
