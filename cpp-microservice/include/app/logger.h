#pragma once
#include <cstdio>
#include <ctime>
#include <string>

inline std::string now_ts(){
    char buf[32];
    std::time_t t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

#define LOGI(fmt, ...) std::fprintf(stderr, "[%s][INFO ] " fmt "\n", now_ts().c_str(), ##__VA_ARGS__)
#define LOGW(fmt, ...) std::fprintf(stderr, "[%s][WARN ] " fmt "\n", now_ts().c_str(), ##__VA_ARGS__)
#define LOGE(fmt, ...) std::fprintf(stderr, "[%s][ERROR] " fmt "\n", now_ts().c_str(), ##__VA_ARGS__)
