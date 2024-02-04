/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/

// Emulate LSP 68k player behavior (using PAULA emulation) to check everything's fine on your PC
// ( -amigapreview command line option )

#define	_CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include "LSPDecoder.h"
#include "WavWriter.h"
#include "Paula.h"


BinaryParser::BinaryParser()
{
	m_data = NULL;
	m_pos = -1;
	m_allocated = false;
}

BinaryParser::~BinaryParser()
{
	Unload();
}

void	BinaryParser::Unload()
{
	if ( m_allocated )
		free(m_data);
	m_data = NULL;
	m_allocated = false;
}

bool	BinaryParser::LoadFromMemory(const void* data, int maxLen, int startOffset /* = 0 */)
{
	Unload();
	assert(startOffset <= maxLen);
	m_data = (u8*)malloc(maxLen);
	memcpy(m_data, data, maxLen);
	m_dataLen = maxLen;
	m_pos = startOffset;
	m_allocated = true;
	return true;
}

bool	BinaryParser::SetMemoryView(const void* data, int len)
{
	m_data = (u8*)data;
	m_allocated = false;
	m_pos = 0;
	m_dataLen = len;
	return true;
}

bool	BinaryParser::LoadFromFile(const char* sFilename, int startOffset /* = 0 */)
{
	Unload();
	bool ret = false;
	FILE* h = fopen(sFilename, "rb");
	if (h)
	{
		fseek(h, 0, SEEK_END);
		m_dataLen = ftell(h);
		fseek(h, 0, SEEK_SET);
		m_data = (u8*)malloc(m_dataLen);
		fread(m_data, 1, m_dataLen, h);
		fclose(h);
		m_pos = startOffset;
		ret = true;
	}
	return ret;
}


u8	BinaryParser::ru8()
{
	assert(m_pos < m_dataLen);
	return m_data[m_pos++];
}

u16	BinaryParser::ru16()
{
	assert(0 == (m_pos & 1));
	u16 data = (u16(ru8()) << 8);
	data |= ru8();
	return data;
}

u32	BinaryParser::ru32()
{
	u32 data = (u32(ru16()) << 16);
	data |= ru16();
	return data;
}

s16	BinaryParser::rs16()
{
	return s16(ru16());
}

void	BinaryParser::skip(int len)
{
	assert(m_pos + len <= m_dataLen);
	m_pos += len;
}

LSPDecoder::LSPDecoder()
{
}

u16	LSPDecoder::ReadNextCmd(BinaryParser& parser)
{
	int idx = 0;
	for (;;)
	{
		u8 b = parser.ru8();
		idx |= b;
		if (b)
			break;
		idx += 256;
	}
	assert(idx < m_codesCount);
	return m_codes[idx];
}

static const int	BpmToSampleCount(int bpm)
{
	return (HOST_REPLAY_RATE * 5) / (bpm * 2);
}

bool	LSPDecoder::LoadAndRender(const char* sMusicName, const char* sBankName, const char* sOutputWavFile, bool verbose)
{

	BinaryParser musicFile;
	BinaryParser bankFile;

	WavWriter paulaOutput;
	paulaOutput.Open(sOutputWavFile, HOST_REPLAY_RATE, 2);

	Paula paulaChip(HOST_REPLAY_RATE);

	printf("Reading back LSP files:\n"
		"  Score: \"%s\"\n"
		"  Bank.: \"%s\"\n",
		sMusicName, sBankName);

	bool ret = false;
	if (musicFile.LoadFromFile(sMusicName))
	{
		u32 lsMusicMaxSize = musicFile.GetLen();
		if (bankFile.LoadFromFile(sBankName))
		{

			paulaChip.UploadChipMemoryBank(bankFile.GetBuffer(), bankFile.GetLen(), 0);		// upload at ad 0

			u32 sign = musicFile.ru32();

			if ((sign != 'LSP1') && (sign != 'LSPm'))
			{
				printf("ERROR: not a valid LSP music file\n");
				return false;
			}

			const bool microMode = (sign == 'LSPm');

			u32 bnkMagic = bankFile.ru32();
			u32 magic = bnkMagic;
			if ( !microMode )
				magic = musicFile.ru32();

			if (magic == bnkMagic)
			{

				u16 version = musicFile.ru16();		// major/minor version
				printf("Version: $%04x\n", version);

				u16 bpm = 125;
				if (!microMode)
				{
					musicFile.ru16();		// skip relocating flag
					bpm = musicFile.ru16();
					m_escCodeRewind = musicFile.ru16();
					m_escCodeSetBpm = musicFile.ru16();
					m_escCodeGetPos = musicFile.ru16();
				}

				printf("Main BPM: %d\n", bpm);

				int frameSampleCount = BpmToSampleCount(bpm);
				m_frameCount = 0;

				if (!microMode)
					m_frameCount = musicFile.ru32();

				m_instrumentCount = musicFile.ru16();
				printf("LSP Instruments: %d\n", m_instrumentCount);
				m_halfInstruments = (LSPHalfInstrument*)malloc((m_instrumentCount * 2) * sizeof(LSPHalfInstrument));	// *2 because half instrument (just pos/len) ( +1 because last half could be read by player)
				for (int i = 0; i < m_instrumentCount; i++)
				{
					m_halfInstruments[i*2].pos = musicFile.ru32();
					m_halfInstruments[i*2].len = musicFile.ru16();
					m_halfInstruments[i*2+1].pos = musicFile.ru32();
					m_halfInstruments[i*2+1].len = musicFile.ru16();
					if (verbose)
					{
						printf("LSP instrument #%3d : %08x|%04x|%08x|%04x\n", i, m_halfInstruments[i*2].pos,
							m_halfInstruments[i*2].len,
							m_halfInstruments[i*2+1].pos,
							m_halfInstruments[i*2+1].len);
					}
				}

				int streamsOffsets[16] = {};

				if (microMode)
				{
					m_codesCount = -1;
					for (int i = 0; i < 16; i++)
						streamsOffsets[i] = musicFile.ru32();
				}
				else
				{
					m_codesCount = musicFile.ru16();
					printf("LSP codes......: %d\n", m_codesCount);
					m_codes = (u16*)malloc(m_codesCount * sizeof(u16));
					for (int i = 0; i < m_codesCount; i++)
						m_codes[i] = musicFile.ru16();

					int seqTimingCount = musicFile.ru16();
					musicFile.skip(seqTimingCount * 8);

					m_wordStreamSize = musicFile.ru32();
					m_byteStreamLoop = musicFile.ru32();
					m_wordStreamLoop = musicFile.ru32();
				}

				BinaryParser streams[16];
				int streamCount = 2;
				const u8* p = (const u8*)musicFile.GetReadPtr();
				if (microMode)
				{
					streamCount = 16;
					for (int s = 0; s < 16; s++)
						streams[s].SetMemoryView(p + streamsOffsets[s], musicFile.GetLen() - streamsOffsets[s]);	// we don't have the stream size so use dummy higher value
				}
				else
				{
					streams[0].LoadFromMemory(p, m_wordStreamSize);
					const int byteStreamSize = musicFile.GetLen() - (musicFile.GetPos() + m_wordStreamSize);
					streams[1].LoadFromMemory(p + m_wordStreamSize, byteStreamSize);
				}

				u32 nextAd[4] = {};
				u16 nextLen[4] = {};

				printf("Simulating LSP Amiga  player & Paula in \"%s\"\n", sOutputWavFile);

				AudioBuffer tmpBuffer(2);

				u32 totalSampleCount = 0;
				int frame = 0;
				u16 prevDmaCon = 0;

				LSPHalfInstrument reset[4] = {};
				bool endOfSong = false;

				while (!endOfSong)
				{
					if (microMode)
					{
						u16 dmaCon = 0;
						for (int v = 0; v < 4; v++)
						{
							if (prevDmaCon&(1 << v))
							{
								paulaChip.SetSampleAd(v, reset[v].pos);
								paulaChip.SetLen(v, reset[v].len);
							}

							u8 vCmd = streams[v+0].ru8();

							if (vCmd&(1 << 7))	// volume
							{
								u8 vol = streams[v + 4].ru8();
								assert(vol <= 64);
								paulaChip.SetVolume(v, vol);
							}
							if (vCmd&(1 << 6))	// period
							{
								assert(0 == (streams[v + 8].GetPos() & 1));
								u16 per = streams[v+8].ru8();
								per = (per<<8) | streams[v+8].ru8();
								paulaChip.SetPeriod(v, per);
							}
							if (vCmd&(1 << 5))	// instrument
							{
								int instrId = streams[v + 12].ru8();
								assert(instrId < m_instrumentCount);
								dmaCon |= 1 << v;
								const LSPHalfInstrument* instr = m_halfInstruments + instrId * 2;
								paulaChip.SetSampleAd(v, instr[0].pos);
								paulaChip.SetLen(v, instr[0].len);
								reset[v] = instr[1];
							}

							// end of song
							if (3 == ((vCmd >> 3) & 3))
								endOfSong = true;

						}
						paulaChip.WriteDmaCon(dmaCon);
						paulaChip.WriteDmaCon(dmaCon | 0x8000);
						prevDmaCon = dmaCon;
					}
					else
					{
						// normal mode
						const u16 cmd = ReadNextCmd(streams[1]);

						if (m_escCodeRewind == cmd)
						{
							assert(frame == m_frameCount);
							//						printf("End of transmission\n",frame);
							break;
						}

						assert(frame < m_frameCount);
						if (m_escCodeSetBpm == cmd)
						{
							bpm = streams[1].ru8();
							frameSampleCount = BpmToSampleCount(bpm);
							//							printf("setBPM(%d)\n", bpm);
						}
						else if (m_escCodeGetPos == cmd)
						{
							// skip getpos byte
							streams[1].ru8();
						}
						else
						{

							//					printf("Frame %d: cmd:$%04x ",frame,cmd);

												// 12..15	set replen
												// 8..11	volume
												// 4..7		periods
												// 0..3		dmacon

												// setreplen

							// volumes
							for (int b = 7; b >= 4; b--)
							{
								if (cmd & (1 << b))
								{
									u8 vol = streams[1].ru8();
									paulaChip.SetVolume(b - 4, vol);
								}
							}

							// periods
							for (int b = 3; b >= 0; b--)
							{
								if (cmd & (1 << b))
								{
									u16 per = streams[0].ru16();
									paulaChip.SetPeriod(b - 0, per);
									//							printf("%04x|", per);
								}
							}

							// voices
							int ioffset = -12;
							int dmaCon = 0;
							for (int v = 3; v >= 0; v--)
							{
								int voiceCode = (cmd >> (8 + v * 2)) & 3;

								if (voiceCode > 0)
								{
									if (1 == voiceCode)
									{
										assert(nextAd[v]);
										assert(nextLen[v]);
										paulaChip.SetSampleAd(v, nextAd[v]);
										paulaChip.SetLen(v, nextLen[v]);
									}
									else
									{
										ioffset += streams[0].rs16();
										if (3 == voiceCode)
										{
											dmaCon |= 1 << v;
											paulaChip.WriteDmaCon(dmaCon);	// switch off DMA
											assert(0 == (ioffset % 12));
										}
										else
										{
											assert(2 == voiceCode);
											assert(0 == (ioffset % 6));
										}
										int halfInstrId = ioffset / 6;
										assert(halfInstrId < m_instrumentCount * 2);
										paulaChip.SetSampleAd(v, m_halfInstruments[halfInstrId].pos);
										paulaChip.SetLen(v, m_halfInstruments[halfInstrId].len);
										if (voiceCode & 1)
										{
											nextAd[v] = m_halfInstruments[halfInstrId + 1].pos;
											nextLen[v] = m_halfInstruments[halfInstrId + 1].len;
											assert(dmaCon & (1 << v));
										}
										else
										{
											nextAd[v] = 0;
											nextLen[v] = 0;
										}

										ioffset += 6;			// the player use move.l (a2)+ and move.w (a2)+
									}
								}
							}

							paulaChip.WriteDmaCon(dmaCon | 0x8000);
						}
					}
					// now we can render a complete paula frame
					s16* buffer = tmpBuffer.GetAudioBuffer(frameSampleCount);
					paulaChip.AudioStreamRender(buffer, frameSampleCount);
					paulaOutput.AddAudioData(buffer, frameSampleCount);
					frame++;

					if ((!microMode) && (frame >= int(m_frameCount)))
						endOfSong = true;


					totalSampleCount += frameSampleCount;
				}
				printf("End of streams. ( %d frames )\n", frame);
				static const int seconds = totalSampleCount / HOST_REPLAY_RATE;
				printf("Music duration: %dm%02ds\n", seconds / 60, seconds % 60);
			}
			else
			{
				printf("ERROR: lsmusic & lsbank magic value does NOT match!\n");
			}
		}
		else
		{
			printf("ERROR: Unable to load \"%s\"\n", sBankName);
		}
	}
	else
	{
		printf("ERROR: Unable to load \"%s\"\n", sMusicName);
	}
	return ret;
}
