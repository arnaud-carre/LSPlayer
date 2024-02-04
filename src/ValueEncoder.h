/*********************************************************************

	LSP (Light Speed Player) Converter
	Fastest & Tiniest 68k MOD player ever!
	Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
	https://github.com/arnaud-carre/LSPlayer

*********************************************************************/

#pragma once

static const int	kDummyEntryValue = 0x7fffffff;

class ValueEncoder
{
public:
	ValueEncoder();
	~ValueEncoder();

	void	Setup(int maxValueCount, int maxCodeCount);

	bool	IsValueRegistered(int value) const;
	int		RegisterValue(int value);
	int		GetValueFromCode(int code) const;
	int		GetCodeFromValue(int value) const;
	int		GetCodesCount() const { return m_codeCount; }
	void	SortValues();
	int		ComputeValueUsedCount() const;
	int		GetFirstUnusedValue() const;
	void	AddDummyCodeEntry();
	bool	IsDummyCodeEntry(int code) const { return (m_valueCounts[m_codeToValue[code]] == kDummyEntryValue); };

	void	DebugLog(const char* name);
	int		HuffmanPack(const char* label);

private:
	void			Release();
	unsigned int	m_maxValueCount;
	unsigned int	m_maxCodeCount;
	int*			m_valueCounts;
	int*			m_valueToCode;
	int*			m_codeToValue;
	unsigned int	m_codeCount;
};
