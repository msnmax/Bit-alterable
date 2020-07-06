//* All the "typedef" and definitions are defined in this file

#ifndef _TYPEDEFINE_H
#define _TYPEDEFINE_H




typedef unsigned long	u32;
typedef unsigned short	u16;
typedef unsigned char	u8;

typedef unsigned long 	ul;
typedef unsigned int	ui;
typedef unsigned short	us;
typedef unsigned char	uc;

typedef unsigned char	BYTE;
typedef unsigned short	TWOBYTE;
typedef unsigned short	HWORD;
typedef unsigned int	WORD;

typedef int		flash_size_t;		//* used for the indices of flash block, and LBAs.
typedef char	flash_page_size_t;		//* used for the indices of flash pages
#ifdef LINUX
	typedef long long flash_huge_size_t;	//* used for counters
	#define PRINTF_LONGLONG	"lld"
#else
	typedef __int64	flash_huge_size_t;	//* used for counters
	#define PRINTF_LONGLONG	"I64d"
#endif

/* Common booleans */
#ifndef BOOLEAN
#define BOOLEAN
typedef enum
{
	False,
	True
}Boolean;
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
















#endif

