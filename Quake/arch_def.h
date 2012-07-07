/*
 * arch_def.h
 * platform specific definitions
 * - standalone header
 * - doesn't and must not include any other headers
 * - shouldn't depend on compiler.h, q_stdinc.h, or
 *   any other headers
 *
 * Copyright (C) 2007-2012  O.Sezer <sezero@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __ARCH_DEFS__
#define __ARCH_DEFS__


#if defined(__APPLE__) && defined(__MACH__)
#   if !defined(__MACOSX__)
#	define	__MACOSX__		1
#   endif
#elif defined(macintosh) /* MacOS Classic */
#   if !defined(__MACOS__)
#	define	__MACOS__		1
#   endif
#elif (defined(__sun) || defined(sun)) && \
		(defined(__svr4__) || defined(__SVR4))
#   if !defined(__SOLARIS__)
#	define	__SOLARIS__		1
#   endif
#elif defined(__sun) || defined(sun)
#   if !defined(__SUNOS__)
#	define	__SUNOS__		1
#   endif
#elif defined(__sgi) || defined(sgi) || defined(__sgi__) || defined(_SGI_SOURCE)
#   if !defined(__IRIX__)
#	define	__IRIX__		1
#   endif
#elif defined(__QNXNTO__)
#   if !defined(__QNX__)
#	define	__QNX__			1
#   endif
#elif defined(__amigados__) || defined(__amigaos4__) || \
		defined(__AMIGA) || defined(__amigaos__)
#   if !defined(__AMIGA__)
#	define	__AMIGA__		1
#   endif
#endif	/* end of custom definitions	*/


#if defined(__DJGPP__) || defined(MSDOS) || defined(__MSDOS__) || defined(__DOS__)

#   if !defined(PLATFORM_DOS)
#	define	PLATFORM_DOS		1
#   endif

#elif defined(_WIN32) || defined(_WIN64)

#   if !defined(PLATFORM_WINDOWS)
#	define	PLATFORM_WINDOWS	1
#   endif

#elif defined(__MACOS__) || defined(__MACOSX__)

#   if !defined(PLATFORM_MAC)
#	define	PLATFORM_MAC		1
#   endif

#elif defined(__MORPHOS__) || defined(__AMIGA__) || defined(__AROS__)

#   if !defined(PLATFORM_AMIGA)
#	define	PLATFORM_AMIGA		1
#   endif

#elif defined(__riscos__)

#   if !defined(PLATFORM_RISCOS)
#	define	PLATFORM_RISCOS		1
#   endif

#else	/* here goes the unix platforms */

#if defined(__unix) || defined(__unix__) || defined(unix)	|| \
    defined(__linux__) || defined(__linux)			|| \
    defined(__FreeBSD__) || defined(__DragonFly__)		|| \
    defined(__OpenBSD__) || defined(__NetBSD__)			|| \
    defined(__hpux) || defined(__hpux__) || defined(_hpux)	|| \
    defined(__sun) || defined(sun) || defined(__IRIX__)		|| \
    defined(__GNU__) /* GNU/Hurd */ || defined(__QNX__)
#   if !defined(PLATFORM_UNIX)
#	define	PLATFORM_UNIX		1
#   endif
#endif

#endif	/* end of PLATFORM_ definitions */


/* Platforms that are (mostly) fine
 * when classified as PLATFORM_UNIX :
 */
#if defined(__MACOSX__)

#   if !defined(PLATFORM_UNIX)
#	define	PLATFORM_UNIX		2
#   endif

#endif	/* end of (pseudo) PLATFORM_UNIX */


#endif	/* __ARCH_DEFS__ */

