/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "LSPEncoder.h"
#include "LSPDecoder.h"
#include "external/micromod/micromod.h"
#ifdef MACOS_LINUX
#include "WindowsCompat.h"
#endif

LSPEncoder gLSPEncoder;

bool	ConvertParams::ParseArgs(int argc, char* argv[])
{

	int nameCount = 0;
	int argId = 1;
	bool ret = false;

	while (argId < argc)
	{
		if ('-' == argv[argId][0])
		{
			if (0 == strcmp(argv[argId], "-insane"))
			{
				m_generateInsane = true;
			}
			else if (0 == strcmp(argv[argId], "-setpos"))
			{
				m_seqSetPosSupport = true;
			}
			else if (0 == strcmp(argv[argId], "-getpos"))
			{
				m_seqGetPosSupport = true;
			}
			else if (0 == strcmp(argv[argId], "-shrink"))
			{
				m_shrink = true;
			}
			else if (0 == strcmp(argv[argId], "-nosampleoptim"))
			{
				m_keepModSoundBankLayout = true;
				printf("Option: keep original MOD sound bank layout\n");
			}
			else if (0 == strcmp(argv[argId], "-v"))
			{
				m_verbose = true;
			}
			#if D_MICROMOD_DEBUG
			else if (0 == strcmp(argv[argId], "-debugpcpreview"))
			{
				m_renderWav = true;
			}
			#endif
			else if (0 == strcmp(argv[argId], "-nosettempo"))
			{
				m_nosettempo = true;
			}
			else if (0 == strcmp(argv[argId], "-amigapreview"))
			{
				m_amigaEmulation = true;
			}
			else if (0 == strcmp(argv[argId], "-looppreview"))
			{
				m_loopPreview = true;
			}
			else if (0 == strcmp(argv[argId], "-pack"))
			{
				m_packEstimate = true;
			}
			else if (0 == strcmp(argv[argId], "-micro"))
			{
				m_lspMicro = true;
			}
			else if (0 == strcmp(argv[argId], "-fixed50hz"))
			{
				m_fixed50hz = true;
			}
			else if (0 == strcmp(argv[argId], "-adpcm"))
			{
				m_adpcm = true;
				printf("Option: Samples lossy compression enabled\n");
			}
			else if (0 == strcmp(argv[argId], "-mono"))
			{
				m_mono = true;
			}
			else if ((0 == strcmp(argv[argId], "-lsbank")) && (argId < argc-1))
			{
				strncpy_s(m_sBankFilename, argv[argId + 1], _MAX_PATH);
				argId++;
			}
			else if ((0 == strcmp(argv[argId], "-lsmusic")) && (argId < argc-1))
			{
				strncpy_s(m_sScoreFilename, argv[argId + 1], _MAX_PATH);
				argId++;
			}
			else if ((0 == strcmp(argv[argId], "-insanefile")) && (argId < argc-1))
			{
				strncpy_s(m_sPlayerFilename, argv[argId + 1], _MAX_PATH);
				argId++;
			}
			else if ((0 == strcmp(argv[argId], "-wav")) && (argId < argc-1))
			{
				strncpy_s(m_sAmigaWavFilename, argv[argId + 1], _MAX_PATH);
				argId++;
			}
			else if ((0 == strcmp(argv[argId], "-lossless")) && (argId < argc-1))
			{
				const int instrument = atoi(argv[argId + 1]);
				if ((instrument >= 1) && (instrument <= 31))
					m_losslessMask |= 1 << (instrument - 1);
				else
				{
					printf("ERROR: Invalid -lossless instrument id #%d (should be [1..31])\n", instrument);
					return false;
				}
				argId++;
			}
			else
			{
				printf("Unknown option \"%s\"\n", argv[argId]);
				return false;
			}
		}
		else
		{
			nameCount++;
			if (1 == nameCount)
				m_modFilename = argv[argId];
			else
			{
				printf("Too much files specified (\"%s\")\n", argv[argId]);
				return false;
			}
		}
		argId++;
	}

	if (1 == nameCount)
	{
		if ( 0 == m_sBankFilename[0] )
			SetNameWithExtension(m_modFilename, m_sBankFilename, ".lsbank", NULL);
		if ( 0 == m_sScoreFilename[0] )
			SetNameWithExtension(m_modFilename, m_sScoreFilename, ".lsmusic", m_lspMicro ? "_micro" : nullptr);
		if ( 0 == m_sPlayerFilename[0] )
			SetNameWithExtension(m_modFilename, m_sPlayerFilename, ".asm", "_insane");
		if ( 0 == m_sAmigaWavFilename[0] )
			SetNameWithExtension(m_modFilename, m_sAmigaWavFilename, ".wav", "_amiga");
		#if D_MICROMOD_DEBUG
		SetNameWithExtension(m_modFilename, m_sWavFilename, ".wav", NULL);
		#endif
		ret = true;
	}

	if (ret)
	{
		// do some sanity checks
		if (m_generateInsane && m_lspMicro)
		{
			printf("ERROR: Insane player doesn't support -micro\n");
			ret = false;
		}
		if (m_seqSetPosSupport || m_seqGetPosSupport)
		{
			if (m_lspMicro)
			{
				printf("ERROR: Micro mode does not support GetPos or SetPos\n");
				ret = false;
			}
			if ((m_generateInsane) && (m_seqSetPosSupport))
			{
				printf("ERROR: Insane mode does not support SetPos (only support GetPos)\n");
				ret = false;
			}
		}
		if (m_keepModSoundBankLayout && m_shrink)
		{
			printf("ERROR: -nosampleoptim is not compatible with -shrink option\n");
			ret = false;
		}

		if (m_keepModSoundBankLayout && m_adpcm)
		{
			printf("ERROR: -adpcm is not compatible with -nosampleoptim option\n");
			ret = false;
		}

		if (m_adpcm && m_lspMicro)
		{
			printf("ERROR: -adpcm is not compatible with -micro mode\n");
			ret = false;
		}
	}

	if ( !ret )
		printf("\n");

	return ret;
}

void	Help()
{
	printf(	"Usage:\n");
	printf("\tLSPConvert <mod file> [-options]\n"
		"\noptions:\n"
		"\t-micro : Produce larger but highly compressible .lsmusic file (need micro replayer)\n"
		"\t-adpcm : Produce highly compressible (greater than x2) .lsbank file (using ADPCM encoding)\n"
		"\t-lossless <x> : In case of -adpcm mode, do not ADPCM pack specific MOD instrument number x\n"
	 	"\t-insane : Generate insane mode fast replayer source code\n"
		"\t-getpos : Enable LSP_MusicGetPos function use\n"
		"\t-setpos : Enable LSP_MusicSetPos function use\n"
		"\t-shrink: shrink any non used sample data if possible\n"
		"\t-nosampleoptim : preserve original .MOD soundbank layout (nice for AmigaKlang)\n"
		"\t-amigapreview : generate a wav from LSP data (output simulated LSP Amiga player)\n"
		"\t-mono : generate MONO wav with -amigapreview option\n"
		"\t-looppreview : generate longer wav preview if you want to test MOD looping\n"
		"\t-pack : display Amiga Schrinkler packing estimation size (.lsmusic file only)\n"
		"\t-fixed50hz : Makes 50hz player compatible even with other BPM than 125! (no CIA required)\n"
		"\t-nosettempo : remove $Fxx>$20 SetTempo support (for very old .mods compatiblity)\n"
		"\t-lsbank <filename> : Set a specific name for .lsbank file\n"
		"\t-lsmusic <filename> : Set a specific name for .lsmusic file\n"
		"\t-wav <filename> : Set a specific name for -amigapreview WAV file\n"
		"\t-insanefile <filename> : Set a specific name for -insane mode generated source code\n"
		"\t-v : verbose\n"
	   );
}

int	Process(int argc, char* argv[])
{
	int ret = -1;
	
	if (gLSPEncoder.ParseArgs(argc, argv))
	{
		if (gLSPEncoder.LoadModule())
		{
			gLSPEncoder.ExportToLSP();
			ret = 0;
		}
	}
	else
	{
		Help();
	}
	return ret;
}

int main(int argc, char* argv[])
{
	printf("Light Speed Player Converter v%d.%02d\n", LSP_MAJOR_VERSION, LSP_MINOR_VERSION);
	printf("Fastest & Smallest 68k MOD music player ever!\n");
	printf("Written by Leonard/Oxygene (@leonard_coder)\n");
#ifdef MACOS_LINUX
	printf("macOS/Linux compatibility added by Rich/Defekt (@asimilon)\n");
#endif
	printf("https://github.com/arnaud-carre/LSPlayer\n\n");

	return Process(argc, argv);
}

