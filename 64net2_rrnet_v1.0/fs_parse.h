int fs64_resolve_partition(uchar *partition,uchar *path,int *track,int *sector);
int fs64_parse_filespec(uchar *filespec,int with_dir, uchar *path,uchar *glob,int *dirflag, int *replace,int *par,int *dirtrack,int *dirsect);
int fs64_findchar(uchar* string, char search, int pos);
int fs64_parse_diskheader(uchar* params, uchar* header, uchar* id);
int fs64_parse_diskimage(uchar* params, uchar* name, uchar* ext, uchar* header, uchar* id);
int fs64_parse_command(uchar* filespec, int * par, uchar* params);
int fs64_parse_filename(uchar* filespec2, int * par, uchar* filename, int * replace, int* isdir);
