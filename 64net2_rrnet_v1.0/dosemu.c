/*
   CBM/CMD DOS Emulation module for 64NET/2
   (C)Copyright Paul Gardner-Stephen 1995, 1996, All rights reserved.

   NOTE: This code is, in part, based upon reading manuals and documentation
   by both Commodore Business Machines (CBM), and Creative Micro Designs (CMD)
   and as such is a legal reverse engineering project.  It is hereby 
   testified by myself, Paul Gardner-Stephen, that the above statement is true
   and that this module contains only original work by myself, and no work by
   either CBM or CMD.
 */

#include "config.h"
#include "fs.h"
#include "fs_func.h"
#include "dosemu.h"
#include "misc_func.h"
#include "comm-rrnet.h"
#include "parse_config.h"

/* virtual drive memory */
uchar drive_memory[MAX_CLIENTS][0x10000];

void reset_drive (int client) {
	/* reset last_drive
	   This routine is called whenever a UJ disk command is sent, or similar
	   reset means are employed */

	int i;

	/* cutesy startup message */
	curr_client=client;
	set_error (73, 0, 0);
	
	/* clear incoming command channel */
	if (client > -1) dos_comm_len[client] = 0;

	/* reset subdirectories */
	for (i = 1; i < MAX_PARTITIONS; i++) {
		if(partn_dirs[client][i]) {
			/* de-allocate current dir */
			if (curr_dir[client][i]) free (curr_dir[client][i]);
			/* set new one */
			if (!(curr_dir[client][i] = (uchar *) malloc (strlen (partn_dirs[client][i]) + 1))) /* couldnt malloc */ fatal_error ("Cannot allocate memory.");
			else {
				strcpy ((char*)curr_dir[client][i],(char*)partn_dirs[client][i]);
			}
		}
	}

	/* set default partition to 1 */
	curr_par[client] = 1;
}

void init_dos (void) {
	/* clear all things which need be cleared, and set default DOS messages.
	   This routine is called whenever the DOS system is completely reset.
	 */

	int i;

	memset(drive_memory,0,last_client*0x10000);
	for (i = 0; i <=last_client; i++) {
		reset_drive (i);
		drive_memory[i][0x69]=0x08; 	/* interleave set */
		drive_memory[i][0xe5c6]='4';	/* end of UJ message DOS VERSION, DRIVE - mimic 1541 */
	}
}

int do_dos_command (void) {
	/* execute the DOS command in the buffer */
	/* scratch vars */
	int j, par, rv;
	unsigned int a,start;
	uchar command[MAX_FS_LEN];
	uchar filespec[MAX_FS_LEN];
	uchar params[MAX_FS_LEN];
	int part_len, comm_len, filespec_len;
	fs64_filesystem fs;
	  

	dos_command[curr_client][dos_comm_len[curr_client]]=0;
	debug_msg ("DOS Command: [%s]\n", dos_command[curr_client]);
	if (dos_comm_len[curr_client] == 0) /* null command needs null action */ return (0);
	
  	j=0;
	filespec_len=0;
	comm_len=0;
	part_len=0;

	//get command
	while(j<dos_comm_len[curr_client] && dos_command[curr_client][j]!=':' && (dos_command[curr_client][j]<'0' || dos_command[curr_client][j]>'9') && comm_len<MAX_FS_LEN) {
		command[comm_len]=dos_command[curr_client][j];
		comm_len++; j++;
	}
	command[comm_len]=(char)0;

	//the rest should be filespec
	while(j<dos_comm_len[curr_client] && filespec_len<MAX_FS_LEN) {
		filespec[filespec_len]=dos_command[curr_client][j];
		filespec_len++; j++;
	}
	filespec[filespec_len]=(char)0;
	
	j=0;
	for(a=0;a<(unsigned int)filespec_len;a++) {
		/* walk along, until no more numbers occur */
		if(filespec[a]>='0' && filespec[a]<='9') {
			j++;
		}
		else {
			/* see if there is a separator */
			if(filespec[a]!=':') {
				/* huh? no separator, bad idea */
				set_error(30,0,0);
				dos_comm_len[curr_client] = 0;
				return -1;
			}
			else {
				break;
			}
		}
	}
	/* we only had numbers and no separator at the end, or filespec is 0 bytes long */
	if(j==filespec_len) {
		filespec[filespec_len]=':';
		filespec_len++;
		filespec[filespec_len]=0;
	}

	fs64_parse_filespec(filespec, 0, NULL, params, NULL, NULL, &par, NULL, NULL);
//	fs64_parse_command(filespec,&par,params);
	
	printf("command: [%s]   filespec: [%s]    partition: [%d]   params: [%s]\n", command,filespec,par,params); 	
	
	if (fs_pathtofilesystem (&fs,  curr_dir[curr_client][curr_par[curr_client]]  )) {
		dos_comm_len[curr_client]=0;
		return (-1);
	}

	//assume a good result, if not, functions will set a proper error, for sure :-) Toby
	set_error(0,0,0);

	//M-R memory read
	if(strcmp((char*)command,"M-R")==0) {
		int addr, k;
		addr = dos_command[curr_client][3]+256*dos_command[curr_client][4];
		debug_msg("M-R on $%04x for $%02x\n",addr,dos_command[curr_client][4]);
		for (k=0;k<dos_command[curr_client][5];k++) {
			dos_status[curr_client][k]=drive_memory[curr_client][addr+k];
		}
		dos_stat_len[curr_client]=dos_command[curr_client][5];
		dos_comm_len[curr_client] = 0;
		return 0;
	}
	//M-W memory write
	if(strcmp((char*)command,"M-R")==0) {
		int addr, k;
		addr = dos_command[curr_client][3]+256*dos_command[curr_client][4];
		debug_msg("M-W on $%04x for $%02x\n",addr,dos_command[curr_client][4]);
		for (k=0;k<dos_command[curr_client][5];k++) {
			drive_memory[curr_client][addr+k]=dos_command[curr_client][k+6];
		}
		dos_comm_len[curr_client] = 0;
		return 0;
	} //md, mkdir make directory md:filename
	if(strcmp((char*)command,"MD")==0 || strcmp((char*)command,"MKDIR")==0) {
		/* strip things before ':' from filespec */
		strcpy((char*)filespec,strchr((char*)filespec,':')+1);
		if(strlen(filespec)>0) {
			if(fs64_mkdir_g(filespec)) {
				dos_comm_len[curr_client]=0;
				return -1;
			}
			dos_comm_len[curr_client]=0;
			return 0;
		}
	} //rd, rmdir, remove dir rd:filename
	if(strcmp((char*)command,"RD")==0 || strcmp((char*)command,"RMDIR")==0) {
		/* strip things before ':' from filespec */
		strcpy((char*)filespec,strchr((char*)filespec,':')+1);
		if(strlen(filespec)>0) {
			if(fs64_rmdir_g(filespec)) {
				dos_comm_len[curr_client]=0;
				return -1;
			}
			dos_comm_len[curr_client]=0;
			return 0;
		}
	} //create, make disk image create:filename
	if(strcmp((char*)command,"CREATE")==0) {
		/* strip things before ':' from filespec */
		strcpy((char*)filespec,strchr((char*)filespec,':')+1);
		if(strlen(filespec)>0) {
			if(fs64_mkdxx_g(filespec)) {
				dos_comm_len[curr_client]=0;
				return -1;
			}
			dos_comm_len[curr_client]=0;
			return 0;
		}
	} //format, new - only works on current dir/par format:filename
	if(strcmp((char*)command,"N")==0 || strcmp((char*)command,"NEW")==0 || strcmp((char*)command,"FORMAT")==0) {
		/* strip things before ':' from filespec */
		strcpy((char*)filespec,strchr((char*)filespec,':')+1);
		uchar header[16];
		uchar id[16];
		fs64_parse_diskheader(filespec, header, id);
		if(fs64_format(&fs,header,id)) {
			dos_comm_len[curr_client]=0;
			return -1;
		}
		dos_comm_len[curr_client]=0;
		return 0;
	} //initialize - only works on current dir/par 
	if(strcmp((char*)command,"I")==0) {
		/* soft reset drive */
		dos_comm_len[curr_client] = 0;
		return 0;
	} //rename - only works on current dir/par, move can handle full paths/pars rename:newname=oldname
	if(strcmp((char*)command,"R")==0 || strcmp((char*)command,"RENAME")==0) {
		uchar source[1024];
		uchar target[1024];
		
		/* strip things before ':' from filespec */
		strcpy((char*)filespec,strchr((char*)filespec,':')+1);
		strcpy((char*)target,strtok((char*)filespec,"="));
		strcpy((char*)source,strtok('\0',""));
		
		dos_comm_len[curr_client] = 0;
		if(strlen(target)<=0 || strlen(source)<=0) {
			set_error(30,0,0);
			return -1;
		}
		if(fs64_rename_g(source,target)) return -1;
		return 0;
	} //move move:path=oldname
	if(strcmp((char*)command,"MV")==0 || strcmp((char*)command,"MOVE")==0) {
		uchar source[1024];
		uchar target[1024];
		strcpy((char*)target,strtok((char*)filespec,"="));
		strcpy((char*)source,strtok('\0',""));

//		if(target[strlen(target)-1]!='/') strcat((char*)target,(char*)"/");

		dos_comm_len[curr_client] = 0;
		if(strlen(target)<=0 || strlen(source)<=0) {
			set_error(30,0,0);
			return -1;
		}
		if(fs64_copy_g(source,target,1)) return -1;
		return 0;
	} //validate with purge - only works on current dir/par 
	if(strcmp((char*)command,"v")==0 || strcmp((char*)command,"vALIDATE")==0) {
		dos_comm_len[curr_client]=0;
		//then get fs with fs_pathtofilesystem
		if(fs64_validate(&fs,1)) {
			return (-1);
		}
		return 0;
	} //validate - only works on current dir/par
	if(strcmp((char*)command,"V")==0 || strcmp((char*)command,"VALIDATE")==0) {
                dos_comm_len[curr_client] = 0;
		if(fs64_validate(&fs,0)) {
			return (-1);
		}
		return 0;
	} //cd - cd:path
	if(strcmp((char*)command,"CD")==0) {
		int dirtr=-1,dirse=-1;
		uchar glob[1024]={0};
		uchar path[1024]={0};
		uchar temp[1024]={0};

		/* add a trailing / if not present, so all is interpreted as a path) */
		
		if(filespec[strlen(filespec)-1]!='/') strcat((char*)filespec,(char*)"/");
		
		if (fs64_parse_filespec(filespec, 1, path, glob, NULL, NULL, &par, &dirtr, &dirse)) {
			return -1;
		}
		dos_comm_len[curr_client] = 0;
		/* path can only be / or even less if size==1 */
		if(strlen(params)==0) {
			/* so it is a pwd as nothing else follows*/
			/* only copy relative part of path, to make it look like a chrooted environment */
			strcpy((char*)path,(char*)curr_dir[curr_client][par]);
			start=strlen((char*)partn_dirs[curr_client][par]);
			strcpy((char*)path,(char*)path+start);
			ascii2petscii(path,strlen(path));
			sprintf ((char*)temp, "%02d,/%s,%02d,%02d\r",0, path, 0, 0);
			set_drive_status (temp, strlen (temp));
			return 0;
		}
		else {
			/* save new path in curr_dir */	
			if (curr_dir[curr_client][par]) free (curr_dir[curr_client][par]);
			curr_dir[curr_client][par] = (uchar *) malloc (strlen (path) + 1);
			strcpy ((char*)curr_dir[curr_client][par], (char*)path);
			debug_msg ("Parsed path: [%d][%s]\n", par, path);
			/* and update dir block */
			curr_dirtracks[curr_client][par] = dirtr;
			curr_dirsectors[curr_client][par] = dirse;

			return 0;
		}
	} //cp change partition
	if(strcmp((char*)command,"CP")==0) {
		/* so it is basically a pwd  on partition as nothing else follows*/
		int dirtr=-1,dirse=-1;
		uchar glob[1024]={0};
		uchar path[1024]={0};
		uchar temp[1024]={0};
		
		if (fs64_parse_filespec (filespec, 1, path, glob, NULL, NULL, &par, &dirtr, &dirse)) {
			return -1;
		}
		if(strlen(filespec)<=1) {
			strcpy((char*)path,(char*)partn_dirs[curr_client][curr_par[curr_client]]);
			ascii2petscii(path,strlen(path));
			sprintf ((char*)temp, "%02d,%s,%02d,%02d\r",0, path,curr_par[curr_client], 0);
			set_drive_status (temp, strlen (temp));
			dos_comm_len[curr_client] = 0;
			return 0;
		}
		/* real change partiton */
		else {
			curr_par[curr_client]=par;
			set_error (2, curr_par[curr_client], 0);
			dos_comm_len[curr_client] = 0;
			return 0;
		}
	} // c: copy copy:path=oldname
	if(strcmp((char*)command,"C")==0 || strcmp((char*)command,"COPY")==0) {
		/* copy into new strings, so that we have enough space for later operations and won't override others */
		uchar source[1024];
		uchar target[1024];
		strcpy((char*)target,strtok((char*)filespec,"="));
		strcpy((char*)source,strtok('\0',""));
		
//		if(target[strlen(target)-1]!='/') strcat((char*)target,(char*)"/");

		dos_comm_len[curr_client] = 0;
		if(strlen(target)<=0 || strlen(source)<=0) {
			set_error(30,0,0);
			return -1;
		}
		if(fs64_copy_g(source,target,0)) return -1;
		return 0;
	} //validate with purge
	if(strcmp((char*)command,"S")==0 || strcmp((char*)command,"SCRATCH")==0 || strcmp((char*)command,"DEL")==0 || strcmp((char*)command,"RM")==0) {
		/* strip things before ':' from filespec */
		strcpy((char*)filespec,strchr((char*)filespec,':')+1);
		if(strlen(filespec)>0) {
			fs64_scratchfile_g(filespec);
			dos_comm_len[curr_client] = 0;
			return 0;
		}
	}
	if(command[0]=='U') {
		switch (dos_command[curr_client][1]) {
			/* Ux disk commands, eg reset, read sector, write sector */
			case '1':
			case 'A':
				rv = dos_u1 (dos_command[curr_client], dos_comm_len[curr_client], curr_client);
				dos_comm_len[curr_client] = 0;
				dos_command[curr_client][0] = 0;
				return (rv);
			case '2':
			case 'B':
				/* sector-write */
				/* unimplemented */
			case '3':
			case 'C':
				/* JMP $0500 */
				/* unimplemented */
			case '4':
			case 'D':
				/* JMP $0503 */
				/* unimplemented */
			case '5':
			case 'E':
				/* JMP $0506 */
				/* unimplemented */
			case '6':
			case 'F':
				/* JMP $0509 */
				/* unimplemented */
			case '7':
			case 'G':
				/* JMP $050C */
				/* unimplemented */
			case '8':
			case 'H':
				/* JMP $050F */
				/* unimplemented */
				set_error (38, 0, 0);
				dos_comm_len[curr_client] = 0;
				return -1;
			case '9':
			case 'I':
				/* soft-reset (does nothing) */
				dos_comm_len[curr_client] = 0;
				return (0);
			case 'J':
			case ':':
				/* hard-reset */
				reset_drive (curr_client);
				return (0);
			default:
				set_error (30, 0, 0);
				dos_comm_len[curr_client] = 0;
				return -1;
		}
	}
	set_error (30, 0, 0);
	dos_comm_len[curr_client] = 0;
	return -1;
}


