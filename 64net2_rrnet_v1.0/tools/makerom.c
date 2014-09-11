#ifdef LINUX
#include "/usr/include/linux/ioctl.h"
#endif
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netdb.h>
#include <sys/types.h>
#include <net/ethernet.h>
#ifdef LINUX
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#endif

#include <math.h>

#define IP_HEADER_OFFSET 0x1898
#define CLOCKP_ENABLE 0x140a
#define CLOCKP_DISABLE 0x1483

#define RR_V38 0x40
#define RR_V31 0x00


unsigned char ipheader[] = {
	/* mac */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* server mac 			00 */	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* client mac 			06 */
	0x08, 0x00,					/* protocoll type (ip) 		0c */
	
	/* ip */
	0x45, 0x00,					/* version 			0e */
	0x00, 0x00,					/* ident 			10 */
	0x00, 0x00,					/* packet length 		12 */
	0x40, 0x00,					/* flags 			14 */
	0x40,						/* time to live (ttl) 		16 */
	0x11,						/* protocoll (udp) 		17 */
	0x00, 0x00,					/* checksum 			18 */
	0x00, 0x00, 0x00, 0x00,				/* client ip 			1a */
	0x00, 0x00, 0x00, 0x00,				/* server ip 			1e */

	/* udp */
	0x80, 0x0c,					/* sourceport 			22 */
	0x13, 0x88,					/* dest. port			24 */
	0x00, 0x00,					/* udp-length			26 */
	0x00, 0x00,					/* udp checksum			28 */
	
	/* system */
	0x00, 0x00					/* final checksum		2a */
};


//XXX add --port=5000 arg and patch port aswell
unsigned char * smac;
unsigned char * cmac;
unsigned char * sip;
unsigned char * cip;

int mac2hex(char* mac, unsigned char* macaddr) {
	struct ether_addr* mac_addr;
	struct hostent* hp;
	mac_addr = ether_aton((const char*)mac);
	if(!mac_addr) return 0;
	memcpy(macaddr,mac_addr->ether_addr_octet,6);
	return 1;
}

int ip2hex(char* ip, unsigned char* hex) {
	struct hostent* hp;
	hp = gethostbyname(ip);
	if(!hp) return 0;
	memcpy(hex, hp->h_addr, 4);
	return 1;
}

int main(int argc, char **argv) {
	int a;
	int flag_smac=0, flag_cmac=0, flag_sip=0, flag_cip=0;
	unsigned int checksum=0;
	unsigned int rest=0;
	FILE* source;
	FILE* target;
	char * filename="patched.bin";

	unsigned char kernal[0x2000];
	unsigned char c;
	char * command;
	char * param;
	int rv;
	
	if(argc==1) {
		fprintf(stderr, "  Saves an optimized kernal-image with patched ip and mac-address.\n");
		fprintf(stderr, "  mac-address is of format: 12:34:56:78:9a:bc\n");
		fprintf(stderr, "  ip-address is of format:  192.168.2.13\n\n");
//		fprintf(stderr, "Usage: makerom\n");
		fprintf(stderr, "The following iptions are available:\n\n");
		fprintf(stderr, "  --smac=<mac-address>\nthe 64net/2-server's mac-address (required)\n\n");
		fprintf(stderr, "  --sip=<ip-address>\nthe 64net/2-server's ip-address (required)\n\n");
		fprintf(stderr, "  --cmac=<mac-address>\nthe c64's mac-address (required)\n\n");
		fprintf(stderr, "  --cip=<ip-address>\nthe c64's ip-address (required)\n\n");
		fprintf(stderr, "  --name=<output filename>\noptional output filename (default=output.bin)\n");
		exit(2);
	}
	for(a=1;a<argc;a++) {
//		printf("arg: %s\n",argv[a]);
		command=strtok(argv[a],"=");
		param=strtok(NULL,"=");
		
//		printf("command: %s    param: %s   \n",command,param);
		
		if(strcmp(command,"--smac")==0) {
			smac=(unsigned char*)malloc(6);
			if(!param || !mac2hex(param,smac)) {
				fprintf(stderr, "ERROR: --smac: mac-address not given or incomplete.\n");
				exit(2);
			}
		}  
		else if(strcmp(command,"--cmac")==0) {
			cmac=(unsigned char*)malloc(6);
			if(!param || !mac2hex(param,cmac)) {
				fprintf(stderr, "ERROR: --cmac: mac-address not given or incomplete.\n");
				exit(2);
			}
		}  
		else if(strcmp(command,"--cip")==0) {
			cip=(unsigned char*)malloc(4);
			if(!param || !ip2hex(param,cip)) {
				fprintf(stderr, "ERROR: --cip: ip-address not given or incomplete.\n");
				exit(2);
			}
		}  
		else if(strcmp(command,"--sip")==0) {
			sip=(unsigned char*)malloc(4);
			if(!param || !ip2hex(param,sip)) {
				fprintf(stderr, "ERROR: --sip: ip-address not given or incomplete.\n");
				exit(2);
			}
		}  
		else if(strcmp(command,"--name")==0) {
			if(!param) {
				fprintf(stderr, "ERROR: --name: no filename given.\n");
				exit(2);
			}
			filename=param;
		}  
		else {
			fprintf(stderr, "ERROR: Unknown option: %s\n",command);
			exit(2);	
		}
		
	}
		
	printf("server mac is: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",smac[0],smac[1],smac[2],smac[3],smac[4],smac[5]);
	printf("client mac is: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",cmac[0],cmac[1],cmac[2],cmac[3],cmac[4],cmac[5]);
	printf("server ip is: %.2x.%.2x.%.2x.%.2x\n",sip[0],sip[1],sip[2],sip[3]);
	printf("client ip is: %.2x.%.2x.%.2x.%.2x\n",cip[0],cip[1],cip[2],cip[3]);

	memcpy(&ipheader[0x00],smac,6);
	memcpy(&ipheader[0x06],cmac,6);
	memcpy(&ipheader[0x1a],cip,4);
	memcpy(&ipheader[0x1e],sip,4);

	checksum=0x1e; /* add minlen of packet already, so we only need to substract payload off checksum later on */
	/* only calc checksum on ip header -> from offset 0x0e on */
	for(a=0x0e;a<0x22;a+=2) { 
		checksum+=(ipheader[a]*256+ipheader[a+1]);
		/* add overflowing bits to simulate a 16 bit addition without clc */
		rest=checksum>>16;
		checksum=(checksum&0xffff)+rest;
	}
	checksum &= 0xffff;
	checksum ^= 0xffff;
	printf("precalculated checksum is: $%.4x\n",checksum); 
	
	ipheader[0x2a]=(unsigned char)((checksum>>8)&0xff);
	ipheader[0x2b]=(unsigned char)(checksum&0xff);

//	for(a=0;a<0x2c;a++) {
//		printf("$%.2x ",ipheader[a]);
//	}
//	printf("\n");

	/* now open kernal file */
	source=fopen("kernal","r");
	if(!source) {
		fprintf(stderr, "ERROR: could not open file (%s).\n","kernal");
		exit(2);
	}
	a=0;
	while(fread(&c,1,1,source)) {
		if(a<0x2000) kernal[a]=c;
		a++;
	}
	fclose(source);
	
	printf("successfully read $%.4x bytes of kernal rom\n",a);
	if(a!=0x2000) {
		fprintf(stderr, "ERROR: kernal rom has wrong size (!=0x2000).\n");
		exit(2);
	}
	printf("patching ip struct...\n",a);
	memcpy(kernal+IP_HEADER_OFFSET,ipheader,0x2c);
	target=fopen(filename,"w");
	if(target) {
		fwrite(kernal,1,0x2000,target);
	}
	else {
		fprintf(stderr, "ERROR: could not open file for writing (%s).\n",filename);
		exit(2);
	}
	printf("patched kernal written to '%s'\n",filename);
	fclose(target);

	

	/* patch ip, mac and chksum */

	/* write out new kernal */
	
	
//	fdw = fopen (write_name, write_mode);
//	if(fdw==NULL) { fprintf(stderr, "ERROR: can't open output file\n"); exit(0); }
//	c=address&255;
//	fwrite(&c,1,1,fdw);
//	c=address>>8;
//	fwrite(&c,1,1,fdw);
//	fclose(fdw);
	free(sip);
	free(cip);
	free(smac);
	free(cmac);
	exit (0);
//	c=0; fread(&c,1,1,fdr); bpp=(int)c;
}
