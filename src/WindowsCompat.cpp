//
// Created by Rich/Defekt on 21.02.2024.
//

#include "WindowsCompat.h"

#ifdef MACOS_LINUX

int fopen_s(FILE** h, const char* fname, const char* mode)
{
    *h = fopen(fname, mode);
    return *h == NULL ? 1 : 0;
}

#endif