#pragma once

#include <stdio.h>
#include <stdlib.h>

#define LOGE(...)                                 \
    do                                            \
    {                                             \
        fprintf(stderr, "[ERROR]: " __VA_ARGS__); \
        fflush(stderr);                           \
    } while (false)

#define LOGW(...)                                \
    do                                           \
    {                                            \
        fprintf(stderr, "[WARN]: " __VA_ARGS__); \
        fflush(stderr);                          \
    } while (false)

#define LOGI(...)                                \
    do                                           \
    {                                            \
        fprintf(stderr, "[INFO]: " __VA_ARGS__); \
        fflush(stderr);                          \
    } while (false)