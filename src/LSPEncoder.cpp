/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/

#define	_CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "MemoryStream.h"
#include "LSPEncoder.h"
#include "LSPDecoder.h"
#include "crc32.h"
#include "external/micromod/micromod.h"
#include "WavWriter.h"
#ifdef MACOS_LINUX
#include <string>
#include <filesystem>
#include "WindowsCompat.h"
#endif

static const	int kWordStreamId = 0;
static const	int kByteStreamId = 1;
static const	int kMicroCmdStreamId = 0;

static const	int	kResampleShrinkMarginPercent = 5;

extern long tick_len;
int	ShrinklerCompressEstimate(u8* data, int size);


void	ConvertParams::SetNameWithExtension(const char* src, char* dst, const char* sExt, const char* sNamePostfix)
{
#ifndef MACOS_LINUX
	char sDrive[_MAX_DRIVE];
	char sDir[_MAX_DIR];
	char sName[_MAX_FNAME];
	_splitpath_s(src, sDrive, _MAX_DRIVE, sDir, _MAX_DIR, sName, _MAX_FNAME, NULL, 0);
	if (sNamePostfix)
		strcat_s(sName, sNamePostfix);
	_makepath_s(dst, _MAX_PATH, sDrive, sDir, sName, sExt);
#else
    std::string srcStdStr { src };
    std::filesystem::path srcPath { srcStdStr };
    auto path = srcPath.parent_path().string();
    if(!path.empty()) path += std::filesystem::path::preferred_separator;
    auto stem = srcPath.stem();
    auto newName = path + stem.string() + (sNamePostfix != NULL ? sNamePostfix : "") + sExt;
    strcpy(dst, newName.c_str());
#endif
}


LSPEncoder::LSPEncoder()
{
	memset(this, 0, sizeof(LSPEncoder));
	m_convertParams.Reset();
	m_channelMaskFilter = 0xf;
	m_EscValueGetPos = -1;
	m_EscValueRewind = -1;
	m_EscValueSetBpm = -1;
}

LSPEncoder::~LSPEncoder()
{
	Reset();
}

static	void	w16(FILE* h, u16 v)
{
	fputc(v >> 8, h);
	fputc(v&255, h);
}

static	void	w32(FILE* h, u32 v)
{
	w16(h, v >> 16);
	w16(h, v & 0xffff);
}

void	LSPEncoder::Reset()
{
	for (int i = 0; i < MOD_CHANNEL_COUNT; i++)
	{
		free(m_ChannelRowData[i]);
		m_ChannelRowData[i] = NULL;
	}
	free(m_RowData);
	m_RowData = NULL;
	m_cmdEncoder.Setup(1<<16, LSP_CMDWORD_MAX);
	m_lspIntrumentEncoder.Setup(31 << 8, LSP_INSTRUMENT_MAX);
	m_periodEncoder.Setup(1 << 12, 256);
	m_sampleOffsetUsed = false;
	m_setBpmCount = 0;
	m_bpm = 125;
	m_minTickRate = 50;
	m_modInstrumentUsedMask = 0;
	m_frameCount = 0;
	m_uniqueId = 0;
	m_originalModSoundBank = NULL;
	m_originalModSoundBankSize = 0;
}

void	LSPEncoder::AddLSPInstrument(int id, int modInstrument, int sampleOffsetInBytes)
{
	// add sample to the LSP bank
	assert(unsigned(id) < LSP_INSTRUMENT_MAX);
	assert((modInstrument >= 1) && (modInstrument <= 31));

	LspSample& lspSample = m_lspSamples[modInstrument-1];

	LSPInstrument& dst = m_lspIntruments[id];
	if (sampleOffsetInBytes >= lspSample.len)
	{
		printf("Warning: bad Sample Offset data for instrument #%d (offset=%d, len=%d) Force to %d\n", modInstrument, sampleOffsetInBytes, lspSample.len, lspSample.repStart);
		sampleOffsetInBytes = lspSample.repStart;
	}

	dst.lspSampleId = modInstrument;
	dst.sampleOffset = sampleOffsetInBytes;
	if (sampleOffsetInBytes > lspSample.sampleOffsetMax)
		lspSample.sampleOffsetMax = sampleOffsetInBytes;
}

int LSPEncoder::VoiceCodeCompute(int frameDmaCon, int frameResetMask, int frameInstMask) const
{
	int ret = 0;
	for (int v = 0; v < 4; v++)
	{
		// 3bits to 2 bits code
		int code = ((frameResetMask & 1) << 2) | ((frameDmaCon & 1) << 1) | (frameInstMask & 1);
		switch (code)
		{
		case 0: break;
		case 1:		ret |= kPlayWithoutNote << (v * 2);	break;
		case 3:		ret |= kPlayInstrument << (v * 2);	break;
		case 4:		ret |= kResetLen << (v * 2);	break;
		default:
			assert(false);
			return -1;
		}
		frameResetMask >>= 1;
		frameDmaCon >>= 1;
		frameInstMask >>= 1;
	}
	return ret;
}

bool	LSPEncoder::LoadModule()
{

	const char* filename = m_convertParams.m_modFilename;

	printf("Loading %s...\n", filename);
	Reset();

	WavWriter micromodOutput;

	bool ret = false;
	FILE* h = fopen(filename, "rb");
	if (h)
	{
		fseek(h, 0, SEEK_END);
		m_ModFileSize = ftell(h);
		fseek(h, 0, SEEK_SET);
		m_ModBuffer = (u8*)malloc(m_ModFileSize);
		fread(m_ModBuffer, 1, m_ModFileSize, h);
		fclose(h);

		int numchan = calculate_num_channels((signed char*)m_ModBuffer);
		if (4 == numchan)
		{
			// compute unique id (MOD content + LSPEncoder version + convert parameters)
			u32 crc = CrcUpdate(~0, m_ModBuffer, m_ModFileSize);
			static const u32 version[2] = { LSP_MAJOR_VERSION, LSP_MINOR_VERSION };
			crc = CrcUpdate(crc, (const unsigned char*)version, sizeof(version));
			crc = CrcUpdate(crc, (const unsigned char*)&m_convertParams.m_keepModSoundBankLayout, sizeof(m_convertParams.m_keepModSoundBankLayout));
			crc = CrcUpdate(crc, (const unsigned char*)&m_convertParams.m_lspMicro, sizeof(m_convertParams.m_lspMicro));
			m_uniqueId = crc;

			m_MODScoreSize = micromod_calculate_score_len((signed char*)m_ModBuffer);


			m_sampleWithoutANote = false;

			if (m_MODScoreSize > 0)
			{
				// as micromod_initialise calls "sequence_tick" (?!!), we should alloc up-front
				m_frameMax = 60 * 30 * 100;	// consider 30 minutes MOD at 100Hz tick
				for (int i = 0; i < MOD_CHANNEL_COUNT; i++)
				{
					m_ChannelRowData[i] = (ChannelRowData*)calloc(m_frameMax, sizeof(ChannelRowData));
					m_previousVolumes[i] = -1;
					m_previousPeriods[i] = -1;
					m_previousInstrument[i] = -1;
				}
				m_RowData = (LspFrameData*)calloc(m_frameMax, sizeof(LspFrameData));
				memset(m_seqPosFrame, 0xff, sizeof(m_seqPosFrame));
				m_frameLoop = 0;
				m_seqHighest = -1;

				if (0 == micromod_initialise((signed char*)m_ModBuffer, HOST_REPLAY_RATE))
				{
					if (m_convertParams.m_renderWav)
					{
						micromodOutput.Open(m_convertParams.m_sWavFilename, HOST_REPLAY_RATE, 2);
						printf("Rendering into %s...\n", m_convertParams.m_sWavFilename);
					}

					m_totalSampleCount = 0;
					m_frameCount = 0;
					m_seqPosFrame[0] = 0;
					ret = true;

					AudioBuffer tmpBuffer(2);

					int previousDmacon = 0;
					int previousInstrument[4] = {};

					//---------------------------------------------------------------------------------------
					// play the complete .mod and store all data per frame in LspFrameData & ChannelRowData
					//---------------------------------------------------------------------------------------
					while (0 == sequence_tick())
					{
						// run the mixer to get the exact amount of each instrument used
						s16* buffer = tmpBuffer.GetAudioBuffer(tick_len);
						simulateMixing(buffer, tick_len);
						if (m_convertParams.m_renderWav)
							micromodOutput.AddAudioData(buffer, tick_len);

						if (m_frameCount >= m_frameMax)
						{
							printf("Fatal ERROR: Music end detection issue (song is more than %d ticks)\n", m_frameMax);
							return false;
						}
						m_frameCount++;
						m_totalSampleCount += tick_len;
					}

					// If any loop point, force the vol & per to be set
					assert(m_frameLoop >= 0);
					for (int v=0;v<4;v++)
					{
						m_ChannelRowData[v][m_frameLoop].volSet = true;
						m_ChannelRowData[v][m_frameLoop].perSet = true;
					}

					//---------------------------------------------------------------------------------------
					// And now read back the data to produce LSP delta stream & proper wordCmd, including
					// the right setVol and setPer at loop point
					//---------------------------------------------------------------------------------------
					for (int frame=0;frame<m_frameCount;frame++)
					{
						LspFrameData& out = m_RowData[frame];

						int frameDmaCon = 0;
						int frameVolMask = 0;
						int frameInstMask = 0;
						int framePerMask = 0;

						for (int v = 3; v >= 0; v--)
						{
							const ChannelRowData& data = m_ChannelRowData[v][frame];

							if (data.instrument > 0)
							{
								const int sampleOffsetCode = data.sampleOffsetInBytes >> 8;
								assert(unsigned(sampleOffsetCode) < 256);
								int intrValue = ((data.instrument - 1) << 8) | sampleOffsetCode;

								if (!m_lspIntrumentEncoder.IsValueRegistered(intrValue))
								{	// create a LSP entry
									int code = m_lspIntrumentEncoder.RegisterValue(intrValue);
									if (MicroMode() && (code >= 256))
									{
										printf("Fatal ERROR: LSP only supports 256 Instruments max in Micro mode (too many $9xx commands)\n");
										return false;
									}
									if (code < LSP_INSTRUMENT_MAX)
									{
										AddLSPInstrument(code, data.instrument, data.sampleOffsetInBytes);
									}
									else
									{
										printf("Fatal ERROR: More than %d LSP Instruments (too many $9xx commands)\n", LSP_INSTRUMENT_MAX);
										return false;
									}
								}

								if (data.dmaRestart)
								{
									frameInstMask |= (1 << v);
									frameDmaCon |= (1 << v);
								}
								else
								{
									if (previousInstrument[v] != data.instrument)
									{
										frameInstMask |= (1 << v);
										previousInstrument[v] = data.instrument;
									}
								}
							}

							if (data.volSet)
								frameVolMask |= (1 << v);

							if (data.perSet)
							{
								framePerMask |= (1 << v);
								m_periodEncoder.RegisterValue(data.period);
							}
						}

						int frameResetMask = previousDmacon;
						frameResetMask &= ~frameDmaCon;				// do not reset any "set instrument"

						if (MicroMode())
						{
							assert(frameInstMask == frameDmaCon);		// temp: not supporting instrument without note
							out.wordCmd = (frameVolMask << 8) | (framePerMask << 4) | (frameDmaCon);
						}
						else
						{
							int voiceCode = VoiceCodeCompute(frameDmaCon, frameResetMask, frameInstMask);
							assert(voiceCode >= 0);
							assert(voiceCode <= 255);
							out.wordCmd = (voiceCode << 8) | (frameVolMask << 4) | (framePerMask << 0);
						}

						int cmd = m_cmdEncoder.RegisterValue(out.wordCmd);
						if ((cmd < 0) || (cmd >= LSP_CMDWORD_MAX))
						{
							printf("Fatal error: Too many LSP cmd words (%d)\n", cmd);
							return false;
						}

						previousDmacon = frameDmaCon;
					}

				// important: sort values to minimize "more than 1 byte" commands
					m_cmdEncoder.SortValues();

				// now ESC codes *should* be >255 (to optimize a bit player by not testing all ESC code most of the time)
				// so if not enough codes, we build dummy codes
					while (m_cmdEncoder.GetCodesCount() < 255)
						m_cmdEncoder.AddDummyCodeEntry();

					// now find some "free" codes for "end of stream", "set bpm" or "GetPos"
					m_EscValueRewind = m_cmdEncoder.GetFirstUnusedValue();
					assert(!m_cmdEncoder.IsValueRegistered(m_EscValueRewind));
					m_cmdEncoder.RegisterValue(m_EscValueRewind);

				// always register valid cmd to avoid collisions
					m_EscValueSetBpm = m_cmdEncoder.GetFirstUnusedValue();
					assert(!m_cmdEncoder.IsValueRegistered(m_EscValueSetBpm));
					m_cmdEncoder.RegisterValue(m_EscValueSetBpm);

					m_EscValueGetPos = m_cmdEncoder.GetFirstUnusedValue();
					assert(!m_cmdEncoder.IsValueRegistered(m_EscValueGetPos));
					m_cmdEncoder.RegisterValue(m_EscValueGetPos);

//					m_periodEncoder.SortValues();

					m_modDurationSec = (m_totalSampleCount+ HOST_REPLAY_RATE-1) / HOST_REPLAY_RATE;

					if (m_convertParams.m_renderWav)
						micromodOutput.Close();

					// stats
					for (int i = 1; i <= 31; i++)
					{
						const LspSample& info = m_lspSamples[i - 1];
						if ((m_modInstrumentUsedMask&(1 << (i - 1))))
						{
							if (m_convertParams.m_verbose)
							{
								printf("Instrument #%2d: %d bytes, max replay rate = %dHz\n", i, info.len, info.maxReplayRate);
								if (info.repStart + info.repLen > info.len)
								{
									printf("  Warning: sample goes over RepStart+RepLen (%d > %d)\n", info.len, info.repStart + info.repLen);
								}
								if (info.resampleMaxLen > 0)
								{
									int cmpLen = (info.resampleMaxLen * (100 + kResampleShrinkMarginPercent)) / 100;
									if (info.len > cmpLen)
										printf("  Warning: Only use %d sample bytes (len=%d)\n", info.resampleMaxLen, info.len);
								}
							}
						}
						else
						{
							if (info.len > 2)
							{
								if (m_convertParams.m_verbose)
								{
									if ( 0 == info.resampleMaxLen )
										printf("Warning: Instrument #%d is never used! (len=%d)\n", i, info.len);
//									assert(0 == info.resampleMaxLen);
								}
							}
						}
					}

					DisplayInfos();
				}
			}
			else
			{
				printf("ERROR: This is not a valid Amiga MOD file\n");
			}
		}
		else
		{
			printf("ERROR: This file is %d channel(s) (AMIGA LSP only supports 4 channels)\n", numchan);
		}
	}
	else
	{
		printf("ERROR: Unable to load file \"%s\"\n", filename);
	}

	const int codesCount = m_cmdEncoder.GetCodesCount();
	if (codesCount > LSP_CMDWORD_MAX)			// 765 max because "extended" and "rewind" code
	{
		printf("ERROR: Too many Cmd combine (%d)\n", codesCount);
		ret = false;
	}

	if (MicroMode())
	{
		if ( m_sampleWithoutANote )
		{
			printf("ERROR: \"-micro\" mode does NOT support \"sample without a note\" technic.\n");
			ret = false;
		}
		if (m_setBpmCount > 1)
		{
			printf("ERROR: \"-micro\" mode does NOT support BPM change within the song\n");
			ret = false;
		}
	}


	return ret;
}

void	LSPEncoder::SetPeriod(int channel, int period)
{
	if (m_channelMaskFilter & (1 << channel))
	{
		assert(unsigned(channel) < unsigned(MOD_CHANNEL_COUNT));
		assert((period >= AMIGA_PER_MIN) && (period < (1 << AMIGA_PERIOD_BITS)));
		m_ChannelRowData[channel][m_frameCount].period = period;
		m_ChannelRowData[channel][m_frameCount].perSet = (period != m_previousPeriods[channel]);
		m_previousPeriods[channel] = period;
	}
}

void	LSPEncoder::SetVolume(int channel, int volume)
{
	if (m_channelMaskFilter & (1 << channel))
	{
		assert(unsigned(channel) < unsigned(MOD_CHANNEL_COUNT));
		assert(unsigned(volume) <= unsigned(64));
		m_ChannelRowData[channel][m_frameCount].volume = volume;
		m_ChannelRowData[channel][m_frameCount].volSet = (volume != m_previousVolumes[channel]);
		m_previousVolumes[channel] = volume;
	}
}

void	LSPEncoder::SetSeqLoop(int seqPos)
{
	assert(seqPos < 128);
	assert(m_seqPosFrame[seqPos] >= 0);
	m_frameLoop = m_seqPosFrame[seqPos];

	if ( m_convertParams.m_verbose )
		printf("Loop, seq=%d (frame=%d)\n", seqPos, m_frameLoop);
}

void	LSPEncoder::SetSeqPos(int seqPos)
{
	assert(seqPos < 128);
	if (m_seqPosFrame[seqPos] < 0)
	{
		if (seqPos > m_seqHighest)
			m_seqHighest = seqPos;

		int secPos = m_totalSampleCount / HOST_REPLAY_RATE;

		const int frame = m_frameCount;
		if ( m_convertParams.m_verbose )
			printf("%02d:%02d | Seq #%2d: frame %d\n", secPos/60, secPos%60, seqPos, frame);
		m_seqPosFrame[seqPos] = frame;
	}
}

void	LSPEncoder::NoteOn(int channel, int instrument, int sampleOffsetInBytes, bool DMAConReset)
{
	if (!DMAConReset)
	{
		if (instrument == m_previousInstrument[channel])
			return;		// if same instrument without note-on then it's 3xx or 5xx effect

		m_sampleWithoutANote = true;
	}

	m_previousInstrument[channel] = instrument;

	if (m_channelMaskFilter & (1 << channel))
	{
		assert(unsigned(channel) < unsigned(MOD_CHANNEL_COUNT));
		assert((instrument > 0) && (instrument <= 31));
		const LspSample& modInfo = m_lspSamples[instrument - 1];

		if (modInfo.len > 0)
		{
			if (sampleOffsetInBytes > 0)
				m_sampleOffsetUsed = true;

			m_ChannelRowData[channel][m_frameCount].instrument = instrument;
			m_ChannelRowData[channel][m_frameCount].sampleOffsetInBytes = sampleOffsetInBytes;
			m_ChannelRowData[channel][m_frameCount].dmaRestart = DMAConReset;
			m_modInstrumentUsedMask |= 1 << (instrument - 1);
		}
		else
		{
			if (m_convertParams.m_verbose)
				printf("Warning: Playing an EMPTY sample instrument (#%d)\n", instrument);
		}
	}
}

void	LSPEncoder::SetOriginalModSoundBank(int moduleFileSoundBankOffset, int size)
{
	m_originalModSoundBank = (s8*)m_ModBuffer + moduleFileSoundBankOffset;
	m_originalModSoundBankSize = size;
}

void	LSPEncoder::SetModInstrumentInfo(int instr, s8* modSampleBank, int start, int len, int repStart, int repLen)
{

	if (m_convertParams.m_verbose)
	{
		const unsigned char* us = (const unsigned char*)(modSampleBank + start);
		printf("MOD Instr #%2d: start=$%06x len=$%05x, repstart=$%05x replen=%05x | ", instr, start, len, repStart, repLen);
		const int dlen = (len > 8) ? 8 : len;
		for (int i = 0; i < dlen; i++)
			printf("%02x ", us[i]);
		if (len > 8)
			printf("...");
		printf("\n");
	}
	assert((instr > 0) && (instr <= 31));
	assert(repStart + repLen <= len);
	LspSample& info = m_lspSamples[instr - 1];
	info.soundBankOffset = start+4;					// bug fix! even with "-nosampleoptim" we should add '4' because of the LSBANK magic 4bytes header
	info.sampleData = (s8*)malloc(len);
	memcpy(info.sampleData, modSampleBank + start, len);
	info.len = len;
	info.repStart = repStart;
	info.repLen = repLen;
	info.maxReplayRate = 0;
	info.resampleMaxLen = 0;
	info.sampleOffsetMax = 0;

	if (repLen < 2)
		repLen = 2;

	if ((len >= 2) && (repLen <= 2))
	{	// Some modules have bad sample start, expecting amiga player clear the first word ( loop )
		assert(0 == repStart);			// replen<2 means one shot sample, generally repstart=0
		u8 byte0 = info.sampleData[repStart];
		u8 byte1 = info.sampleData[repStart+1];
		if (byte0 || byte1)
		{
			if (m_convertParams.m_verbose)
				printf("Warning: MOD Instrument #%d is non looping and wave two first bytes are not $00 ($%02x $%02x)\n", instr, byte0, byte1);

			if (!m_convertParams.m_keepModSoundBankLayout)
			{
				if (m_convertParams.m_verbose)
					printf("  Fixing by clearing two first bytes...\n");

				info.sampleData[repStart] = 0;
				info.sampleData[repStart + 1] = 0;
			}
		}
	}
}

void	LSPEncoder::SetSampleReplayRate(int modSampleId, int replayRate)
{
	assert((modSampleId >= 1) && (modSampleId <= 31));
	if (replayRate > PAULA_REPLAY_RATE_MAX)
		replayRate = PAULA_REPLAY_RATE_MAX;
	LspSample& info = m_lspSamples[modSampleId - 1];
 	if (replayRate > info.maxReplayRate)
 		info.maxReplayRate = replayRate;
}

void	LSPEncoder::SetSampleFetch(int modSampleId, int offset)
{
	assert((modSampleId >= 1) && (modSampleId <= 31));
	LspSample& info = m_lspSamples[modSampleId - 1];
	if (offset + 1 > info.resampleMaxLen)
		info.resampleMaxLen = offset + 1;
}

void	LSPEncoder::SetBPM(int bpm)
{
	if (bpm != m_bpm)
	{
		const int tickRate = (bpm * 2) / 5;
		m_RowData[m_frameCount].bpm = bpm;
		if ( m_convertParams.m_verbose )
			printf("Set BPM to %d (%dHz)\n", bpm, tickRate);
		m_setBpmCount++;
		m_bpm = bpm;
		if (tickRate < m_minTickRate)
			m_minTickRate = tickRate;
	}
}

void	LSPEncoder::DisplayInfos()
{
	printf("MOD File........: %3d KiB\n", (m_ModFileSize + 1023) >> 10);
	printf("  MOD Samples...: %d bytes\n", m_originalModSoundBankSize);
	printf("  MOD Score.....: %d bytes\n", m_MODScoreSize);

	if (m_setBpmCount > 1)
		printf("  BPM changed %d times during MOD!\n", m_setBpmCount);
	else
		printf("  Main BPM......: %d (%dHz)\n", m_bpm, (m_bpm * 2) / 5);

	if ((m_setBpmCount > 1) || (m_bpm != 125))
		printf("  Warning: Non conventional BPM, use CIA player!\n");

	if (m_convertParams.m_verbose)
	{
		printf("  Sample offset: %s\n", m_sampleOffsetUsed ? "Yes" : "No");
		printf("  Sequence count: #%d\n", m_seqHighest+1);
	}
}


static int	ComputeCmdSize(int value)
{
	const int h = value / 255;
	return h + 1;
}

static	void	StoreIntoCmdStream(MemoryStream& stream, int value)
{
	int h = value / 255;
	int l = value % 255;
	for (int i = 0; i < h; i++)
		stream.Add8(0);

	stream.Add8(l+1);
}

static int	ComputeCodesTableSize(int codesCount)
{
	int h = codesCount / 255;
	int l = codesCount % 255;
	return h * 256 + l + 1;
}

int		LSPEncoder::ComputeLSPMusicSize(int dataStreamSize) const
{
	int size = 4;	// LSP1 or LSPm
	size += 4;	// uniqueid
	size += 2;		// major&minor version
	assert(10 == size);		// if not true then change reloc offset in insane generated code
	size += 2;		// reloc done flag
	size += 2;		// music BPM
	size += 2;		// esc code for rewind
	size += 2;		// esc code for setbpm
	size += 2;		// esc code for getpos
	size += 4;		// tick count
	size += 2;		// LSP instrument count
	size += m_lspIntrumentEncoder.GetCodesCount() * 12;
	size += 2;		// codes count value
	size += ComputeCodesTableSize(m_cmdEncoder.GetCodesCount()) * 2;
	size += 2;		// seq count ( should always be 0 in "insane mode" because no SetPos support )
	size += 4;		// word stream size
	size += 4;		// byte stream loop point
	size += 4;		// word stream loop point
	size += dataStreamSize;
	return size;
}

void	LSPEncoder::LspSample::ExtendSample(int addedSampleCount)
{
	assert(addedSampleCount > 0);
	assert(repLen >= 2);
	int addedLoopCount;

	// note: even a non looping sound has a 2 bytes dummy loop
	addedLoopCount = (addedSampleCount + repLen - 1) / repLen;
	addedSampleCount = addedLoopCount * repLen;

	sampleData = (s8*)realloc(sampleData, len + addedSampleCount);

	for (int i = 0; i < addedSampleCount; i++)
		sampleData[len + i] = sampleData[repStart+(i%repLen)];

	len += addedSampleCount;
}

void	LSPEncoder::ComputeAndFixSampleOffsets()
{
	if (!m_convertParams.m_keepModSoundBankLayout)
	{
		assert(m_minTickRate > 0);
		int bankOffset = 4;					// 4 to store first magic bank 32bits number
		for (int i = 0; i < 31; i++)
		{
			if (m_modInstrumentUsedMask & (1 << i))
			{
				LspSample& info = m_lspSamples[i];
				info.soundBankOffset = bankOffset;

				if (m_convertParams.m_shrink)
				{
					// first reduce any "replen overrun"
					if (info.repStart + info.repLen > info.len)
					{
						int len2 = info.repStart + info.repLen;
						printf("Instrument #%02d: Len overrun RepLen. Shrinking from %d to %d bytes\n", i+1, info.len, len2);
						info.len = len2;
					}

					// then reduce any "non sampled byte"
					if (info.resampleMaxLen > 0)
					{
						int cmpLen = (info.resampleMaxLen * (100 + kResampleShrinkMarginPercent)) / 100;
						if (info.len > cmpLen)
						{
							int len2 = (cmpLen + 1)&(-2);		// always even size
							printf("Instrument #%02d: Sample not fully replayed. Shrinking from %d to %d bytes\n", i+1, info.len, len2);
							info.len = len2;
							if (info.repStart + info.repLen > len2)
							{
								assert(info.repStart < len2);
								info.repLen = len2 - info.repStart;
							}
						}
					}

				}

				int minSampleLen = info.len - info.sampleOffsetMax;
				assert(minSampleLen >= 0);

				int minTickLen = info.maxReplayRate / m_minTickRate;
				minTickLen = (minTickLen * 110) / 100;		// add 10% margin
				if (minTickLen > minSampleLen)
				{
					// fix micro samples
					int sampleToAdd = minTickLen - minSampleLen;
					int oldLen = info.len;
					info.ExtendSample(sampleToAdd);
					if (m_convertParams.m_verbose)
						printf("NOTE: extending micro-sample #%d from %d to %d bytes\n", i + 1, oldLen, info.len);
				}

				bankOffset += info.len;
			}
		}
		m_lspSoundBankSize = bankOffset;
	}
	else
	{
		printf("Warning: -nosampleoptim option used. No micro-sample fix will be applied\n");
		m_lspSoundBankSize = m_originalModSoundBankSize;
	}
}

int	LSPEncoder::FrameToSeq(int frame) const
{
	for (int i = 0; i <= m_seqFinalCount; i++)
	{
		if (frame == m_seqPosFrame[i])
		{
			return i;
		}
	}
	return -1;
}

// 4kasm
//  1 streams: 7788 -> 776
//  4 streams: 7788 -> 552
//	5 streams: 7788 -> 600
// 13 streams: 7788 -> 560
// 13 streams: 21613-> 528


// leonard4kb
//  1 streams: 5546 -> 656
//  4 streams: 5546 -> 536
//	5 streams: 5546 -> 604
// 13 streams: 5546 -> 528
// 16 streams: 17066-> 572

// colombia
//  1 str: 28870 -> 5040
//  4 str: 28870 -> 4284
//  5 str: 28870 -> 4300
// 13 str: 28870 -> 4272
// 16 str: 67465 -> 4068

bool	LSPEncoder::ExportToLSP()
{
	bool ret = true;

	MemoryStream	streams[kMicroModeStreamCount];

	int cmdSize = 0;
	int volSize = 0;
	int instrSize = 0;
	int perSize = 0;

/*
	MemoryStream cmdStream;
	MemoryStream volStream;
	MemoryStream perHStream;
	MemoryStream perLStream;
	MemoryStream instrHStream;
	MemoryStream instrLStream;
*/

	const ConvertParams& params = m_convertParams;

	int previousDmacon = 0;
	int	instrStream = 0;

	m_byteStreamLoopPos = -1;
	m_wordStreamLoopPos = -1;

	int streamCount = 2;

	m_seqFinalCount = (params.m_seqSetPosSupport || params.m_seqGetPosSupport) ? (m_seqHighest + 1) : 0;

	if (MicroMode())
	{
		streamCount = kMicroModeStreamCount;
		for (int voice = 3; voice>=0; voice--)
		{
			for (int frame = 0; frame < m_frameCount; frame++)
			{
				u16 wordCmd = m_RowData[frame].wordCmd;
				const ChannelRowData& data = m_ChannelRowData[voice][frame];

				const int kPerStreamId = 0 + voice;	// period first to be 16bits aligned
				const int kCmdStreamId = 4 + voice;
				const int kVolStreamId = 8 + voice;
				const int kInstStreamId = 12 + voice;

				u8 cmdVoice = 0;
				if (wordCmd & (1 << (voice + 8)))		// volume
					cmdVoice |= (1 << 7);
				if (wordCmd & (1 << (voice + 4)))		// period
					cmdVoice |= (1 << 6);
				if (wordCmd & (1 << (voice + 0)))		// instrument
					cmdVoice |= (1 << 5);

				// handle loop point in "-micro" mode (tricky)
				if ( 3 == voice )
				{
					// the loop point is frame 0 by default. Only need to "backup" loop point if m_frameLoop>0
					if ((m_frameLoop > 0) && (frame == (m_frameLoop-1)))
					{
						// as the player is backuping stream pointers "after" the frame, the backup command should be at frameLoop-1
						cmdVoice |= 2 << 3;		// 10 means "backup loop point"
					}

					// at the end of the stream, put a "restore" command (loop streams are valid)
					if ( frame == (m_frameCount-1))
					{
						cmdVoice |= 3 << 3;		// 11 means "restore loop point" ( music loop )
					}
				}

				streams[kCmdStreamId].Add8(cmdVoice);

				// volume
				if (wordCmd & (1 << (voice + 8)))
				{
					assert(m_ChannelRowData[voice][frame].volume <= 64);
					streams[kVolStreamId].Add8(m_ChannelRowData[voice][frame].volume);
				}

				// period
				if (wordCmd & (1 << (voice + 4)))
				{
					const u16 per = u16(m_ChannelRowData[voice][frame].period);
					streams[kPerStreamId].Add8(per>>8);
					streams[kPerStreamId].Add8(per&255);
				}

				// instrument
				if (wordCmd & (1 << (voice + 0)))
				{
					assert(0 == (data.sampleOffsetInBytes & 255));
					const int sampleOffsetCode = data.sampleOffsetInBytes >> 8;
					assert(sampleOffsetCode < 256);
					assert(data.instrument <= 31);
					int intrValue = ((data.instrument - 1) << 8) | sampleOffsetCode;
					int instrId = m_lspIntrumentEncoder.GetCodeFromValue(intrValue);
					assert((instrId >= 0) && (instrId <= 255));
					assert(data.dmaRestart);			// do not support intrument without note
					streams[kInstStreamId].Add8(instrId);
				}
			}
		}
	}
	else
	{

		for (int frame = 0; frame < m_frameCount; frame++)
		{
			if (frame == m_frameLoop)
			{
				m_byteStreamLoopPos = streams[kByteStreamId].GetSize();
				m_wordStreamLoopPos = streams[kWordStreamId].GetSize();
			}

			if ((params.m_seqSetPosSupport) || (params.m_seqGetPosSupport))
			{
				int seq = FrameToSeq(frame);
				if (seq >= 0)
				{
					m_seqPosByteStream[seq] = streams[kByteStreamId].GetSize();
					m_seqPosWordStream[seq] = streams[kWordStreamId].GetSize();

					if (params.m_seqGetPosSupport)
					{
						int cmd = m_cmdEncoder.GetCodeFromValue(m_EscValueGetPos);
						assert(cmd >= 0);
						StoreIntoCmdStream(streams[kByteStreamId], cmd);
						streams[kByteStreamId].Add8(u8(seq));
						cmdSize += ComputeCmdSize(cmd);
						cmdSize += 1;		// seq pos byte
					}
				}
			}

			const u16 wordCmd = m_RowData[frame].wordCmd;

			// maybe there is a BPM change
			if ((m_RowData[frame].bpm) && (m_setBpmCount > 1))
			{
				int cmd = m_cmdEncoder.GetCodeFromValue(m_EscValueSetBpm);
				assert(cmd >= 0);
				StoreIntoCmdStream(streams[kByteStreamId], cmd);
				assert(m_RowData[frame].bpm < 256);
				streams[kByteStreamId].Add8(u8(m_RowData[frame].bpm));

				cmdSize += ComputeCmdSize(cmd);
				cmdSize += 1;		// bpm byte
			}

			int cmd = m_cmdEncoder.GetCodeFromValue(wordCmd);
			assert(cmd >= 0);
			StoreIntoCmdStream(streams[kByteStreamId], cmd);
			cmdSize += ComputeCmdSize(cmd);

			for (int voice = MOD_CHANNEL_COUNT - 1; voice >= 0; voice--)
			{
				// volume
				if (wordCmd & (1 << (voice + 4)))
				{
					streams[kByteStreamId].Add8(m_ChannelRowData[voice][frame].volume);
					volSize += 1;
				}
			}

			for (int voice = MOD_CHANNEL_COUNT - 1; voice >= 0; voice--)
			{
				// period
				if (wordCmd & (1 << (voice + 0)))
				{
					streams[kWordStreamId].Add16(m_ChannelRowData[voice][frame].period);
					perSize += 2;
				}
			}

			// tricky: for generic player, instrument byte data stream should come in same order as dmacon "add.w dn,dn & carry test" thing ( so D, C, B, A )
			int currentOffset = -12;	// -12 because the first move.l (a1),a0 ( and not move.l (a1)+ )
			for (int voice = MOD_CHANNEL_COUNT - 1; voice >= 0; voice--)
			{
				// 00: nothing
				// 01: reset
				// 10: set instr
				// 11: set instr + dma restart
				int codeVoice = (wordCmd >> (8 + voice * 2)) & 3;

				if (codeVoice & 2)
				{
					const ChannelRowData& data = m_ChannelRowData[voice][frame];

					assert(0 == (data.sampleOffsetInBytes & 255));
					const int sampleOffsetCode = data.sampleOffsetInBytes >> 8;
					assert(sampleOffsetCode < 256);

					int intrValue = ((data.instrument - 1) << 8) | sampleOffsetCode;

					int instrId = m_lspIntrumentEncoder.GetCodeFromValue(intrValue);
					assert(instrId >= 0);

					int offset = instrId * 12 - currentOffset;
					if (!data.dmaRestart)
					{
						// tricky: if sample set without a note, we point on the "repeat" part of the sample
						offset += 6;
					}

					streams[kWordStreamId].Add16(offset);

					instrSize += 2;

					currentOffset += offset + 6;	// 6 because of move.l (a2)+ and move.w (a2)+ if DMARestart code executed
				}
			}
		}
		int rewindCode = m_cmdEncoder.GetCodeFromValue(m_EscValueRewind);
		StoreIntoCmdStream(streams[kByteStreamId], rewindCode);
		cmdSize += ComputeCmdSize(rewindCode);
	}


	ComputeAndFixSampleOffsets();

	if (params.m_generateInsane)
	{
		assert(!MicroMode());
		int streamsSize = 0;
		for (int i = 0; i < streamCount; i++)
			streamsSize += streams[i].GetSize();

		const int lspScoreSize = ComputeLSPMusicSize(streamsSize);

		FILE* hc;
		if (fopen_s(&hc, params.m_sPlayerFilename, "w"))
			return false;
		printf("Writing LSP insane player source code (%s)\n", params.m_sPlayerFilename);

		if (ExportCodeHeader(hc, lspScoreSize, streams[kWordStreamId].GetSize()))
			ExportReplayCode(hc);
		fclose(hc);
	}

	ExportBank(params.m_sBankFilename);
	ExportScore(params, streams, streamCount, MicroMode());

//	assert(lspScoreSize == m_lspScoreSize);

	printf("LSP File........: %3d KiB\n", (m_lspScoreSize + m_lspSoundBankSize + 1023) >> 10);
	printf("  LSP Samples...: %d bytes\n", m_lspSoundBankSize);
	printf("  LSP Score.....: %d bytes\n", m_lspScoreSize);
	printf("  Duration......: %02d:%02d\n", m_modDurationSec / 60, m_modDurationSec % 60);
	printf("  LSP Frames....: %d\n", m_frameCount);
	if (params.m_seqSetPosSupport)
		printf("  SetPos enabled (%d bytes)\n", m_seqFinalCount * 8);
	if (params.m_seqGetPosSupport)
		printf("  GetPos enabled (%d bytes)\n", m_seqFinalCount * 3);	// 2 bytes ESC code + 1 byte seqpos

	if (m_convertParams.m_verbose)
	{
		printf("  Periods count.: %d\n", m_periodEncoder.GetCodesCount());
		printf("  LSP Instruments: %d\n", m_lspIntrumentEncoder.GetCodesCount());
		printf("  Cmd count......: %d\n", m_cmdEncoder.GetCodesCount());
		printf("  Stream details:\n");
		for (int s = 0; s < streamCount; s++)
		{
/*			if (m_convertParams.m_packEstimate)
			{
				int packedSize = ShrinklerCompressEstimate((u8*)streams[s].GetRawBuffer(), streams[s].GetSize());
				const float prc = (float(packedSize) * 100.f) / float(streams[s].GetSize());
				printf("    Stream #%02d: %d bytes -> %d bytes ( %.02f%% )\n", s, streams[s].GetSize(), packedSize, prc);
			}
			else
*/			{
				printf("    Stream #%02d: %d bytes\n", s, streams[s].GetSize());
			}
		}
	}

	if (m_convertParams.m_lspMicro)
		printf("NOTE: -micro option enabled, please use LightSpeedPlayer_Micro.asm replayer!\n");

	if (m_convertParams.m_amigaEmulation)
	{
		LSPDecoder decoder;
		decoder.LoadAndRender(m_convertParams.m_sScoreFilename, m_convertParams.m_sBankFilename, m_convertParams.m_sAmigaWavFilename, m_convertParams.m_verbose, m_convertParams.m_loopPreview);
	}

	if (m_convertParams.m_packEstimate)
	{
		BinaryParser fs;
		if (fs.LoadFromFile(m_convertParams.m_sScoreFilename))
		{
			int size = fs.GetLen();
			u8* data = (u8*)fs.GetBuffer();
			printf("Packing estimation for \"%s\"\n", m_convertParams.m_sScoreFilename);
			int packedSize = ShrinklerCompressEstimate(data, size);
			printf("Packing from %d to %d bytes\n", size, packedSize);
		}
	}

	return ret;
}

bool	LSPEncoder::ExportBank(const char* sfilename)
{
	bool ret = false;
	FILE* h;

	if (0 == fopen_s(&h, sfilename, "wb"))
	{
		w32(h, m_uniqueId);
		if (m_convertParams.m_keepModSoundBankLayout)
		{
			assert(m_lspSoundBankSize == m_originalModSoundBankSize);
			fwrite(m_originalModSoundBank, 1, m_lspSoundBankSize, h);
		}
		else
		{
			for (int i = 0; i < 31; i++)
			{
				if (m_modInstrumentUsedMask & (1 << i))
				{
					const LspSample& info = m_lspSamples[i];
					fwrite(info.sampleData, 1, info.len, h);
				}
			}
		}
		fclose(h);
		ret = true;
	}
	return ret;
}

bool	LSPEncoder::ExportScore(const ConvertParams& params, MemoryStream* streams, int streamCount, bool microMode)
{
	bool ret = false;
	FILE* h;
	if (0 == fopen_s(&h, params.m_sScoreFilename, "wb"))
	{
		if ( microMode)
			w32(h, 'LSPm');
		else
			w32(h, 'LSP1');
		if (!microMode)		// no uniqueid in micro mode
			w32(h, m_uniqueId);
		fputc(LSP_MAJOR_VERSION, h);
		fputc(LSP_MINOR_VERSION, h);
		assert(m_bpm > 0);

		if (!microMode)
		{
			int code = 0;
			code |= int(m_convertParams.m_seqGetPosSupport & 1)<<0;
			code |= int(m_convertParams.m_seqSetPosSupport & 1)<<1;

			w16(h, code);				// relocation byte & seq timing flags
			w16(h, m_bpm);
			w16(h, m_EscValueRewind);
			w16(h, m_EscValueSetBpm);
			w16(h, m_EscValueGetPos);
			w32(h, m_frameCount);
		}
		else
			w16(h, m_bpm);

		const int instrumentCount = m_lspIntrumentEncoder.GetCodesCount();
		w16(h, u16(instrumentCount));

		// store instruments ( 12 bytes padded )
		for (int i = 0; i < instrumentCount; i++)
		{
			const int sampleId = m_lspIntruments[i].lspSampleId;
			assert((sampleId >= 1) && (sampleId <= 31));
			const LspSample& info = m_lspSamples[sampleId-1];
			int lspLen = info.len - m_lspIntruments[i].sampleOffset;
			assert(lspLen >= 2);
			assert(lspLen <= 0xffff*2);

			w32(h, info.soundBankOffset + m_lspIntruments[i].sampleOffset);
			w16(h, lspLen / 2);				// word count
			w32(h, info.soundBankOffset + info.repStart);
			w16(h, info.repLen / 2);
		}

		if (!microMode)
		{
			int tableSize = ComputeCodesTableSize(m_cmdEncoder.GetCodesCount());
			w16(h, tableSize);

			for (int i = 0; i < m_cmdEncoder.GetCodesCount(); i++)
			{
				if (0 == (i % 255))
					w16(h, 0);			// 0 is reserved

				u16 shortCmd = m_cmdEncoder.GetValueFromCode(i);
				w16(h, shortCmd);
			}

			// save seq info
			if (params.m_seqSetPosSupport)
			{
				w16(h, m_seqFinalCount); // seq count
				const int wordStreamSize = streams[kWordStreamId].GetSize();
				for (int i = 0; i < m_seqFinalCount; i++)
				{
					w32(h, u32(m_seqPosWordStream[i]));
					w32(h, u32(m_seqPosByteStream[i])+wordStreamSize);
				}
			}
			else
			{
				w16(h, 0);
			}

			assert(streams[kWordStreamId].GetSize() / 2 < 65536);
			assert(0 == (streams[kWordStreamId].GetSize() & 1));
			w32(h, u32(streams[kWordStreamId].GetSize()));
			w32(h, u32(m_byteStreamLoopPos));
			w32(h, u32(m_wordStreamLoopPos));
		}
		else
		{

			/*
					0 per	0
					1 per	1
					2 per	2
					3 per	3
					0 cmd	4
					1 cmd	5
					2 cmd	6
					3 cmd	7
					0 vol	8
					1 vol	9
					2 vol	10
					3 vol	11
					0 inst	12
					1 inst	13
					2 inst	14
					3 inst	15
			*/
			assert(kMicroModeStreamCount == streamCount);
			int offsets[kMicroModeStreamCount];
			int offset = 0;
			for (int i = 0; i < streamCount; i++)
			{
				offsets[i] = offset;
				offset += streams[i].GetSize();
			}

			// note: streams are ordered so "period" streams come first in the file ( to be even aligned, because of move.w reading )
			static const int ordering[kMicroModeStreamCount] =
			{
				4,5,6,7,			// period streams come first in the file
				8,9,10,11,
				0,1,2,3,
				12,13,14,15
			};
			for (int i = 0; i < streamCount; i++)
				w32(h, offsets[ordering[i]]);
		}

		int baseOffset = ftell(h);
		for (int s = 0; s < streamCount; s++)
		{
			if (m_convertParams.m_verbose)
			{
				printf("Offset $%06x : stream #%d\n", baseOffset, s);
				baseOffset += streams[s].GetSize();
			}
			streams[s].FileWrite(h);
		}

		const int scoreSize = ftell(h);

		m_lspScoreSize = scoreSize;
		fclose(h);
		ret = true;
	}

	return ret;
}

void	LSPEncoder::GenLabel(int word, char* out)
{
	// b2: period
	// b1: volume
	// b0: instrument
	if (word)
	{
		if (m_EscValueRewind == word)
			sprintf(out, "rewind");
		else if (m_EscValueGetPos == word)
			sprintf(out, "getpos");
		else
		{
			if (m_EscValueSetBpm == word)
			{
				sprintf(out, "setBPM");
			}
			else
			{
				char* p = out;
				for (int v = 0; v < 4; v++)
				{
					int pv = (word >> v) & 0x11;
					int voiceCode = (word >> (8 + v * 2)) & 3;
					if ((pv) || (voiceCode))
					{
						*p++ = 'A' + v;
						if (kResetLen == voiceCode) *p++ = 'r';
						if (kPlayWithoutNote == voiceCode) *p++ = 's';
						if (kPlayInstrument == voiceCode) *p++ = 't';
						if (pv & 0x0010) *p++ = 'v';
						if (pv & 0x0001) *p++ = 'p';
					}
				}
				*p++ = 0;
			}
		}
	}
	else
		sprintf(out, "None");
}

struct FetchInfo
{
	int		voice;
	int		voiceCode;
	int		offset;
};

bool	LSPEncoder::ExportReplayCode(FILE* h)
{

	const int codes_count = m_cmdEncoder.GetCodesCount();


	fprintf_s(h, "; %d specific callback\n", codes_count - 1);

	FetchInfo fetchInfo[4];

	// gen the code
	for (int i = 0; i < codes_count; i++)
	{
		int fetchCount = 0;

		if (m_cmdEncoder.IsDummyCodeEntry(i))
			continue;

		int word = m_cmdEncoder.GetValueFromCode(i);
		if (m_EscValueRewind == word)		// no use to generate rewind subroutine
			continue;
		if (m_EscValueSetBpm == word)		// no use to generate setBPM special code
			continue;
		if (m_EscValueGetPos == word)		// no use to generate GetPos code
			continue;

		assert(word < 1 << 16);

		int resetCount = 0;
		int dmaCount = 0;
		int instrCount = 0;
		int dmaCon = 0;
		for (int v = 3; v >= 0; v--)
		{
			int voiceCode = (word >> (8 + v * 2)) & 3;
			if (voiceCode > 0)
			{
				FetchInfo& finfo = fetchInfo[fetchCount];
				finfo.offset = (3-v) * 4;

				finfo.voiceCode = voiceCode;
				finfo.voice = v;
				if (voiceCode == kResetLen)
				{
					resetCount++;
				}
				else if (voiceCode == kPlayWithoutNote)
				{
					instrCount++;
				}
				else if (voiceCode == kPlayInstrument)
				{
					instrCount++;
					dmaCount++;
					dmaCon |= (1 << v);
				}
				fetchCount++;
			}
		}

		char sLabel[128];
		GenLabel(word, sLabel);
		fprintf_s(h, ".r_%s:\n", sLabel);

		const bool dpcA4 = ((resetCount <= 2) && (0 == instrCount));

		// gen volume
		for (int v = 7; v >= 4; v--)
		{
			if (word&(1 << v))
			{
				fprintf_s(h, "\t\tmove.b\t(a0)+,$%02x(a6)\n", (v-4) * 16 + 9);
			}
		}

		fprintf_s(h, "\t\tmove.l\ta0,(a1)+\n");


		const bool needWordStream = (instrCount > 0) || (word & 0xf);	// if instr or periods, need word stream

		if (dmaCount > 0)
		{
			fprintf_s(h, "\t\tmove.l\t(a1)+,a0\n");
			fprintf_s(h, "\t\tmoveq\t#$%02x,d0\n", dmaCon);
			fprintf_s(h, "\t\tmove.w\td0,$96-$a0(a6)\n");
			fprintf_s(h, "\t\tmove.b\td0,(a0)\n");
		}
		else if (needWordStream)
		{
			fprintf_s(h, "\t\taddq.w\t#4,a1\n");
		}

		if ( needWordStream)
			fprintf_s(h, "\t\tmove.l\t(a1),a0\n");

		for (int v = 3; v >= 0; v--)
		{
			if ( word & (1<<v))
				fprintf_s(h, "\t\tmove.w\t(a0)+,$%02x(a6)\n", v * 16 + 6);
		}


		if (fetchCount > 0)
		{
			int currentOffset = fetchInfo[0].offset;
			if (!dpcA4)
			{
				if (currentOffset > 0)
					fprintf_s(h, "\t\tlea\t\t.resetv+%d(pc),a4\n", currentOffset);
				else
					fprintf_s(h, "\t\tlea\t\t.resetv(pc),a4\n");
			}

			if ( instrCount > 0)
				fprintf_s(h, "\t\tmovea.l\ta1,a2\n");

			for (int i = 0; i < fetchCount; i++)
			{
				const FetchInfo& finfo = fetchInfo[i];
				int delta = finfo.offset - currentOffset;
				if (kResetLen == finfo.voiceCode)
				{
					if (dpcA4)
					{
						fprintf(h, "\t\tmove.l\t.resetv+%d(pc),a3\n", finfo.offset);
					}
					else
					{
						if (0 == delta)
						{
							fprintf(h, "\t\tmove.l\t(a4)+,a3\n");
							currentOffset += 4;
						}
						else
						{
							fprintf(h, "\t\tmove.l\t%d(a4),a3\n", delta);
						}
					}
					if (finfo.voice)
						fprintf(h, "\t\tmove.l\t(a3)+,$%x0(a6)\n", finfo.voice);
					else
						fprintf(h, "\t\tmove.l\t(a3)+,(a6)\n");
					fprintf(h, "\t\tmove.w\t(a3)+,$%x4(a6)\n", finfo.voice);
				}
				else
				{
					assert(finfo.voiceCode != kNone);
					fprintf(h, "\t\tadd.w\t(a0)+,a2\n");
					if (finfo.voice)
						fprintf(h, "\t\tmove.l\t(a2)+,$%x0(a6)\n", finfo.voice);
					else
						fprintf(h, "\t\tmove.l\t(a2)+,(a6)\n");
					fprintf(h, "\t\tmove.w\t(a2)+,$%x4(a6)\n", finfo.voice);
					if (finfo.voiceCode == kPlayInstrument)
					{
						if (0 == delta)
						{
							fprintf(h, "\t\tmove.l\ta2,(a4)+\n");
							currentOffset += 4;
						}
						else
						{
							fprintf(h, "\t\tmove.l\ta2,%d(a4)\n", delta);
						}
					}
				}
			}
		}

		if (needWordStream)
			fprintf_s(h, "\t\tmove.l\ta0,(a1)\n");

		fprintf_s(h, "\t\trts\n\n");
	}

	return true;
}

static int	toKiB(int v)
{
	return (v + 1023) >> 10;
}

static void	emitLea(FILE* h, int offset, int rs, int rd, const char* comment)
{
	if ( offset != 0 )
	{
		if ((offset >= -32768) && (offset <= 32767))
			fprintf_s(h, "\t\t\tlea\t\t%d(a%d),a%d", offset, rs, rd);
		else
		{
			if ( rs != rd )
				fprintf_s(h, "\t\t\tmovea.l\ta%d,a%d\n", rs, rd);
			fprintf_s(h, "\t\t\tadd.l\t#%d,a%d", offset, rd);
		}

		if ( comment )
			fprintf_s(h, "\t; %s", comment);

		fprintf_s(h, "\n");
	}
}

bool	LSPEncoder::ExportCodeHeader(FILE* h, int lspScoreSize, int wordStreamSize)
{
	const ConvertParams& params = m_convertParams;

	fprintf_s(h, ";*****************************************************************\n");
	fprintf_s(h,";\n"
		";\tLight Speed Player v%d.%02d\n"
		";\tFastest Amiga MOD player ever :)\n"
		";\tWritten By Arnaud Carré (aka Leonard / OXYGENE)\n"
		";\thttps://github.com/arnaud-carre/LSPlayer\n"
		";\ttwitter: @leonard_coder\n"
		";\n"
		";\t\"Insane speed\" player version ( half scanline average player time! )\n"
		";\t(this source code is generated by LSPConvert)\n"
		";\tYou can also use the generic LightSpeedPlayer.asm player ( less than 512bytes! )\n"
		";\n",
		LSP_MAJOR_VERSION,
		LSP_MINOR_VERSION);

		const int lspInstrumentCount = m_lspIntrumentEncoder.GetCodesCount();

		fprintf_s(h, ";\t*WARNING* This generated source code specific to \"%s\" LSP file\n",params.m_sScoreFilename);

		fprintf_s(h, ";\n");
		fprintf_s(h, ";\t--------How to use--------- \n");
		fprintf_s(h, ";\n");
		fprintf_s(h, ";\tbsr LSP_MusicInitInsane : Init LSP player code & music\n");
		fprintf_s(h, ";\t\ta0: LSP music data(any memory)\n");
		fprintf_s(h, ";\t\ta1: LSP sound bank(chip memory)\n");
		fprintf_s(h, ";\t\ta2: DMACON 8bits low byte address (odd)\n");
		fprintf_s(h, ";\n");
		fprintf_s(h, ";\tbsr LSP_MusicPlayTickInsane : LSP player tick (call once per frame)\n");
		fprintf_s(h, ";\t\ta6: should be $dff0a0 (and not $dff000)\n");
		fprintf_s(h, ";\t\tUsed regs: d0/a0/a1/a2/a3/a4\n");
		fprintf_s(h, ";\n");
		fprintf_s(h, ";*****************************************************************\n");

		fprintf_s(h, "\n"
			"LSP_MusicInitInsane:\n"
			"\t\t\tmove.l\t#$%08x,d0\n"
			"\t\t\tcmp.l\t(a1),d0\n"
			"\t\t\tbne.s\t.dataError\n"
			"\t\t\tcmpi.l\t#'LSP1',(a0)+\n"
			"\t\t\tbne.s\t.dataError\n"
			"\t\t\tcmp.l\t(a0)+,d0\n"
			"\t\t\tbne.s\t.dataError\n", m_uniqueId);

		const int skip = ComputeLSPMusicSize(0) - 8;	// already read 8 bytes ( LSP1 + unique id )

		fprintf_s(h, "\t\t\tlea\t\t2(a0),a5\t\t; relocation byte\n");
		fprintf_s(h, "\t\t\tlea\t\t%d(a0),a0\t\t; skip header\n", skip);

		fprintf_s(h,
			"\t\t\tlea\t\tLSP_StateInsane(pc),a3\n"
			"\t\t\tmove.l\ta2,12(a3)\n"
			"\t\t\tmove.l\ta0,16(a3)\t\t; word stream ptr\n");

		emitLea(h, wordStreamSize, 0, 4, nullptr);
		fprintf_s(h, "\t\t\tmove.l\ta4,8(a3)\t\t; byte stream ptr\n\n");

		emitLea(h, m_wordStreamLoopPos, 0, 0, "word stream loop pos");
		fprintf_s(h, "\t\t\tmove.l\ta0,(a3)\t; word stream loop ptr\n");

		emitLea(h, m_byteStreamLoopPos, 4, 4, "byte stream loop pos");
		fprintf_s(h, "\t\t\tmove.l\ta4,24(a3)\t; byte stream loop ptr\n");



		fprintf_s(h,
			"\t\t\ttst.b\t(a5)\n"
			"\t\t\tbne.s\t.noReloc\n"
			"\t\t\tst\t\t(a5)\n");

		if (lspInstrumentCount < 128)
			fprintf_s(h, "\t\t\tmoveq\t#%d-1,d0\n", lspInstrumentCount);
		else
			fprintf_s(h, "\t\t\tmove.w\t#%d-1,d0\n", lspInstrumentCount);


		fprintf_s(h, "\t\t\tlea\t\tLSP_InstrumentInfoInsane(pc),a0\n"
			"\t\t\tmove.l\ta1,d1\n"
			".rloop:\t\tadd.l\td1,(a0)\n"
			"\t\t\tadd.l\td1,6(a0)\n"
			"\t\t\tlea\t\t12(a0),a0\n"
			"\t\t\tdbf\t\td0,.rloop\n"
			".noReloc:\tbset.b\t#1,$bfe001\t; disable this fucking Low pass filter!!\n");

		fprintf_s(h, "\t\t\tlea\t\tLSP_StateInsane+6(pc),a0\n");
		fprintf_s(h, "\t\t\tmove.w\t#%d,(a0)\t\t; music BPM\n", m_bpm);

		fprintf_s(h, "\t\t\trts\n\n");

		fprintf_s(h,".dataError:\tillegal\n\n");

		fprintf_s(h, "LSP_MusicGetPos:\n");
		if ( params.m_seqGetPosSupport )
			fprintf_s(h, "\t\t\tmove.w\tLSP_CurrentPos(pc),d0\n");
		else
			fprintf_s(h, "\t\t\tmoveq\t#0,d0\t\t; (music have been generated without \"-getpos\" support)\n");
		fprintf_s(h, "\t\t\trts\n\n");

		if (params.m_seqGetPosSupport)
			fprintf_s(h, "LSP_CurrentPos:\t\tdc.w\t0\n");

		// gen LSPVars
		fprintf_s(h,
			"LSP_StateInsane:\tdc.l\t0\t\t\t; 0  word stream loop\n"
			"\t\t\t\t\tdc.w\t0\t\t\t; 4  reloc has been done\n"
			"\t\t\t\t\tdc.w\t0\t\t\t; 6  current music BPM\n"
			"\t\t\t\t\tdc.l\t0\t\t\t; 8  byte stream\n"
			"\t\t\t\t\tdc.l\t0\t\t\t; 12 m_lfmDmaConPatch\n"
			"\t\t\t\t\tdc.l\t0\t\t\t; 16 word stream\n"
			"\t\t\t\t\tdc.l\t0\t\t\t; 20 word stream loop\n"
			"\t\t\t\t\tdc.l\t0\t\t\t; 24 byte stream loop\n"
			"\n");

		fprintf(h, "; WARNING: in word stream, instrument offset is shifted by -12 bytes (3 last long of LSP_StateInsane)\n");

		fprintf_s(h, "LSP_InstrumentInfoInsane:\t\t\t; (%d instruments)\n", lspInstrumentCount);

		// gen sampleInfo
		for (int i = 0; i < lspInstrumentCount; i++)
		{
			const int smpId = m_lspIntruments[i].lspSampleId;
			assert((smpId >= 1) && (smpId <= 31));
			const LspSample& info = m_lspSamples[smpId - 1];
			assert(info.repLen > 1);
			const u32 startAd = info.soundBankOffset + m_lspIntruments[i].sampleOffset;
			const u32 endAd = info.soundBankOffset + info.repStart;

			int lspLen = info.len - m_lspIntruments[i].sampleOffset;
			assert(lspLen >= 2);
			assert(lspLen <= 0xffff * 2);

			fprintf_s(h, "\t\t\tdc.w\t$%04x,$%04x,$%04x,$%04x,$%04x,$%04x\n",
				startAd >> 16,
				startAd & 0xffff,
				lspLen / 2,
				endAd >> 16,
				endAd & 0xffff,
				info.repLen / 2);
		}

		fprintf_s(h, "\n");



		const int hc = m_cmdEncoder.GetCodesCount() / 255;

		fprintf_s(h, "LSP_MusicPlayTickInsane:\n");
		fprintf_s(h, "\t\t\tlea\t\tLSP_StateInsane+8(pc),a1\n");
		fprintf_s(h, "\t\t\tmove.l\t(a1),a0\t\t; byte stream\n");
		fprintf_s(h, ".process:\tmoveq\t#0,d0\n");
		fprintf_s(h, "\t\t\tmove.b\t(a0)+,d0\n");
		if ( hc>0 )
			fprintf_s(h, "\t\t\tbeq.s\t.extended1\n");
		fprintf_s(h, "\t\t\tadd.w\td0,d0\n");
		fprintf_s(h, "\t\t\tmove.w\t.LSP_JmpTable(pc,d0.w),d0\t; 14 cycles\n");
		fprintf_s(h, "\t\t\tjmp\t\t.LSP_JmpTable(pc,d0.w)\t\t; 14 cycles\n");
		fprintf_s(h, "\n");

		if (hc>0)
		{
			fprintf_s(h, ".extended1:\tmove.w\t#$0100,d0\n");
			fprintf_s(h, "\t\t\tmove.b\t(a0)+,d0\n");
			if (hc > 1)
				fprintf_s(h, "\t\t\tbeq.s\t.extended2\n");
			fprintf_s(h, "\t\t\tadd.w\td0,d0\n");
			fprintf_s(h, "\t\t\tmove.w\t.LSP_JmpTable(pc,d0.w),d0\n");
			fprintf_s(h, "\t\t\tjmp\t\t.LSP_JmpTable(pc,d0.w)\n");
			fprintf_s(h, "\n");
		}

		if (hc > 1)
		{
			fprintf_s(h, ".extended2:\tmove.w\t#$0200,d0\n");
			fprintf_s(h, "\t\t\tmove.b\t(a0)+,d0\n");
			fprintf_s(h, "\t\t\tadd.w\td0,d0\n");
			fprintf_s(h, "\t\t\tmove.w\t.LSP_JmpTable(pc,d0.w),d0\n");
			fprintf_s(h, "\t\t\tjmp\t\t.LSP_JmpTable(pc,d0.w)\n");
			fprintf_s(h, "\n");
		}

		fprintf_s(h,
			".r_rewind:\tmove.l\t0-8(a1),16-8(a1)\n"
			"\t\t\tmove.l\t24-8(a1),a0\n"
			"\t\t\tbra.s\t.process\n\n");


		if (params.m_seqGetPosSupport)
		{
			fprintf_s(h,".r_getpos:\tmove.b\t(a0)+,-9(a1)\t; patch LSP_CurrentPos low byte\n"
			          	"\t\t\tbra.s\t.process\n\n");
		}

		if (m_setBpmCount > 1)
		{
			fprintf_s(h,
				".r_setBPM:\tmove.b\t(a0)+,-1(a1)\t; patch BPM byte\n"
				"\t\t\tbra.s\t.process\n\n");
		}

		fprintf_s(h, ".resetv:\tdc.l\t0,0,0,0\n");


		const int codes_count = m_cmdEncoder.GetCodesCount();
		fprintf_s(h, "\n.LSP_JmpTable:\t\t; (%d codes)\n", codes_count);
		for (int i = 0; i < codes_count; i++)
		{
			if (0 == (i % 255))
			{
				fprintf_s(h, "\t\t\tdc.w\t-1\t\t; extended code\n");
			}
			if (m_cmdEncoder.IsDummyCodeEntry(i))
			{
				fprintf_s(h, "\t\t\tdc.w\t$0000\t\t; dummy code\n");
			}
			else
			{
				int word = m_cmdEncoder.GetValueFromCode(i);
				assert(word >= 0);
				char sLabel[128];
				bool genLabel = true;
				if (word == m_EscValueSetBpm)
				{
					if (m_setBpmCount <= 1)
					{
						fprintf_s(h, "\t\t\tdc.w\t$0000\t\t; SetBpm code (not used in this music)\n");
						genLabel = false;
					}
				}
				if (word == m_EscValueGetPos)
				{
					if (!params.m_seqGetPosSupport)
					{
						fprintf_s(h, "\t\t\tdc.w\t$0000\t\t; no GetPos (not supported in insane player)\n");
						genLabel = false;
					}
				}

				if ( genLabel )
				{
					GenLabel(word, sLabel);
					fprintf_s(h, "\t\t\tdc.w\t.r_%s-.LSP_JmpTable\n", sLabel);
				}
			}
		}
		fprintf_s(h, "\n");

	return true;
}