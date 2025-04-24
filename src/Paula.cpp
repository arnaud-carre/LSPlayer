/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/

// PAULA sound chip emulation to provide perfect WAV preview of the LSP player output on your PC
// ( -amigapreview command line option )

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include "Paula.h"

Paula::Paula(int renderingRate)
{
	m_chipRam = (s8*)malloc(kAmigaChipRamSize);
	m_renderingRate = renderingRate;
	m_dmaCon = 0;
	memset(m_voice, 0, sizeof(m_voice));
}

Paula::~Paula()
{
	if (m_chipRam)
		free(m_chipRam);
}

void	Paula::UploadChipMemoryBank(const void* bank, int size, int uploadAd)
{
	assert(uploadAd + size <= kAmigaChipRamSize);
	memcpy(m_chipRam + uploadAd, bank, size);
}

void	Paula::SetSampleAd(int v, u32 ad)
{
	m_voice[v].nextAd = ad;
}

void	Paula::SetPeriod(int v, u16 per)
{
	if (per < 14)
		per = 14;

	int freq = kPaulaClock / per;
	if (freq > m_renderingRate)
		freq = m_renderingRate;
	m_voice[v].step = (freq << kPaulaPosPrec) / m_renderingRate;
}

void	Paula::SetVolume(int v, u8 vol)
{
	if (vol > 64)
		vol = 64;
	m_voice[v].volume = vol;
}

void	Paula::SetLen(int v, u16 len)
{
	m_voice[v].nextLen = u32(len) * 2;		// len in bytes
}

void Paula::AudioStreamRender(s16* buffer, int sampleCount)
{
	// safe gain is 2 ( 14bits*2voices*2=16bits)
	const int gain = int(2.9f * 256.f);
	for (int i = 0; i < sampleCount; i++)
	{
		int outL = 0;
		int outR = 0;
		outL += m_voice[0].ComputeNextSample(m_chipRam, (m_dmaCon&(1<<0)) != 0);
		outR += m_voice[1].ComputeNextSample(m_chipRam, (m_dmaCon&(1<<1)) != 0);
		outR += m_voice[2].ComputeNextSample(m_chipRam, (m_dmaCon&(1<<2)) != 0);
		outL += m_voice[3].ComputeNextSample(m_chipRam, (m_dmaCon&(1<<3)) != 0);

		outL = (outL * gain)>>8;
		outR = (outR * gain)>>8;

		if (outL < -32768)
			outL = -32768;
		else if (outL > 32767)
			outL = 32767;
		if (outR < -32768)
			outR = -32768;
		else if (outR > 32767)
			outR = 32767;

		buffer[0] = outL;
		buffer[1] = outR;
		buffer += 2;
	}
}

void	Paula::WriteDmaCon(u16 value)
{
	if (value & (1 << 15))
	{
		for (int v = 0; v < 4; v++)
		{
			if (0 == (m_dmaCon & (1 << v)))
			{
				if (value & (1 << v))
				{
					// voice just started
					PaulaVoice& voice = m_voice[v];
					voice.ad = voice.nextAd;
					voice.len = voice.nextLen;
					voice.pos = 0;
				}
			}
		}
		m_dmaCon |= value & 15;
	}
	else
	{
		m_dmaCon &= ~(value & 0xf);
	}
}

int	Paula::PaulaVoice::ComputeNextSample(const s8* chipMemory, bool dmaOn)
{
	if (dmaOn)
	{
		audioDat = chipMemory[ad + (pos >> kPaulaPosPrec)];
		pos += step;
		if ((pos >> kPaulaPosPrec) >= len)
		{
			// looping sound
			ad = nextAd;
			len = nextLen;
			pos &= (1 << kPaulaPosPrec) - 1;
		}
	}
	return audioDat * volume;
}
