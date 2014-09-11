/*
   File system parsing routines for 64NET/2
   (C)Copyright Paul Gardner-Stephen 1995, All rights reserved
 */

#include "config.h"
#include "fs.h"


/* Re-written version of resolve partition, to allow globbing in paths in
   a proper manner */
int fs64_resolve_partition (uchar *partition, uchar *path, int *dirtrack, int *dirsect) {
	/* resolve the partition and path into a single path that points 
	   to the right place */

	
	uchar temp[1024]={0};
	
	int par;

	if (partition[0] == 0) {
		/* default partition is 0: */
		partition[0] = '0';
		partition[1] = 0;
	}

	par=atol(partition);
	if (!par) par=curr_par[curr_client];

	*dirtrack = curr_dirtracks[curr_client][par];
	*dirsect = curr_dirsectors[curr_client][par];

	if (partition[0] == 'n') {
		/* no partition */
		temp[0]=0;
	}
	else {
		if(!partn_dirs[curr_client][par]) {
			/* No path for this partition */
			/* 77, ILLEGAL PARTITION,00,00 */
			debug_msg("Error reason: Path for specified partition (%s) is null\n",partition);
			set_error (77, 0, 0);
			return -1;
		}
	}
	if(path[0] == '/') {
		/* copy original partition path if available */
		if(partn_dirs[curr_client][par]) strcpy ((char*)temp, (char*)partn_dirs[curr_client][par]);
		/* remove leading / from path */
		strcpy ((char*)path, (char*)path+1);
	}
	else {
		/* copy current path if available */
		if(curr_dir[curr_client][par]) strcpy ((char*)temp, (char*)curr_dir[curr_client][par]);
	}
	/* add a / if necessary */
	if(((temp[strlen (temp) - 1] != '/') && (fs64_mediatype (temp) == media_UFS)) || strlen(temp)==0) strcat ((char*)temp, "/");
//	if(strlen(temp)==0 || temp[strlen(temp)-1]!='/') strcat((char*)temp,(char*)"/");
	if(strlen(path)>0 && path[strlen(path)-1]!='/') strcat((char*)path,(char*)"/");
//	printf("Resolve Partition: partiton: [%d]    path: [%s]  temp: [%s]\n",par,path,temp);
	
	/* now check each path element, and add the first match */
	/* I think a state diagram would be shiny here */
	unsigned int i = 0, j = 0;
	char pathelement[MAX_FS_LEN]={0};
	uchar glob[1024]={0};
	fs64_direntry de;
	de.dir = 0;

	while (i<strlen(path)) {
		j=0;
		while(i<strlen(path) && path[i]!='/') { pathelement[j]=path[i]; j++; i++; }
		i++;	/* skip trailing / */
		pathelement[j]=0;
		//printf("pathelement: %s\n",pathelement);
		
		//back to home, so we replace that with / 
//		if(strcmp((char*)pathelement,"~/")==0) { /* load basedir again to temp */ }
		

		//cases that we ignore	
		if(strcmp((char*)pathelement,(char*)".") ==0 || strlen(pathelement)==0) {
		}
		//cases in which we trim back
		else if(strcmp((char*)pathelement,(char*)"..")==0) {
			switch (fs64_mediatype (temp)) {
				case media_DHD:
					fs64_filesystem fs;
					uchar block[256];
					fs.fsfile = 0;

					if (fs_pathtofilesystem (&fs, temp)) return (-1);
					if (readts (&fs, *dirtrack, *dirsect, block)) {
						fclose (fs.fsfile);
						fs.fsfile = 0;
						return (-1);
					}
					fclose (fs.fsfile);
					fs.fsfile = 0;
					if (block[34]) {
						if ((*dirtrack != block[34]) || (*dirsect != block[35])) {
							/* there is a parent */
							*dirtrack = block[34];
							*dirsect = block[35];
							break;
						}
					}
					/* fall through */
				default:
					if (strcmp ((char*)temp, (char*)partn_dirs[curr_client][par])) {
						/* CD != root dir */
						/* so trim temp back to next '/' */
						if(strlen(temp)==0) break;
						temp[strlen (temp) - 1] = 0;
						while (strlen(temp)>0 && temp[strlen (temp) - 1] != '/') { temp[strlen (temp) - 1] = 0; }
						/* set dir track & sector back to default */
						/* if new target is not UFS also remove trailing / */
						if(fs64_mediatype (temp) != media_UFS) temp[strlen (temp) - 1] = 0;
						*dirtrack = -1;
						*dirsect = -1;
					}
				break;
			}
		}
		/* nothing to trim back, so do the usual stuff and search for a fitting dir now */
		else {
			sprintf ((char*)glob, "%s,D", pathelement);
			if (fs64_findfirst_g (temp, glob, &de, dirtrack, dirsect)) {
				/* dir not found */
				debug_msg("Error reason: fs64_findfirst_g(%s,%s,&de,%d,%d) failed\n",temp,glob,*dirtrack,*dirsect);
				set_error (39, 0, 0);
				fs64_closefind_g (&de);
				return (-1);
			}
			/* nothing found, not even a dir, we give up */
			else {
				/* before we go any futher, lets close the find, and not leak memory */
				fs64_closefind_g (&de);
				/* match.. now, is it a dir */
				if ((de.filetype & 0x0f) != cbm_DIR) {
					/* not a dir */
					debug_msg("Error reason: de.filetype& 0x0f (= %d) != cbm_DIR\n", de.filetype & 0x0f);
					set_error (64, 0, 0);
					return (-1);
				}
				/* bingo! */
				*dirtrack = de.first_track;
				*dirsect = de.first_sector;
			}
			/* do a media check */
			j = fs64_mediatype (de.realname);
			switch (j) {
				case media_BAD:
				case media_NOTFS:
					/* no sir, it isnt it */
					debug_msg("Error reason: fs64_mediatype(%s) returned media_BAD or media_NOTFS\n",de.realname);
					set_error (39, 0, 0);
					return (-1);
				default:
					strcpy ((char*)temp, (char*)de.realname);
					/* make sure path ends in a "/" */
					if ((temp[strlen (temp) - 1] != '/') && (fs64_mediatype (temp) == media_UFS)) strcat ((char*)temp, "/");
			}
		}
		//printf("Resolve Partition: partiton: [%d]    path: [%s]  temp: [%s]\n",par,path,temp);
	}			/* end of while(state); */

	/* make sure ends in '/' if needed */
	if ((temp[strlen (temp) - 1] != '/') && (fs64_mediatype (temp) == media_UFS)) strcat ((char*)temp, "/");
//	printf("fs64_resolve_partition said: partiton: [%d]    path: [%s]  temp: [%s]\n",par,path,temp);

	/* copy temp back into path */
	strcpy ((char*)path, (char*)temp);

	return (0);
}

int fs64_parse_filespec (uchar *filespec2, int with_dir, uchar *path, uchar *glob, int *isdir, int *replace, int *par, int *dirtrack, int *dirsect) {
	/* take a file file reference string, and seperate it in to the path, glob
	   string and a few other things (eg whether the spec refers to a file or a
	   directory etc
	   for easier handling all parameters can be NULL if no result is wished for them TB
	   Also, glob the path part of this */

	uchar *tmp;
	uchar partition[256]={0};
	uchar filespec_snoz[1024];
	uchar *filespec = filespec_snoz;
	int i;

	if(!filespec2 || strlen(filespec2)==0) {
		set_error(34,0,0);
		return -1;
	}

	/* dont modify original */
	memset (filespec, 0, 1024);
	strcpy ((char*)filespec, (char*)filespec2);

	/* set some defaults */
//	*mode = mode_READ;
	if(isdir) *isdir = 0;
	if(replace) *replace = 0;

	if(path) path[0] = 0;
	if(glob) glob[0] = 0;

	/* STEP 1 - check out the first char */
	if(filespec[0]=='@') {
		if(replace) *replace = 1;
		filespec++;
	}
	else if(filespec[0]=='$') {
		if(isdir) *isdir=1;
		filespec++;
	}


	/* strip of partition */
	for(i=0;i<(int)strlen(filespec);i++) {
		if(i>3) {
			/* too many numbers for partition -> illegal partition */
			set_error(77,0,0);
			return -1;
		}
		/* check for numbers, but always expect more content after partition */
		if(filespec[i]>='0' && filespec[i]<='9' && i<((int)strlen(filespec)-1)) {
			partition[i]=filespec[i];
		}
		else {
			if(filespec[i]==':') {
				filespec+=i;
				partition[i]=0;
				break;
			}
			else {
				/* oops, no separator */
				partition[0]=0;
				break;
			}
		}
	}
	if(par) {
	       	*par = atol (partition);
		if(*par<=0) *par=curr_par[curr_client];
	}
	
	if(filespec[0]==':') { filespec++; }
	
//	debug_msg("fs64_parse_filespec said1: path=%s, partition=%d filespec=%s\n",path,*par,filespec);
	/* strip linefeeds left by lazy routines */
	if ((filespec[strlen (filespec) - 1] == 0x0a) || (filespec[strlen (filespec) - 1] == 0x0d)) filespec[strlen (filespec) - 1] = 0;

	/* strip '0:' off the filespec completely as it confuses the filesearch */
	tmp = (uchar*)strstr((char*)filespec,"0:");
	if (tmp!=NULL) strcpy ((char*)tmp, (char*)&tmp[2]);

	/* moreover, due to 1541 compatibility: $0: and $0 are also legal and mean current directory */
	if ((filespec[0]=='$') && filespec[1]=='0' && ((filespec[2]=='\0') || (filespec[2]==':'))) {
		filespec[1]='$';
		filespec = &filespec[1];
	}

	/* now copy pathpart of filespec to path */
	if(with_dir==1 && path) {
		if ((int)strlen (filespec)>0) {
			int ep=0;
			tmp=(uchar*)strrchr((char*)filespec,'/');
			if(tmp!=NULL) {
				ep=strlen(filespec)-strlen(tmp)+1;
				strncpy((char*)path,(char*)filespec,ep);
				path[ep]=0;
				if((int)strlen(path)>0) filespec=(uchar*)strrchr((char*)filespec,'/')+1;
			}
		}
	}

	if(par) {
		sprintf((char*)partition,"%d",*par);
		if((int)strlen(partition)<=0) strcpy ((char*)partition, "0");
	}
	
	/* now, resolve the partition name into a path, and concatenate it with 
	   the path to give the absolute path to the directory in question */
	if(par && path) {
		if (fs64_resolve_partition (partition, path, dirtrack, dirsect)) {
			/* resolve failed */
			/* resolve will have set the error which occured */
			return (-1);
		}
		/* ok, path now has the full path name to the file system & file */
//		debug_msg("fs64_parse_filespec said2: path=%s, partition=%d filespec=%s\n",path,*par,filespec);
	}

	if(isdir) {	
		if(filespec[0]=='$') {
			* isdir=1;
			strcpy((char*)filespec,(char*)filespec+1);
//			printf("glob: %s %d\n",filespec,strlen(filespec));
		}
		if(*isdir==1) {
			if(strlen(filespec)==0) { filespec[0]='*'; filespec[1]=0; }
//			printf("glob: %s %d\n",filespec,strlen(filespec));
			strcat((char*)filespec,(char*)",$");
//			printf("glob: %s %d\n",filespec,strlen(filespec));
		}
	}

	/* STEP 3 - Get glob string */
	if(glob) strcpy ((char*)glob, (char*)filespec);

	/* STEP 3.5 - Glob path to allow things like LOAD"$/ST*:* */

	return (0);
}

int fs64_parse_command(uchar* filespec2, int * par, uchar* params) {
	int i;
	uchar * filespec = filespec2;
	uchar partition[4]={0};
	for(i=0;i<(int)strlen(filespec);i++) {
		if(i>3) {
			/* too many numbers for partition -> illegal partition */
			set_error(77,0,0);
			return -1;
		}
		if(filespec[i]>='0' && filespec[i]<='9') partition[i]=filespec[i];
		else {
			filespec+=i;
			partition[i]=0;
			break;
		}
	}
	*par = atol (partition);
	if(*par<=0) *par=curr_par[curr_client];
	
	if(filespec[0]==':') { filespec++; }
	strcpy((char*)params,(char*)filespec);
	return 0;
}

//int fs64_parse_filename(uchar* filespec2, int * par, uchar* filename, int * replace, int* isdir) {
//	uchar filespec_snoz[1024]={0};
//	uchar *filespec = filespec_snoz;
//
//
//	*replace=0;
//	*isdir=0;
//
//	/* dont modify original */
//	strcpy ((char*)filespec, (char*)filespec2);
//	
//	if(filespec[0]=='@') {
//		*replace = 1;
//		filespec++;
//	}
//	else if(filespec[0]=='$') {
//		*isdir=1;
//		filespec++;
//	}
//
//	/* separate into partiton and rest of command */
//	if(fs64_parse_command(filespec,par,filename)) return -1; 
//
//	/* prepare for loading a dir */
//	if(filename[0]=='$') {
//		* isdir=1;
//		strcpy((char*)filename,(char*)filename+1);
////		printf("glob: %s %d\n",filename,strlen(filename));
//	}
//	if(*isdir==1) {
//		if(strlen(filename)==0) { filename[0]='*'; filename[1]=0; }
////		printf("glob: %s %d\n",filename,strlen(filename));
//		strcat((char*)filename,(char*)",$");
////		printf("glob: %s %d\n",filename,strlen(filename));
//	}
//	return 0;
//}

int fs64_parse_diskheader(uchar* params, uchar* header, uchar* id) {
	//XXX check for too long parameters
	int a,b;
	int head_pos=fs64_findchar(params,',',0);
	b=0; for(a=0;a<head_pos;a++) { header[b]=params[a]; b++; } header[b]=0;
	b=0; for(a=head_pos+1;a<(int)strlen(params);a++) { id[b]=params[a]; b++; } id[b]=0;
	return 1;       	
}

int fs64_parse_diskimage(uchar* params, uchar* name, uchar* ext, uchar* header, uchar* id) {
	int a,b;
	int name_pos=fs64_findchar(params,',',0);
	int head_pos=fs64_findchar(params,',',name_pos+1);
	b=0; for(a=0;a<name_pos;a++) { name[b]=params[a]; b++; }; name[b]=0;
	b=0; for(a=name_pos+1;a<head_pos && b<16;a++) { header[b]=params[a]; b++; }; header[b]=0;
	b=0; for(a=head_pos+1;a<(int)strlen(params);a++) { id[b]=params[a]; b++; }; id[b]=0;
	b=0; for(a=strlen(name)-4;a<(int)strlen(name) && b<4;a++) { ext[b]=params[a]; b++; }; ext[b]=0;
	return 1;       	
}

int fs64_findchar(uchar* string, char search, int pos) {
	for (pos = pos; string[pos]!=search && pos<(int)strlen(string); pos++);
	if(pos>=(int)strlen(string)) pos=strlen(string);
	return pos;
}
