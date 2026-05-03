#pragma once
#include <stdint.h>

void dpcmEncode(const int8_t* input, int inLen, uint8_t* output, const int8_t* table = nullptr);
void dpcmDecode(const uint8_t* stream, int inLen, int8_t* output, const int8_t* table = nullptr);
