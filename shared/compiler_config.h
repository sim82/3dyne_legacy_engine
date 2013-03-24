#ifndef _3dyne_compiler_config_h
#define _3dyne_compiler_config_h

#include <stdint.h>

#if (INTPTR_MAX == INT64_MAX)
#define D3DYNE_MM_64 (1)
#elif (INTPTR_MAX == INT32_MAX)
#define D3DYNE_MM_32 (1)
#else
#error the memory model seems to be neither 64bit or 32bit (or stdint.h is broken)
#endif

#if defined(_WIN32)
#define D3DYNE_OS_WIN (1)
#define D3DYNE_COMP_MSVC (1)
// todo: support D3DYNE_COMP_GCCLIKE on windows
#else
#define D3DYNE_OS_UNIXLIKE (1)
#define D3DYNE_COMP_GCCLIKE (1)
#endif

#define D3DYNE_CPU_X86 (1)


#if D3DYNE_OS_UNIXLIKE && D3DYNE_CPU_X86
#define linux_i386
#endif


#if D3DYNE_OS_WIN && D3DYNE_CPU_X86
#define win32_x86
#endif

#endif