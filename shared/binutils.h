#pragma once

// Copyright (c) advancedfx.org
//
// Last changes:
// 2015-10-04 dominik.matrixstorm.com
//
// First changes:
// 2014-10-28 dominik.matrixstorm.com

#include <windows.h>

namespace Afx {
namespace BinUtils {

struct MemRange
{
	// inclusive
	DWORD Start;

	// exclusive
	DWORD End;

	MemRange();
	MemRange(DWORD start, DWORD end);

	bool IsEmpty(void);
};

class ImageSectionsReader
{
public:
	ImageSectionsReader(HMODULE hModule);

	bool Eof(void);

	void Next(void);

	/// <param name="characteristicsFilter">Minium bit mask that must be matched, should be a combination of IMAGE_SCN_*</param>
	void Next(DWORD characteristicsFilter);

	PIMAGE_SECTION_HEADER Get(void);
	MemRange GetMemRange(void);
	DWORD GetStartAddress(void);
	DWORD GetSize(void);

private:
	HMODULE m_hModule;
	PIMAGE_SECTION_HEADER m_Section;
	DWORD m_SectionsLeft;
};

/// <remarks>The memory specified by memRange must be readable.</remarks>
MemRange FindBytes(MemRange memRange, char const * pattern, DWORD patternSize);

MemRange FindCString(MemRange memRange, char const * pattern);

MemRange FindWCString(MemRange memRange, wchar_t const * pattern);

} // namespace Afx {
} // namespace BinUtils {
