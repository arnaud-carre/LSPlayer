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
			else if (0 == strcmp(argv[argId], "-debugpcpreview"))
			{
				m_renderWav = true;
			}
			else if (0 == strcmp(argv[argId], "-nosettempo"))
			{
				m_nosettempo = true;
			}
			else if (0 == strcmp(argv[argId], "-amigapreview"))
			{
				m_amigaEmulation = true;
			}
			else if (0 == strcmp(argv[argId], "-pack"))
			{
				m_packEstimate = true;
			}
			else if (0 == strcmp(argv[argId], "-micro"))
			{
				m_lspMicro = true;
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
		SetNameWithExtension(m_modFilename, m_sBankFilename, ".lsbank", NULL);
		SetNameWithExtension(m_modFilename, m_sScoreFilename, ".lsmusic", m_lspMicro ? "_micro" : nullptr);
		SetNameWithExtension(m_modFilename, m_sPlayerFilename, ".asm", "_insane");
		SetNameWithExtension(m_modFilename, m_sWavFilename, ".wav", NULL);
		SetNameWithExtension(m_modFilename, m_sAmigaWavFilename, ".wav", "_amiga");
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
			if (m_generateInsane)
			{
				printf("ERROR: Insane mode does not support GetPos or SetPos\n");
				ret = false;
			}
		}
		if (m_keepModSoundBankLayout && m_shrink)
		{
			printf("ERROR: -nosampleoptim is not compatible with -shrink option\n");
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
		"\t-v : verbose\n"
		"\t-insane : Generate insane mode fast replayer source code\n"
		"\t-getpos : Enable LSP_MusicGetPos function use\n"
		"\t-setpos : Enable LSP_MusicSetPos function use\n"
		"\t-micro : Produce larger but highly compressible .lsmusic file (need micro replayer)\n"
		"\t-shrink: shrink any non used sample data if possible\n"
		"\t-nosampleoptim : preserve orginal .MOD soundbank layout\n"
		"\t-amigapreview : generate a wav from LSP data (output simulated LSP Amiga player)\n"
		"\t-pack : display Amiga Schrinkler packing estimation size (.lsmusic file only)\n"
		"\t-nosettempo : remove $Fxx>$20 SetTempo support (for very old .mods compatiblity)\n"
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
	printf("https://github.com/arnaud-carre/LSPlayer\n\n");

	return Process(argc, argv);
}

