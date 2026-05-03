/*********************************************************************

 LSP (Light Speed Player) Converter
 Fastest & Tiniest 68k MOD player ever!
 Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
 https://github.com/arnaud-carre/LSPlayer

 *********************************************************************/
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "adpcm.h"
#include "WavWriter.h"

static const int8_t sAdpcmTable[16] =
{
	0,1,2,4,8,16,32,64,-128,-64,-32,-16,-8,-4,-2,-1
};

static uint8_t bestIndex(int value, int next, const int8_t* table)
{
	uint8_t bestId = 0;
	uint32_t bestDist = ~0;
	int diff = next - value;
	for (int i = 0; i < 16; i++)
	{
		int emulatedValue = value + table[i];
		if ((emulatedValue < -128) || (emulatedValue > 127))	// discard overshot value during encoding to avoid clamping in runtime decoder
			continue;

		uint32_t dist = abs(diff - table[i]);
		if (dist < bestDist)
		{
			bestDist = dist;
			bestId = i;
		}
	}
	return bestId;
}

void	dpcmEncode(const int8_t* input, int inLen, uint8_t* output, const int8_t* table)
{
	if (nullptr == table)
		table = sAdpcmTable;

	assert(0 == (inLen & 1));		// amiga samples len are always even
	int sample = 0;
	for (int i = 0; i < inLen; i+=2)
	{
		uint8_t nib0 = bestIndex(sample, input[i], table);
		sample += table[nib0];
		uint8_t nib1 = bestIndex(sample, input[i+1], table);
		sample += table[nib1];
		output[i >> 1] = (nib0 << 4) | (nib1);
	}
}

void dpcmDecode(const uint8_t* stream, int inLen, int8_t* output, const int8_t* table)
{
	if (nullptr == table)
		table = sAdpcmTable;

	int sample = 0;
	for (int i = 0; i < inLen; i++)
	{
		uint8_t byte = stream[i];
		sample += table[byte >> 4];
		assert((sample >= -128) && (sample <= 127));
		*output++ = (int8_t)sample;
		sample += table[byte&15];
		assert((sample >= -128) && (sample <= 127));
		*output++ = (int8_t)sample;
	}
}
