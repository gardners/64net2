
int fs_pathtofilesystem (fs64_filesystem * fs, uchar *path);
int fs64_create_g (uchar *path, uchar *glob, fs64_file * f, int *dirtrack, int *dirsect);
int fs64_unreadchar(fs64_file *f, uchar *c);
int fs64_readchar (fs64_file * f, uchar *c);
int fs64_writechar (fs64_file * f, uchar c);
int fs64_writeblock (fs64_file * f);
int fs64_readblock (fs64_file * f);
int fs64_openfile_g (uchar* path, uchar *glob, fs64_file * f, int mode);
int fs64_closefile_g (fs64_file * f);
int fs64_openfile (fs64_direntry * de, fs64_file * f);
int fs64_closefile (fs64_file * f);
int fs64_scratchfile_g (uchar *filespec);
int fs64_scratchfile (fs64_direntry * de);
int fs64_rename(fs64_direntry de, uchar* newname);
int fs64_rename_g(uchar* sourcespec, uchar* targetspec);
int fs64_format(fs64_filesystem * fs,uchar* header, uchar* id);
int fs64_validate(fs64_filesystem * fs,int mode);
int fs64_rmdir_g(uchar* filespec);
int fs64_mkdir_g(uchar* filespec);
int fs64_mkdxx_g(uchar* filespec);
int fs64_copy_g(uchar* sourcespec, uchar* targetspec,int scratch);
int fs64_copy(fs64_direntry * de, uchar* targetspec);
