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
#include "ValueEncoder.h"

ValueEncoder::ValueEncoder()
{
	m_valueToCode = NULL;
	m_valueCounts = NULL;
	m_codeToValue = NULL;
}

void	ValueEncoder::Setup(int maxValueCount, int maxCodeCount)
{
	Release();

	m_maxValueCount = maxValueCount;
	m_maxCodeCount = maxCodeCount;
	m_codeCount = 0;

	m_valueToCode = (int*)malloc(m_maxValueCount * sizeof(int));
	m_codeToValue = (int*)malloc(m_maxValueCount * sizeof(int));	// should be maxCodeCount, but we allow more codes ( to keep some statistics for debug )
	m_valueCounts = (int*)calloc(m_maxValueCount,sizeof(int));
}

ValueEncoder::~ValueEncoder()
{
	Release();
}

void	ValueEncoder::Release()
{
	free(m_valueCounts);
	free(m_valueToCode);
	free(m_codeToValue);
}

bool	ValueEncoder::IsValueRegistered(int value) const
{
	if (unsigned(value) >= m_maxValueCount)
		return false;

	return m_valueCounts[value]>0;
}

int	ValueEncoder::RegisterValue(int value)
{
	int ret = -1;
	if (unsigned(value) < m_maxValueCount)
	{
		if (m_valueCounts[value]>0)
			ret = m_valueToCode[value];
		else
		{
			if (m_codeCount < m_maxValueCount)
			{
				ret = m_codeCount;
				m_valueToCode[value] = ret;
				m_codeToValue[ret] = value;
			}
			m_codeCount++;
		}

		m_valueCounts[value]++;

	}
	return ret;
}

int	ValueEncoder::GetValueFromCode(int code) const
{
	int ret = -1;
	if (unsigned(code) < m_codeCount)
	{
		ret = m_codeToValue[code];
		assert(m_valueCounts[ret]>0);
		assert(m_valueToCode[ret] == code);
	}
	return ret;
}

int	ValueEncoder::GetCodeFromValue(int value) const
{
	int ret = -1;
	if (IsValueRegistered(value))
		ret = m_valueToCode[value];
	return ret;
}

struct SortElement
{
	int	code;
	int value;
	int count;
};

int fCompare(const void *arg1, const void *arg2)
{
	const SortElement* pa = (const SortElement*)arg1;
	const SortElement* pb = (const SortElement*)arg2;
	return pb->count - pa->count;
}

int	ValueEncoder::GetFirstUnusedValue() const
{
	for (unsigned int i = 0; i < m_maxValueCount; i++)
	{
		if (m_valueCounts[i] == 0)
			return i;
	}
	return -1;
}

void	ValueEncoder::AddDummyCodeEntry()
{
	int dummyValue = GetFirstUnusedValue();
	int codeEntry = RegisterValue(dummyValue);

	m_valueCounts[dummyValue] = kDummyEntryValue;
	m_codeToValue[codeEntry] = dummyValue;
}

int	ValueEncoder::ComputeValueUsedCount() const
{
	int count = 0;
	for (unsigned int i = 0; i < m_codeCount; i++)
	{
		int v = GetValueFromCode(i);
		count += m_valueCounts[v];
	}
	return count;
}


void ValueEncoder::SortValues()
{
	SortElement* list = (SortElement*)malloc(m_codeCount * sizeof(SortElement));
	for (int i = 0; i<int(m_codeCount); i++)
	{
		list[i].code = i;
		list[i].value = m_codeToValue[i];
		list[i].count = m_valueCounts[m_codeToValue[i]];
	}

	qsort(list, m_codeCount, sizeof(SortElement), fCompare);

	// clear everything
	memset(m_valueToCode, 0, m_maxValueCount * sizeof(int));
	memset(m_codeToValue, 0, m_maxValueCount * sizeof(int));
	memset(m_valueCounts, 0, m_maxValueCount * sizeof(int));

	// 
	for (int i = 0; i<int(m_codeCount); i++)
	{
		const SortElement& e = list[i];
		m_valueToCode[e.value] = i;
		m_codeToValue[i] = e.value;
		m_valueCounts[e.value] = e.count;
	}

	free(list);
}

void ValueEncoder::DebugLog(const char* name)
{

	printf("ValueEncoder %s : %d codes\n", name, m_codeCount);
	int small = 0;
	for (int i = 0; i < int(m_codeCount); i++)
	{
		int val = m_codeToValue[i];
		assert(m_valueCounts[val] > 0);

		printf("Code %3d : $%04x ( count=%6d )\n", i, val, m_valueCounts[val]);
	}
}
