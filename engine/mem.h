#pragma once

#include <cstdint>

void* memalloc(uint64_t size);
void memfree(void* ptr);
