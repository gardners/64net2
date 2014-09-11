/*
   DOS Error message thingy 

 */

#include "config.h"
#include "error.h"
#include "client-comm.h"

/* ** CBM DOS emulation stuff: */
/* DOS Statii */
uchar dos_status[MAX_CLIENTS][MAX_FS_LEN];

/* length of DOS statii */
uchar dos_stat_len[MAX_CLIENTS];

/* position of char we actually talking */
uchar dos_stat_pos[MAX_CLIENTS];

int set_drive_status (uchar *string, int len) {
	/* set the drive message */
	if(curr_client<0) return -1;
	/* copy string */
	memcpy (dos_status[curr_client], string, len+1);
	
	/* and set length */
	dos_stat_len[curr_client] = len;
	//Fixed: need to set pos to 0!! TB
	dos_stat_pos[curr_client] = 0;
	return (0);
}

int set_error(int en, int t, int s, uchar* info) {
  /* display a DOS error message */

  uchar temp[256];
  char mesg[82][32] =
  {
    "OK",			//00
    "FILES SCRATCHED",		//01
    "PARTITION SELECTED",	//02
    "DIR X-LINK FIXED",		//03
    "FILE X-LINK FIXED",	//04
    "05", "06", "07", "08", "09", "10", "11", "12", "13", "14", "15", "16", "17",
    "18", "19",
    "READ ERROR",		//20
    "READ ERROR",		//21
    "READ ERROR",		//22
    "READ ERROR",		//23
    "READ ERROR",		//24
    "WRITE ERROR",		//25
    "WRITE PROTECT ON",		//26
    "READ ERROR",		//27
    "WRITE ERROR",		//28
    "DISK ID MISMATCH",		//29
    "SYNTAX ERROR",		//30
    "SYNTAX ERROR",		//31
    "SYNTAX ERROR",		//32
    "SYNTAX ERROR",		//33
    "SYNTAX ERROR",		//34
    "35", "36", "37",
    "OPERATION UNIMPLEMENTED",	//38
    "DIR NOT FOUND",		//39
    "40", "41", "42", "43", "44", "45", "46", "47",
    "ILLEGAL JOB",		//48
    "49",
    "RECORD NOT PRESENT",	//50
    "OVERFLOW IN RECORD",	//51
    "FILE TOO LARGE",		//52
    "FILENAME TOO LONG", 	//53
    "ILLEGAL FILENAME", 	//54
    "55", "56", "57", 
    "DIR NOT EMPTY",		//58
    "DIR EXISTS",		//59
    "WRITE FILE OPEN",		//60
    "FILE NOT OPEN",		//61
    "FILE NOT FOUND",		//62
    "FILE EXISTS",		//63
    "FILE TYPE MISMATCH",	//64
    "NO BLOCK",			//65
    "ILLEGAL BLOCK",		//66
    "ILLEGAL BLOCK",		//67
    "68", "69",
    "NO CHANNEL",		//70
    "DIRECTORY ERROR",		//71
    "PARTITION FULL",		//72
    "64NET/2 V1.0-RRNET (C) 2007",		//73
    "DRIVE NOT READY",		//74
    "FILE SYSTEM INCONSISTANT",	//75
    "MEDIA TYPE MISMATCH",	//76
    "ILLEGAL PARTITION",	//77
    "RECURSIVE FILESYSTEM",	//78
    "INVALID MEDIA",		//79
    "END OF FILE",		//80
    "PERMISSION DENIED"		//81    
	    //illegal filelength
	    //illegal filename
  };

  if ((en < 20) || (en == 73) || (en == 80))
    client_error (0);
  else
    client_error (1);

  sprintf ((char*)temp, "%02d, %s%s,%02d,%02d\r", en, mesg[en],info, t, s);
  //debug_msg ("%s\n", temp);

  /* set the message into the real status variable */
  set_drive_status (temp, strlen (temp));
  return (0);
}

int set_error(int en, int t, int s) {
	return set_error(en,t,s, (uchar*)"");
}

