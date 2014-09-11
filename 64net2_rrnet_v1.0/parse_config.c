/* 
   64NET/2 Config parser module
   (C)Copyright Tobias Bindhammer 2006 All Rights Reserved.
 */

#include "config.h"
#include "fs.h"
#include "fs_func.h"
#include "comm-rrnet.h"
#include "client-comm.h"
#include "misc_func.h"
//#include "datestamp.h"
//#include "dosemu.h"
//#include "arp.h"
#include "parse_config.h"

#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/time.h>

#define PADDING 1

/* Partition base directories for each drive */
uchar *partn_dirs[MAX_CLIENTS][MAX_PARTITIONS];
int partn_dirtracks[MAX_CLIENTS][MAX_PARTITIONS];
int partn_dirsectors[MAX_CLIENTS][MAX_PARTITIONS];
/* Last accessed drive, for the purposes of error channels etc */
int last_client=0;
char port[256]; 
/* number of clients */
char client_ip[MAX_CLIENTS][15];
char client_mac[MAX_CLIENTS][17];
char client_name[MAX_CLIENTS][MAX_CLIENT_NAME_LEN];
__uid_t client_uid[MAX_CLIENTS];
__gid_t client_gid[MAX_CLIENTS];
int client_umask[MAX_CLIENTS];

int read_config (char *file) {
	/* Read in the 64netrc file */

	last_client=-1;
	curr_client=-1;
	FILE *cf = 0;
	char temp[256];
//	char value[256];
//	char key[256];
//	int is_section=0;

//	strcpy(temp,(char*)"         [client     drago]");
//	parse_line(temp,value,key,is_section);
//	return 0;

	if ((cf = fopen (file, "r")) == NULL) fatal_error ("Cannot read configuration file.");

	while (!feof (cf)) {
		fgets ((char*)temp, 256, cf);
		if ((temp[0] == '#') || (temp[0] == 0) || (temp[0] == '\n') || (temp[0] == '\r')) {
			/* its a comment or blank line */
		}
		else {
		/* its a real line of stuff */
			if (!strncmp ("port ", (char*)temp, 4)) {
				/* its a port line */
				strcpy ((char*)port, (char*)&temp[5]);
				/* chop CR/LF */
				chomp(port);
				printf ("INIT: Communication port set to %s\n", port);
			}
			/* deprecated, will be ignored anyway, hmm */
			else if (!strncmp ("path ", (char*)temp, 4)) {
				/* path partition */
				//pathdir = atol (&temp[5]);
				//printf ("64NET/2 will look in partition %d for programmes\n", pathdir);
			}
			//found a client block, read in config
			//XXX
			//also read out identifier so that we can separate clients (store in client_name[])
			else if (!strncmp ("[client", (char*)temp, 7)) {
				/* its a device line */
				read_client (cf,temp);
			}
			else if (!strncmp ("ledfile", (char*)temp, 6)) {
				/* its a device line */
				if (temp[strlen (temp) - 1] < ' ') temp[strlen (temp) - 1] = 0;
				if (temp[7] < ' ') sprintf ((char*)&temp[7], " %s/.64net2.leds", getenv ("HOME"));
				client_init (&temp[8]);
				client_activity (0);
				client_error (0);
			}
			else if (!strncmp ("debug mode", (char*)temp, 9)) {
				/* debug mode */
				debug_mode = 1;
				printf ("INIT: Debug mode messages will be displayed.\n");
			}
			else {
				/* Unknown line type */
				fprintf(stderr,"Unknown configuration directive:\n%s\n",temp);
				fatal_error ("Bad configuration file.  Unknown line types.");
			}
		}
	}
	fclose (cf);
	cf = 0;

	/* all done! */

	/* Check for required bits */
	//better check if a client was configured
	if (last_client<0) {
		/* no clients defined */
		fatal_error ("Configuration file contains no client sections.");
	}
	if (port[0] == 0) {
		/* no port lines */
		fatal_error ("Configuration file contains no port line.");
	}
	return (0);
}

int is_empty(char* temp) {
	unsigned int b=0;
	if(strlen(temp)==0) return 1;
	while(b<strlen(temp)) {
		switch(temp[b]) {
			case 0x0a:
			case 0x0d:
			case 0x20:
			case '\t':
			case 0xa0:
			break;
			case '#':
				return 1;
			break;
			default:
				return 0;
			break;
		}
		b++;
	}
	return 1;
}

void chomp(char* temp) {
	unsigned int b=0;
	int a=0;
	while(b<strlen(temp) && (temp[b]==' ' || temp[b]=='\t')) b++;
	for(b=b;b<strlen(temp);b++) {
		if(temp[b]=='\n' || temp[b]=='\r') temp[a]=0x00;
		else temp[a]=temp[b];
		a++;
	}
	//while((temp[strlen(temp)-1]=='\n')||(temp[strlen(temp)-1]=='\r')) temp[strlen(temp)-1]=0;
	//whipe out all whitespace at beginning, after first arg, only one 0x20 should be left
	//XXX now split into to args, don't count in sections marks [ ]
	//comments/empty lines return an error
	//sections set is_section
	return;	
}

int read_client (FILE * cf,char* name) {
	/* read a device section */
	passwd* uid;
	group* gid;
	int i, pn,a;
	char temp[256];
	char foo[256];
	int mode;

	last_client++;
	curr_client++;
	
	a=8;
	while (a<(int)strlen(name) && name[a]!=']') {client_name[curr_client][a-8]=name[a];a++; }
	printf ("INIT client #%d: reading config for client '%s'\n", curr_client, client_name[curr_client]);

	while (!feof (cf)) {
		fgets (temp, 256, cf);
		if(is_empty(temp) || feof(cf)) {
			/* comment line */
		}
		else {
			/* real line */
			/* Acceptables are:
			   IP <ip_num>                  - Sets the device # for this dev
			   MAC <mac_num>                - End definition of device
			   PARTITION <part_num>,<path>  - Define a partition
			   UID <name>			- owner of files
			   GID <name>			- dito
			   UMASK <mask>			- umask for files
			 */
			if (!strncmp ("partition", temp, 9)) {
				/* partition for drive */
				/* find first comma */
				for (i = 9; i < (int)strlen (temp); i++) {
					if (temp[i] == ',') break;
				}
				if (i >= (int)strlen (temp)) fatal_error ("Bad partition line (no commas).");
				pn = atol (&temp[9]);
				if ((pn < 1) || (pn > MAX_PARTITIONS)) {
					/* bad partition # */
					fatal_error ("Bad partition number. Must be between 1 and 255.");
				}
				else {
					/* okey */
					free (partn_dirs[curr_client][pn]);
					partn_dirs[curr_client][pn] = (uchar *) malloc (strlen (&temp[i + 1]) + PADDING);
					if (!partn_dirs[curr_client][pn]) /* couldnt malloc */ fatal_error ("Cannot allocate memory.");
					else {
						/* strip newline */
						uchar partition[8], path[1024];
						partition[0] = 'n';
						temp[strlen (temp) - 1] = 0;
						strcpy ((char*)partn_dirs[curr_client][pn], (char*)&temp[i + 1]);
						printf ("INIT client #%d:  %s added as partition %d\n", curr_client, partn_dirs[curr_client][pn], pn);
						/* parse for .DHD sub-directories */
						partn_dirtracks[curr_client][pn] = -1;
						partn_dirsectors[curr_client][pn] = -1;
						curr_dirtracks[curr_client][pn] = -1;
						curr_dirsectors[curr_client][pn] = -1;
						strcpy ((char*)path, (char*)partn_dirs[curr_client][pn]);
						ascii2petscii(path,strlen(path));
						if (path[0] != '@') {
							if (fs64_resolve_partition (partition, path, &partn_dirtracks[curr_client][pn], &partn_dirsectors[curr_client][pn])) {
								/* failed */
								printf ("Invalid partition path for %d\n", pn);
								fatal_error ("Invalid partition table\n");
							}
							curr_dirtracks[curr_client][pn] = partn_dirtracks[curr_client][pn];
							curr_dirsectors[curr_client][pn] = partn_dirsectors[curr_client][pn];
							//debug_msg ("  (%s T%d S%d)\n", path,partn_dirtracks[curr_client][pn], partn_dirsectors[curr_client][pn]);
							free (partn_dirs[curr_client][pn]);
							if (!(partn_dirs[curr_client][pn] = (uchar *) malloc (strlen (path) + 1 + PADDING))) {
								/* couldnt malloc */
								fatal_error ("Cannot allocate memory.");
							}
							strcpy ((char*)partn_dirs[curr_client][pn], (char*)path);
						}
					}
				}
			}
			else if (!strncmp ("ip", (char*)temp,2)) {
				strcpy ((char*)client_ip[curr_client], (char*)&temp[3]);
				chomp(client_ip[curr_client]);
				printf ("INIT client #%d: client IP set to %s\n", curr_client, client_ip[curr_client]);
			}
			else if (!strncmp ("uid", (char*)temp,3)) {
				strcpy (foo, (char*)&temp[4]);
				chomp(foo);
				uid = getpwnam(foo);
				if(!uid) {
					printf("Invalid uid '%s'\n",foo);
					fatal_error ("Invalid uid.");
				}
				client_uid[curr_client]=uid->pw_uid;
				printf ("INIT client #%d: client uid set to '%s'\n", curr_client, foo);
			}
			else if (!strncmp ("gid", (char*)temp,3)) {
				strcpy (foo, (char*)&temp[4]);
				chomp(foo);
				gid = getgrnam(foo);
				if(!gid) {
					printf("Invalid gid '%s'\n",foo);
					fatal_error ("Invalid gid.");
				}
				client_gid[curr_client]=gid->gr_gid;
				printf ("INIT client #%d: client gid set to '%s'\n", curr_client, foo);
			}
			else if (!strncmp ("umask", (char*)temp,5)) {
				strcpy (foo, (char*)&temp[6]);
				chomp(foo);
				printf ("INIT client #%d: client umask set to '%s'\n", curr_client, foo);
				mode=0;
				a=foo[0]-0x30;
				if(a<0 || a>7) fatal_error ("Invalid umask value.");
				if(a&0x1) mode|=S_IXUSR;
				if(a&0x2) mode|=S_IWUSR;
				if(a&0x4) mode|=S_IRUSR;
				a=foo[1]-0x30;
				if(a<0 || a>7) fatal_error ("Invalid umask value.");
				if(a&0x1) mode|=S_IXGRP;
				if(a&0x2) mode|=S_IWGRP;
				if(a&0x4) mode|=S_IRGRP;
				a=foo[2]-0x30;
				if(a<0 || a>7) fatal_error ("Invalid umask value.");
				if(a&0x1) mode|=S_IXOTH;
				if(a&0x2) mode|=S_IWOTH;
				if(a&0x4) mode|=S_IROTH;

				client_umask[curr_client]=mode;
				
			}
			else if (!strncmp ("mac", (char*)temp,3)) {
				strcpy ((char*)client_mac[curr_client], (char*)&temp[4]);
				chomp(client_mac[curr_client]);
				printf ("INIT client #%d: client MAC set to %s\n", curr_client, client_mac[curr_client]);
			}
			else if (!strncmp ("[client", (char*)temp, 7)) {
				/* its a device line */
				read_client (cf,temp);
			}
			else {
				printf("%x\n",temp[0]);
				/* Unknown line type */
				fprintf(stderr,"Unknown configuration directive:\n%s\n",temp);
				fatal_error ("Bad configuration file.  Unknown line types.");
			}
		}				/* end of not a comment */
	}				/* end of while !feof */
	return (0);
}

int get_word(char *line, char* val,int pos) {
	int start;
	int b=0;
	int oldpos=pos;
	while(pos<(int)strlen(line)) {
	       if(line[pos]!=(char)0x20 && line[pos]!=(char)0xa0 && line[pos]!=(char)'\t') break;
	       pos++;
	}
	start=pos;
	while(pos<(int)strlen(line)) {
	       if(line[pos]==(char)0x20 || line[pos]==(char)0xa0 || line[pos]==(char)'\t' || line[pos]==(char)']') break;
	       pos++;
	}
//	printf("%d\n",strlen(line));
//	printf("%d\n",start);
//	printf("%d\n",pos);
	for(b=0;b<pos-start;b++) val[b]=line[start+b];
	val[b]=0;
	return pos-oldpos;
}

int parse_line(char* line, char* key, char* value, int is_section) {
	int a=0;
	int flag=0;
	int pos=0;

	while((line[(int)strlen(line)-1]=='\n')||(line[(int)strlen(line)-1]=='\r')) line[(int)strlen(line)-1]=0;
	printf("%s\n",line);
	while(a<(int)strlen(line) && flag==0) {
		switch(line[a]) {
			case 0x20:
			case '\t':
			case 0xa0:
				a++;
			break;
			case '#':
				flag=-1;
			break;
			default:
				flag=1;
			break;
		}
	}
	if(flag==-1) return -1; /* comment */
	if(line[a]=='[') {
		is_section=1;
		pos=a+1;
	}
	else {
		is_section=0;
		pos=a;
	}
	pos+=get_word(line,key,pos);
	pos+=get_word(line,value,pos);
	//letztes zeichen testen -> muss bei is_section==1 ']' sein -> abstrippen
	if(is_section==1) {
		if(value[strlen(value)-1]!=']') {
			printf("Missing closing ] in section [%s %s ...\n",key,value);
			return -1;
		}
		else {
			value[strlen(value)-1]=0;
		}
	}
	printf("key:%s value:%s section:%d\n",key,value,is_section);
	return 0;
}

//int get_value(char* temp, char* value) {
//	int a;
//	/* skip whitespace */
//	for(a=0;a<strlen(temp);a++) {
//		if(temp[a]=='\t' || temp[a]==' ');
//		else break;
//	}
//}
