/*
*/

/* function prototypes */
int read_config (char *file);
int read_device (FILE * cf);
int read_client (FILE * cf,char* temp);
void chomp(char*);
int parse_line(char*, char*, char*, int);
