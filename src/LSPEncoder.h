/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/

#pragma once
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "LspTypes.h"
#include "ValueEncoder.h"
#include "MemoryStream.h"

static	const	int		MOD_CHANNEL_COUNT = 4;
static	const	int		AMIGA_PERIOD_BITS = 12;
static	const	int		AMIGA_PER_MIN = 14;
static	const	int		PAULA_REPLAY_RATE_MAX = 28800;

static const int		LSP_INSTRUMENT_MAX = 2600;		// theoretical max is 32767/12
static const int		LSP_CMDWORD_MAX = 255 * 3;

static	const	int		LSP_MAJOR_VERSION = 1;
static	const	int		LSP_MINOR_VERSION = 22;

static const int kMicroModeStreamCount = 16;

struct ConvertParams
{
	void	Reset()
	{
		m_generateInsane = false;
		m_keepModSoundBankLayout = false;
		m_verbose = false;
		m_renderWav = false;
		m_nosettempo = false;
		m_amigaEmulation = false;
		m_lspMicro = false;
		m_packEstimate = false;
		m_seqGetPosSupport = false;
		m_seqSetPosSupport = false;
		m_shrink = false;
	}

	bool	ParseArgs(int argc, char* argv[]);

	const char*	m_modFilename;
	char		m_sBankFilename[_MAX_PATH];
	char		m_sScoreFilename[_MAX_PATH];
	char		m_sPlayerFilename[_MAX_PATH];
	char		m_sWavFilename[_MAX_PATH];
	char		m_sAmigaWavFilename[_MAX_PATH];

	void		SetNameWithExtension(const char* src, char* dst, const char* sExt, const char* sNamePostfix);

	bool		m_generateInsane;
	bool		m_keepModSoundBankLayout;
	bool		m_verbose;
	bool		m_renderWav;
	bool		m_nosettempo;
	bool		m_amigaEmulation;
	bool		m_lspMicro;
	bool		m_packEstimate;
	bool		m_seqGetPosSupport;
	bool		m_seqSetPosSupport;
	bool		m_shrink;

};

class LSPEncoder
{
public:
	struct ChannelRowData
	{
		int		period;
		int		volume;
		int		instrument;
		int		sampleOffsetInBytes;
		bool	dmaRestart;
		bool	volSet;
		bool	perSet;
	};

	struct LspSample
	{
		s8*	sampleData;
		int len;
		int repStart;
		int repLen;
		int soundBankOffset;		// offset in the final sound bank
		int resampleMaxLen;			// real sample bytes used (depending of PAULA simulation)
		int maxReplayRate;			// max PAULA play rate for this sample (to properly fix micro-samples)
		int sampleOffsetMax;		// max $9xx fx (sample offset) applied to this sample

		void	ExtendSample(int addedSampleCount);
	};

	struct LSPInstrument
	{
		int			lspSampleId;
		int			sampleOffset;
	};

	struct LspFrameData 
	{
		u16		bpm;
		u16		wordCmd;
	};

	LSPEncoder();
	~LSPEncoder();

	bool	LoadModule();

	void	SetPeriod(int channel, int period);
	void	SetVolume(int channel, int volume);
	void	NoteOn(int channel, int instrument, int sampleOffsetInBytes, bool DMAConReset);
	void	SetSeqPos(int seqPos);
	void	SetSeqLoop(int seqPos);
	void	SetBPM(int bpm);
	void	DisplayInfos();
	void	SetModInstrumentInfo(int instr, s8* modSampleBank, int start, int len, int repStart, int repLen);

	void	SetSampleReplayRate(int modSampleId, int replayRate);
	void	SetSampleFetch(int modSampleId, int offset);
	void	SetOriginalModSoundBank(int moduleFileSoundBankOffset, int size);

	bool	ExportCodeHeader(FILE* h, int lspScoreSize, int wordStreamSize);
	bool	ExportToLSP();
	bool	NoSetTempoCommand() const { return m_convertParams.m_nosettempo; }
	bool	MicroMode() const { return m_convertParams.m_lspMicro; }

	bool	ParseArgs(int argc, char* argv[])
	{
		return m_convertParams.ParseArgs(argc, argv);
	}

private:

	enum VoiceCode
	{
		kNone,
		kResetLen,
		kPlayWithoutNote,
		kPlayInstrument
	};

	void	Free();
	void	Reset();
	bool	ExportBank(const char* sfilename);
	bool	ExportScore(const ConvertParams& params, MemoryStream* streams, int streamCount, bool microMode);
	bool	ExportReplayCode(FILE* h);
	void	AddLSPInstrument(int id, int modInstrument, int sampleOffset);

	int		ComputeLSPMusicSize(int dataStreamSize, bool microMode) const;
	void	ComputeAndFixSampleOffsets();
	void	GenLabel(int word, char* out);
	int		VoiceCodeCompute(int frameDmaCon, int frameResetMask, int frameInstMask) const;
	int		FrameToSeq(int frame) const;


	int		m_ModFileSize;
	u8*		m_ModBuffer;

	int		m_previousVolumes[4];
	int		m_previousPeriods[4];
	int		m_previousInstrument[4];

	const s8*	m_originalModSoundBank;
	int		m_originalModSoundBankSize;

	int		m_lspSoundBankSize;
	int		m_lspScoreSize;

	int		m_frameCount;
	int		m_frameMax;

	int		m_MODScoreSize;
	int		m_totalSampleCount;

	bool	m_sampleOffsetUsed;
	int		m_setBpmCount;

	int		m_bpm;
	int		m_minTickRate;
	bool	m_sampleWithoutANote;

	u32		m_modInstrumentUsedMask;

	u32		m_uniqueId;
	int		m_modDurationSec;

	int		m_channelMaskFilter;

	ValueEncoder	m_cmdEncoder;
	ValueEncoder	m_lspIntrumentEncoder;
	ValueEncoder	m_periodEncoder;

	int				m_EscValueRewind;
	int				m_EscValueSetBpm;
	int				m_EscValueGetPos;

	LspSample		m_lspSamples[31];

	int				m_seqHighest;
	int				m_seqFinalCount;
	int				m_frameLoop;
	int				m_byteStreamLoopPos;
	int				m_wordStreamLoopPos;

	LSPInstrument	m_lspIntruments[LSP_INSTRUMENT_MAX];
	ChannelRowData*	m_ChannelRowData[MOD_CHANNEL_COUNT];
	LspFrameData*	m_RowData;
	u16*			m_wordCommands_foo;
	int				m_seqPosFrame[128];
	int				m_seqPosWordStream[128];
	int				m_seqPosByteStream[128];

	ConvertParams	m_convertParams;
};

extern LSPEncoder gLSPEncoder;
