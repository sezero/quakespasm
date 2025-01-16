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
