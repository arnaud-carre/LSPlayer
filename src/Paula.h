/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/

// PAULA sound chip emulation to provide perfect WAV preview of the LSP player output on your PC
// ( -amigapreview command line option )

#pragma once

#include "LSPTypes.h"

static	const int	kPaulaPosPrec = 15;
static const int	kAmigaChipRamSize = 2*512*1024;
static const int	kPaulaClock = 3546895;

class Paula
{
public:

			Paula(int renderingRate);
			~Paula();

	void	UploadChipMemoryBank(const void* bank, int size, int uploadAd);

	void	AudioStreamRender(s16* buffer, int sampleCount);
	void	WriteDmaCon(u16 value);

	void	SetVolume(int v, u8 value);
	void	SetPeriod(int v, u16 per);
	void	SetSampleAd(int v, u32 ad);
	void	SetLen(int v, u16 ad);

private:

	struct PaulaVoice 
	{
		u32	pos;
		u32 ad;
		u32 len;
		u32 nextAd;
		u32 nextLen;
		int volume;
		u32 step;
		int audioDat;

		int	ComputeNextSample(const s8* chipMemory, bool dmaOn);
	};

	PaulaVoice	m_voice[4];

	u16		m_dmaCon;
	s8*		m_chipRam;
	int		m_chipRamSize;
	int		m_renderingRate;

};