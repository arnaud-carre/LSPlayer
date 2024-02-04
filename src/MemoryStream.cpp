/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "MemoryStream.h"

MemoryStream::MemoryStream()
{
	m_buffer = NULL;
	m_bufferSize = 0;
	m_pos = 0;
}

MemoryStream::~MemoryStream()
{
	free(m_buffer);
}

void	MemoryStream::Store8(unsigned char v, int offset)
{
	if (offset >= m_bufferSize)
	{	// grow
		m_bufferSize = offset + GROWING_SIZE;
		m_buffer = (unsigned char*)realloc(m_buffer, m_bufferSize);
	}
	assert(offset < m_bufferSize);
	m_buffer[offset] = v;
}

void	MemoryStream::Add(const MemoryStream& stream)
{
	if (m_pos + stream.m_pos > m_bufferSize)
	{
		m_bufferSize = m_pos + stream.m_pos + GROWING_SIZE;
		m_buffer = (unsigned char*)realloc(m_buffer, m_bufferSize);
	}
	memcpy(m_buffer + m_pos, stream.GetRawBuffer(), stream.m_pos);
	m_pos += stream.m_pos;
}

void	MemoryStream::DebugSave(const char* fname)
{
	FILE* h;
	if (0 == fopen_s(&h, fname, "wb"))
	{
		FileWrite(h);
		fclose(h);
	}
}

void	MemoryStream::DeltaProcess()
{
	unsigned char cur = 0;
	for (int i = 0; i < m_pos; i++)
	{
		const unsigned char ncur = m_buffer[i];
		m_buffer[i] = m_buffer[i] - cur;
		cur = ncur;
	}
}

void	MemoryStream::Store16(unsigned short v, int offset)
{
	assert(0 == (offset & 1));
	Store8(v >> 8, offset);
	Store8(v & 255, offset + 1);
}

void	MemoryStream::Store32(unsigned int v, int offset)
{
	assert(0 == (offset & 1));
	Store8(v >> 24, offset);
	Store8(v >> 16, offset + 1);
	Store8(v >> 8, offset + 2);
	Store8(v >> 0, offset + 3);
}

void	MemoryStream::FileWrite(FILE* h) const
{
	fwrite(m_buffer, 1, m_pos, h);
}
