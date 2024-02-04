/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/

// Emulate LSP 68k player behavior (using PAULA emulation) to check everything's fine on your PC
// ( -amigapreview command line option )

#pragma once
#include "LSPTypes.h"

class BinaryParser 
{
public:
			BinaryParser();
			~BinaryParser();

	bool	LoadFromFile(const char* sFilename, int startOffset = 0);
	bool	LoadFromMemory(const void* data, int maxLen, int startOffset = 0);
	bool	SetMemoryView(const void* data, int len);

	const void*	GetReadPtr() { return m_data + m_pos; }
	const void*	GetBuffer() { return m_data; }
	int			GetPos() const { return m_pos; }
	int			GetLen() const { return m_dataLen; }

	u8		ru8();
	u16		ru16();
	u32		ru32();
	s16		rs16();
	void	skip(int len);

private:

	void		Unload();

	u8*			m_data;
	int			m_dataLen;
	int			m_pos;
	bool		m_allocated;
};




class LSPDecoder
{
public:
	LSPDecoder();

	bool	LoadAndRender(const char* sMusicName, const char* sBankName, const char* sOutputWavFile, bool verbose);


private:

	u16		ReadNextCmd(BinaryParser& parser);

	struct LSPHalfInstrument
	{
		u32		pos;
		u16		len;
	};
	LSPHalfInstrument*	m_halfInstruments;

	int		m_instrumentCount;
	int		m_codesCount;
	u32		m_frameCount;
	u16		m_escCodeRewind;
	u16		m_escCodeSetBpm;
	u16		m_escCodeGetPos;
	u16*	m_codes;

	int		m_wordStreamSize;
	int		m_byteStreamLoop;
	int		m_wordStreamLoop;


};