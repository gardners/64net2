/*
   Header file for 64NET/2 Miscellaneous functions 
   (C) Copyright Paul Gardner-Stephen 1996, All rights reserved 
 */

#include "config.h"

int firsthighbit (uchar foo) {
int i;
  /* return the number of the first high bit in the byte */
    if (!foo) return (-1);
    for (i=8;i!=0;i--) {
	if (!(foo=foo << 1)) return (--i); }
  /* will never reach this point */
    return -1;	
}


int fatal_error (char *message) {
	/* fatal error */

	printf ("64NET: A fatal error has occured:\n");
	printf ("       %s\n", message);
#ifdef AMIGA
	FreeMiscResource (MR_PARALLELPORT);
#endif /* AMIGA */
	exit (2);
}

/* repair hack for opendir that can open '/windows' but cant '/windows/' */
#ifdef WINDOWS
#undef opendir
DIR* winhack_opendir (uchar *fn) {
  if ((strlen(fn)>1) && (fn[strlen(fn)-1]='/'))
     fn[strlen(fn)-1]='\0';
  return opendir(fn);
}
#endif
