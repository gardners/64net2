/*
  Header file for 64NET/2 Miscellaneous functions 
  (C) Copyright Paul Gardner-Stephen 1996, All rights reserved 
  */

int firsthighbit (uchar foo);
int fatal_error (char *message);
#ifdef WINDOWS
DIR *winhack_opendir(uchar *fn);
#endif
