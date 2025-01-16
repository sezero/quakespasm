/*
 * Example Quake Extension
 *
 * Copyright (C) 2025 Tóth János <gomba007@gmail.com>
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
 *
 */

#include <stdio.h>
#include <string.h>

#include "qlib.h"

static char buffer[64];

// do not remove this
QEXTFNVER

QEXTFN const char* qextension_string (const char *cmd, const char *arg)
{
    printf ("qextension_string, cmd: %s, arg: %s\n", cmd, arg);

	int len = strlen (arg);
	if (len >= sizeof(buffer))
		return "NOK";

	strncpy (buffer, arg, sizeof(buffer));
	strncat (buffer, " of Quake", sizeof(buffer) - len - 1);
    return buffer;
}

QEXTFN float qextension_number (const char *cmd, float arg)
{
    printf ("qextension_number, cmd: %s, arg: %5.1f\n", cmd, arg);
    return arg + 1;
}

QEXTFN float* qextension_vector (const char *cmd, vec3_t arg)
{
    printf ("qextension_vector, cmd: %s, arg: " QV3FMT "\n", cmd, QV3ARG(arg));

	arg[0] += 1;
	arg[1] += 1;
	arg[2] += 1;

    return arg;
}

QEXTFN float qextension_entity (const char *cmd, edict_t *arg)
{
    printf ("qextension_entity, cmd: %s\n", cmd);
    return 1;
}
