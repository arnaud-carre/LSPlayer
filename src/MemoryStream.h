/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/

#pragma once

static	const	int	GROWING_SIZE = 256 * 1024;

class MemoryStream
{
public:
		MemoryStream();
		~MemoryStream();

		void	Add8(unsigned char v)
		{
			Store8(v, m_pos);
			m_pos++;
		}
		void	Add16(unsigned short v)
		{
			Store16(v, m_pos);
			m_pos += 2;
		}
		void	Add32(unsigned int v)
		{
			Store32(v, m_pos);
			m_pos += 4;
		}
		void	Add(const MemoryStream& stream);
		
		void	Store8(unsigned char v, int offset);
		void	Store16(unsigned short v, int offset);
		void	Store32(unsigned int v, int offset);

		int		GetSize() const { return m_pos; }
		const unsigned char*	GetRawBuffer() const { return m_buffer; }
		void	FileWrite(FILE* h) const;
		void	DeltaProcess();
		void	DebugSave(const char* fname);

private:

	int				m_pos;
	int				m_bufferSize;
	unsigned char*	m_buffer;
};
