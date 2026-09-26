#pragma once

#include <cstdio>

#define LOGE(...)                                 \
    do {                                          \
        fprintf(stderr, "[ERROR]: " __VA_ARGS__); \
        fprintf(stderr, "\n");                    \
        fflush(stderr);                           \
    } while (false)

#define LOGW(...)                                \
    do {                                         \
        fprintf(stderr, "[WARN]: " __VA_ARGS__); \
        fprintf(stderr, "\n");                   \
        fflush(stderr);                          \
    } while (false)

#define LOGI(...)                                \
    do {                                         \
        fprintf(stderr, "[INFO]: " __VA_ARGS__); \
        fprintf(stderr, "\n");                   \
        fflush(stderr);                          \
    } while (false)
