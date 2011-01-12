// Embed a custom conback.lmp into the binary
//
// S.A. 6 Feb 2010, Ozkan 18 Feb 2010

#include "quakedef.h"

#if defined(USE_QS_CONBACK)
static const byte conback_byte[] =
{
#  include "conback.h"
};

static char *custom_conback = NULL;
static char size_data[2 * sizeof(int)];

char *get_conback (void)
{
  if (custom_conback) {
    memcpy(custom_conback, size_data, 2 * sizeof(int));
    return custom_conback;
  }
  /* sanity check */
  if (sizeof(conback_byte) < sizeof(qpic_t))
    Sys_Error ("Bad conback image.");
  /* make a copy of the conback_byte[] array */
  custom_conback = (char *)malloc (sizeof(conback_byte));
  memcpy (custom_conback, conback_byte, sizeof(conback_byte));
  /* backup the original size fields */
  memcpy (size_data, custom_conback, 2 * sizeof(int));
  return custom_conback;
}

#endif	/* USE_QS_CONBACK */

