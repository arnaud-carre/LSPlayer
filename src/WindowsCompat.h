//
// Created by Rich/Defekt on 21.02.2024.
//

#pragma once
#ifndef WINDOWS_COMPAT_H
#define WINDOWS_COMPAT_H

#include <stdio.h>

#ifdef MACOS_LINUX

#define fprintf_s fprintf

int fopen_s(FILE** h, const char* fname, const char* mode);

#endif

#endif