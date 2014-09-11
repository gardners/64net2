/* 
   64NET/2 Commune module
   (C)Copyright Tobias Bindhammer 2006 All Rights Reserved.

   This module does all the C64 <--> Server communications
   RR-Net version
 */


#include "config.h"
#include "fs.h"
#include "fs_func.h"
#include "comm-rrnet.h"
#include "client-comm.h"
#include "misc_func.h"
#include "datestamp.h"
#include "dosemu.h"
#include "arp.h"

#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/time.h>

/* Current talk & listen logical files */
int talklf[MAX_CLIENTS]={-1};
int listenlf[MAX_CLIENTS]={-1};

/* Logical files */
fs64_file logical_files[MAX_CLIENTS][16];

/* ** DRIVE and PARTITION resolution: */
/* DOS commands */
uchar dos_command[MAX_CLIENTS][MAX_FS_LEN];
/* length of DOS command */
uchar dos_comm_len[MAX_CLIENTS]={0};


/* current partition on each drive */
int curr_par[MAX_CLIENTS];
/* current subdirectory (absolute) for each partition on each drive */
uchar *curr_dir[MAX_CLIENTS][MAX_PARTITIONS];
int curr_dirtracks[MAX_CLIENTS][MAX_PARTITIONS];
int curr_dirsectors[MAX_CLIENTS][MAX_PARTITIONS];

/* file that points to our communications device */
//fs64_file file;

/* partition # that will be searched for programs, whatever this means, not used */
//int pathdir;

//#define DEBUG_COMM

//different states
#define IEC_IDLE		0x00
#define IEC_TALK		0x40
#define IEC_LISTEN		0x20
#define IEC_UNTALK		0x5f
#define IEC_UNLISTEN		0x3f
#define IEC_IDENTIFY		0xff

//special SAs for LOAD/SAVE
#define IEC_LOAD		0x60
#define IEC_SAVE		0x60
#define IEC_OPEN		0xf0
#define IEC_CLOSE		0xe0
#define ERROR_EOI		0x40
#define ERROR_FILE_NOT_FOUND	0x02
#define ERROR_FILE_EXISTS	0x03
#define uchar		unsigned char

#define PACKET_DATA		0x44
#define PACKET_ACKNOWLEDGE	0x41
#define PACKET_COMMAND		0x43
#define PACKET_ERROR		0x45

#define DATA_PAYLOAD_SEND	0xfe
#define DATA_PAYLOAD_RECV	0xfe

#define WAITFORACK		0x01
#define IDLE			0x00
#define TIMEOUT			10

int sendfd[MAX_CLIENTS];
int receivefd;
struct sockaddr_in receiver = { 0 };
struct sockaddr_in sender[MAX_CLIENTS] = { 0 };

int acknowledge[MAX_CLIENTS];
unsigned SA[MAX_CLIENTS];
int state[MAX_CLIENTS]={IEC_IDLE};
unsigned char myfilespec[MAX_CLIENTS][1024];
int myfilespecsize[MAX_CLIENTS];

/* for measuring transfertimes */
clock_t starttime[MAX_CLIENTS];
clock_t endtime[MAX_CLIENTS];

struct timeb start[MAX_CLIENTS] = { 0 };
struct timeb end[MAX_CLIENTS] = { 0 };

long transferred_amount[MAX_CLIENTS]={0};

struct packet {
        unsigned char* data;
        unsigned char type;
        int size;
};
struct packet* out[MAX_CLIENTS];

int curr_client;

int iec_commune(int unused) {
	int a;
        struct hostent *hp;

	initialize();
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup (MAKEWORD (1, 1), &wsaData) != 0) {
		fprintf (stderr, "WSAStartup (): Can't initialize Winsock.\n");
		exit (EXIT_FAILURE);
		return 0;
	}
#endif
	
	//install receiver socket
        receiver.sin_family=AF_INET;
        receiver.sin_port=htons(atoi(port));
        receiver.sin_addr.s_addr=htonl(INADDR_ANY);
        receivefd = socket (AF_INET,SOCK_DGRAM, IPPROTO_UDP);

	//install sender sockets fro all clients
	for(a=0;a<=last_client;a++) {
        	myfilespecsize[a]=0;
	        acknowledge[a]=0;
		hp = gethostbyname(client_ip[a]);
		sender[a].sin_family = AF_INET;
		sender[a].sin_port = htons(atoi(port));
		sendfd[a] = socket (AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		bcopy (hp->h_addr, &(sender[a].sin_addr.s_addr), hp->h_length);
		if (sendfd[a]<0) {
			fprintf(stderr,"Failed to bind socket for client %s.\n",client_ip[a]);
			perror("open_port");
			exit (EXIT_FAILURE);
			return 0;
		}
	}

        bind(receivefd, (struct sockaddr *) &receiver, sizeof(receiver));

	//bind input socket
	if (receivefd<0) {
		fprintf(stderr,"Failed to bind to port %d to listen for UDP packets.\n",atoi(port));
		perror("open_port");
		exit (EXIT_FAILURE);
		return 0;
	}
	
//	strcpy((char*)dos_command[curr_client],(char*)"CD:/TESTD*/BLASEN.D64");
//	dos_comm_len[curr_client]=strlen(dos_command[curr_client]);
//	do_dos_command();
//	dos_status[curr_client][dos_stat_len[curr_client]]=0;
//	printf("%s\n",dos_status[curr_client]);
//	strcpy((char*)dos_command[curr_client],(char*)"CD:../..");
//	dos_comm_len[curr_client]=strlen(dos_command[curr_client]);
//	do_dos_command();
//	dos_status[curr_client][dos_stat_len[curr_client]]=0;
//	printf("%s\n",dos_status[curr_client]);
//	return -1;

	
	for(a=0;a<=last_client;a++) {
		//printf("Adding static arp entry (IP=%s MAC=%s)\n",client_ip[a], client_mac[a]);
		if(!arp_set(client_ip[a],client_mac[a])) {
			fatal_error("Failed to make static arp entry\n"); 
		}
	}

	start_server();
	return 0;
}

void rrnet_quit() {
	int a;
	for(a=0;a<=last_client;a++) {
		//printf("Deleting arp-entry for %s\n",client_ip[a]);
		arp_del(client_ip[a]);
	}
}

void initialize() {
	int i;
	for(i=0;i<MAX_CLIENTS;i++) {
		out[i]=(struct packet*)malloc(sizeof(struct packet));
       		out[i]->data=(unsigned char*)malloc(DATA_PAYLOAD_SEND);
		out[i]->size=0;
	}
	curr_client=1;
	return;
}

/* this has to be moved elsewhere into a kind of dispatcher which dispatches the appropriate
   packets to the respective server processes */

void start_server() {
	int i;
	struct packet* p;
	unsigned char buffer[1024];
	int bsize;
	struct sockaddr from;
	socklen_t fromlen;
        p=(struct packet*)malloc(sizeof(struct packet));
        p->data=(unsigned char*)malloc(1024);
	int flags;
	struct hostent* hp;
	
	printf ("Network started.\n");
	while(1) {					
		fromlen=16;
		flags=MSG_WAITALL;
		bsize=recvfrom(receivefd, buffer, sizeof(buffer), flags, &from, &fromlen);
		if(bsize>1) {
			/* my really cheap dispatcher *cough* - Toby 
			 * it would be more wisely to make a extra table that has 
			 * the last number of the ip as index, as all clients are 
			 * in the same subnet */
			//but so far this should be enough :-)
			
			for(curr_client=0;curr_client<=last_client;curr_client++) {
				hp = gethostbyname(client_ip[curr_client]);
				if(memcmp(hp->h_addr,from.sa_data+2,4)==0) break;
			}
			if(curr_client>last_client) curr_client=-1;
			if(curr_client>=0) {
#ifdef DEBUG_COMM
				printf("packet is for client #%d\n",curr_client);
#endif
				//XXX this could be placed somewhere else, but well :-) TB
				p->type=buffer[0];
				if(p->type==PACKET_DATA) {			//assemble a data packet (0x44, len, data)
					if(bsize>1) {
						for(i=0;i<bsize-2;i++) {
							p->data[i]=buffer[i+2];
						}
						p->size=buffer[i+1];
					}
				}
				else {						//assemble other packet (command, ack)
					p->data[0]=buffer[1];
					p->data[1]=buffer[2];
					p->size=bsize-1;
					//p->size=1;
				}
				buffer[1]=0;
				buffer[2]=0;
				//dispatch();
				//This is teh central point to decide which client will be handled by all methods using curr_client later on
				//As all further code is only executed due to an initiating packet in here, this is all fine
				//set curr_client on base of ip first, then process
				//all stuff and variables for otehr clients should stay untouched
				//as each packet = one cycle for one client
				process_packet(p);				//let the respective server process process the packet
			}
		}
	}
	free(p->data);
	free(p);
	return;
}

void process_packet(struct packet* p) {
	#ifdef DEBUG_COMM
	int i;
	printf ("current state: %d\n",state[curr_client]);
	printf ("Packet type: ");
	switch(p->type) {
		case PACKET_DATA:
			printf("data");
		break;
		case PACKET_COMMAND:
			printf("command $%02x",p->data[1]);
		break;
		case PACKET_ACKNOWLEDGE:
			printf("acknowledge $%02x",p->data[0]);
		break;
		case PACKET_ERROR:
			printf("error");
		break;
	}
	
	printf ("\nreceived packetsize: $%X",p->size);
	for(i=0;i<p->size;i++) {
		if((i&0x7)==0) printf("\n");
		printf ("$%02x ",p->data[i]);
	}
	printf("\n");
	printf("\n\n");
	#endif

	
        if(pkt_is_acknowledge(p)==1) {
		acknowledge[curr_client]=1;	//set flag if we got an ack
	}
        if(p->type==PACKET_COMMAND && p->size==1) {			//got a command? 
		change_state(p->data[0]);				//then change state 
		acknowledge[curr_client]=0;				//ignore pending acknowledges
		send_acknowledge();					//acknowledge command packet
	}
        if(p->type==PACKET_DATA) {
		transferred_amount[curr_client]+=p->size;
		send_acknowledge();					//acknowledge data packet
	}
	/* now enter state machine and do the next step */
        switch(state[curr_client]) {
                case IEC_TALK:
                        iec_talk(p);
                break;
                case IEC_LISTEN:
                        iec_listen(p);
                break;
                case IEC_UNLISTEN:
                        iec_unlisten(p);
                break;
                case IEC_UNTALK:
                        iec_untalk(p);
                break;
        }
        return;
}

int pkt_is_acknowledge(struct packet* p) {
	if(p->type==PACKET_ACKNOWLEDGE && p->size==1) return 1;
	return 0;
}

void change_state(unsigned char command) {
        /* Change TALK/LISTEN/UNLISTEN/UNTALK state based on character received 
	   as for talk and listen, mask out devicenumber */

        if((command&0xf0)==IEC_LISTEN) {
                state[curr_client]=IEC_LISTEN;
                return;
        }
        else if((command&0xf0)==IEC_TALK) {
                state[curr_client]=IEC_TALK;
                return;
        }
        else if(command==IEC_UNLISTEN) {
                state[curr_client]=IEC_UNLISTEN;
                return;
        }
        else if(command==IEC_UNTALK) {
                state[curr_client]=IEC_UNTALK;
                return;
        }
        else if(command==IEC_IDLE) {
                state[curr_client]=IEC_IDLE;
                return;
        }
        return;
}

/* the way we act when we are in listen state */
void iec_listen(struct packet* p) {
        int i;
        unsigned char command;
        if(p->type==PACKET_COMMAND) {							//we receive another command?
                command=p->data[0];
                if((command&0xf0)==IEC_SAVE || 
		   (command&0xf0)==IEC_OPEN || 
		   (command&0xf0)==IEC_CLOSE) {						//is it something we can handle?
                        SA[curr_client]=command&0xf0;                        			//set SA and lf
                        listenlf[curr_client]=command&0x0f;

                        if(SA[curr_client]==IEC_CLOSE) {						//close actual file
				closefile();
				talklf[curr_client]=-1;
				listenlf[curr_client]=-1;
                        }
                        if(SA[curr_client]==IEC_SAVE && listenlf[curr_client]!=0x0f) {				//prepare for saving
                                myfilespec[curr_client][myfilespecsize[curr_client]]=0;				
                                if(openfile(myfilespec[curr_client], mode_WRITE)) {				//try to get write access on requested file
					acknowledge[curr_client]=0;						//ignore pending acknowledges
                                	state[curr_client]=IEC_IDLE;                       			//so drop current state
                                        send_error(ERROR_FILE_EXISTS);						//failed
					return;
                                }
                        }
                        if(SA[curr_client]==IEC_OPEN || listenlf[curr_client]==0xf) { 				//prepare filespec
                                myfilespecsize[curr_client]=0;
                                myfilespec[curr_client][myfilespecsize[curr_client]]=0;
                        }
                }
                else {									//no idea what we got
                        if((command&0xf0)!=IEC_LISTEN) {
                                state[curr_client]=IEC_IDLE;                       			//so drop current state
                                process_packet(p);                      		//and reinterpret command packet
                        }
                }
		return;
        }
        if(p->type==PACKET_DATA) {							//we receive data
                if(SA[curr_client]==IEC_OPEN || (listenlf[curr_client]==0x0f && SA[curr_client]==IEC_LOAD)) {			//what we get is filename/diskcommand	
                        for(i=0;i<p->size;i++) {
                                if(myfilespecsize[curr_client]<1024) myfilespec[curr_client][myfilespecsize[curr_client]]=p->data[i];
                                myfilespecsize[curr_client]++;
                        }
                }
		/* XXX check for file open here? ->  file not open error, just in case */
                if(SA[curr_client]==IEC_SAVE && listenlf[curr_client]!=0x0f) {					//what we get is data to be saved
                        for(i=0;i<p->size;i++) {
				fs64_writechar(&logical_files[curr_client][listenlf[curr_client]], p->data[i]);
                        }
                }
        }
        return;
}

void iec_talk(struct packet* p) {
        unsigned char command;
        unsigned char temp;
        if(p->type==PACKET_COMMAND) {
                command=p->data[0];
                if((command&0xf0)==IEC_LOAD) {
                        SA[curr_client]=command&0xf0;
                        talklf[curr_client]=command&0x0f;
                        acknowledge[curr_client]=1;                          			//fake acknowledge, so that we get can immediatedly send first packet (passing if statement with acknowlegde!=0)
                        if(talklf[curr_client]!=0x0f) {						//file or command?				
				myfilespec[curr_client][myfilespecsize[curr_client]]=0;				//finish filespec
				switch(openfile(myfilespec[curr_client],mode_READ)) {			//try to open file
				//	case 1:
                        	//		send_error(ERROR_FILE_NOT_FOUND);			//failed
				//		acknowledge[curr_client]=0;				//clear ack flag, to recognize new acks
                                //		state[curr_client]=IEC_IDLE;          			//so drop current state
				//		return;
				//	break;
					case 1:
						//XXX fixme badly
						/* don't return to be able to repeat EOI. The C64 kernal however should 
						 * not request another packet after receiving an EOI -> so need 
						 * to bugfix kernal soon! This is just a temporary workaround */

						/* we opened a dir and now need to send empty dummy file */
						if(out[curr_client]->size==0) {					//didn't manage to get a single byte into packet
							send_error(ERROR_EOI);				//so must be end of file
							set_error(0,0,0);
						}
						else {							//have some bytes, send them
							send_data(out[curr_client]);
						}
					break;
					case 0:
						/* normal operation */
					break;
					default:
						/* no file or zero size file */
                        			send_error(ERROR_FILE_NOT_FOUND);			//failed
						acknowledge[curr_client]=0;				//clear ack flag, to recognize new acks
						return;
                                }
                        }
                }
        }
        if(SA[curr_client]==IEC_LOAD) {								//c64 wants to load?
                if(acknowledge[curr_client]!=0) {							//already got our okay for next package?
                        if(talklf[curr_client]!=0x0f) {						//file? yes, send file data
				/* fill up with new data if there's space left */
				while(out[curr_client]->size<DATA_PAYLOAD_SEND && !fs64_readchar(&logical_files[curr_client][talklf[curr_client]],&temp)) {			//still bytes free in packet?
//					if(fs64_readchar(&logical_files[curr_client][talklf[curr_client]],&temp)) break;	//oops, reached file end
                                	out[curr_client]->data[out[curr_client]->size]=temp;			//all okay, add byte
                                       	out[curr_client]->size++;
                                }
				if(out[curr_client]->size==0) {					//didn't manage to get a single byte into packet
					send_error(ERROR_EOI);				//so must be end of file
					set_error(0,0,0);
				}
				else {							//have some bytes, send them
					send_data(out[curr_client]);
				}
                        }
                        else {								//hmm, command, so send status
                                //send status until we reach end mark with 0x0d, then reset status
                                while(out[curr_client]->size<DATA_PAYLOAD_SEND && dos_stat_pos[curr_client]<dos_stat_len[curr_client]) {			//still place in packet?
//					printf("dos_stat_pos: %d    dos_stat_len:  %d\n", dos_stat_pos[curr_client], dos_stat_len[curr_client]);
					temp=dos_status[curr_client][dos_stat_pos[curr_client]];
		                        out[curr_client]->data[out[curr_client]->size]=temp;			//add byte
               			        out[curr_client]->size++;					 
		         //               if(temp==0x0d) {				//end reached?
			//			break;
			//		}
					dos_stat_pos[curr_client]++;			//count consumed byte
				}
				if(out[curr_client]->size==0) {				//didn't manage to get a single byte into packet
					send_error(ERROR_EOI);				//so must be end of file
//					printf("reset\n");
					set_error(0,0,0);
				}
				else {							//have some bytes, send them
					send_data(out[curr_client]);
				}
                        	//send_data(out[curr_client]);				//now send packet
                        }
                }
                acknowledge[curr_client]=0;                 	 			//allow next acknowledge again
        }
        return;
}

void iec_unlisten(struct packet* p) {
        myfilespec[curr_client][myfilespecsize[curr_client]]=0;							//finalize filename/command
        if(listenlf[curr_client]==0x0f) {
		strcpy((char*)dos_command[curr_client],(char*)myfilespec[curr_client]);
		dos_comm_len[curr_client]=myfilespecsize[curr_client];
		do_dos_command();							//try to execute command if we should
	}
        state[curr_client]=IEC_IDLE;
        return;
}

void iec_untalk(struct packet* p) {							//we just need to shut up, so let's IDLE
//	if(listenlf[curr_client]==0x0f) {
//		if(dos_status[curr_client][dos_stat_pos[curr_client]]==0x0d) 
//			set_error(0,0,0);						//reset status and send last packet
//	}
        state[curr_client]=IEC_IDLE;
        return;
}


void send_error(unsigned char err) {							//assemble error packet
        out[curr_client]->size=1;
        out[curr_client]->data[0]=err;
	out[curr_client]->type=PACKET_ERROR;
        send_packet(out[curr_client]);
	out[curr_client]->size=0;
        return;
}

void send_acknowledge() {								//assemble acknowledge packet
        out[curr_client]->size=1;
        out[curr_client]->data[0]=0x00;
	out[curr_client]->type=PACKET_ACKNOWLEDGE;
        send_packet(out[curr_client]);
	out[curr_client]->size=0;
        return;
}

void send_data(struct packet* p) {							//send data packet
	out[curr_client]->type=PACKET_DATA;
        send_packet(out[curr_client]);
	out[curr_client]->size=0;
	return;
}

void send_packet(struct packet* p) {
	unsigned char reply[p->size+2];
	int size;
	int i=0;
	switch(p->type) {
		case PACKET_DATA:							//data packet, so add size too (code, size, data)
			reply[0]=PACKET_DATA;
			reply[1]=p->size;
			while(i<p->size) { reply[2+i]=p->data[i]; i++; }
			size=p->size+2;
			transferred_amount[curr_client]+=p->size;
		break;
		default:								//other packets, just do code, data
			reply[0]=p->type;
			reply[1]=p->data[0];
			size=p->size+2;
		break;
	}
//if(size<32) size=32;	//gna. Avoid that too small packets are thrown away. Actually this somehow happens with packets that have 15 or 14 bytes payload. So this sucks rather hard :-(
		
	sendto(sendfd[curr_client], reply,size, 0, (struct sockaddr *) &sender[curr_client], sizeof(sender[curr_client]));
#ifdef DEBUG_COMM
	printf ("Packet type: ");
	switch(p->type) {
		case PACKET_DATA:
			printf("data");
		break;
		case PACKET_COMMAND:
			printf("command");
		break;
		case PACKET_ACKNOWLEDGE:
			printf("acknowledge");
		break;
		case PACKET_ERROR:
			printf("error");
		break;
	}
	printf("\nsent packet size: $%X\n", p->size);
	for(i=0;i<size;i++) {
		printf("$%02x ",reply[i]);
		if((i+1)%8==0) printf("\n");
	}
	if((i)%8!=0) printf("\n");

	printf("\n\n");
#endif
}

void begin_measure() {									//set measure start point
	transferred_amount[curr_client]=0;
	ftime(&start[curr_client]);
	return;
}

void end_measure() {									//end of measure, calculate time needed
	float time;
	float rate;
	long timediff;
	ftime(&end[curr_client]);
	timediff= (end[curr_client].time*1000+end[curr_client].millitm)-(start[curr_client].time*1000+start[curr_client].millitm);
					
	time=((float)timediff/(float)1000);
	rate=(float)transferred_amount[curr_client]/time;
	debug_msg("%ld bytes transferred in %.2fs => %.3f kb/s\n", transferred_amount[curr_client],time,rate/1024);
	return;	
}
	
int openfile(unsigned char* filespec, int mode) {						//try to open a file/dir/whatever
	begin_measure();
	if (logical_files[curr_client][listenlf[curr_client]].open == 1) {
		debug_msg ("*** Closing previous opened file first");
		closefile();
	}
	if (logical_files[curr_client][listenlf[curr_client]].open != 1) {				//open already?
		if (fs64_openfile_g((uchar*)"",filespec, &logical_files[curr_client][listenlf[curr_client]],mode)) {
			/* open failed, got to react on that */
			// try to interpret file as path + filename and try again
		//	int replace, m, par, dirflag;
		//	int dirtrack=-1, dirsect=-1;
		//	uchar path[1024];
		//	uchar glob[1024];
		//	if(!fs64_parse_filespec(filespec, path, glob, &dirflag, &m, &replace, &par, &dirtrack, &dirsect)) {
		//		if(!fs64_openfile_g(path,glob,&logical_files[curr_client][listenlf[curr_client]],mode)) {
		//			set_error(0,0,0);
		//			return 0;
		//		}
		//	}
			logical_files[curr_client][listenlf[curr_client]].open=0;
			if(mode==mode_WRITE) {
				return -1;
			}
			if(mode==mode_READ) {
				//maybe we just tried to load a dir?
				//if so, let's change to that dir!
				strcpy((char*)dos_command[curr_client],(char*)"CD:");
				strcat((char*)dos_command[curr_client],(char*)filespec);
				printf("filespec: %s  dos_command: %s\n",filespec,dos_command[curr_client]);	
				dos_comm_len[curr_client]=strlen(dos_command[curr_client]);
				
				/* prepare a empty dummy file we send, to make c64 happy TB */
				if(do_dos_command()==0) {
					out[curr_client]->data[0]=0x00;
					out[curr_client]->data[1]=0x00;
					out[curr_client]->size=2;
					return 1;
				}
				else {
					set_error(62,0,0);
				}
				
				return -1;
			}
		}
	}
	
	/* a zero size file, we better skip if it is not a dir */
	if(logical_files[curr_client][listenlf[curr_client]].blocks==0 && logical_files[curr_client][listenlf[curr_client]].isdir==0 && mode!=mode_WRITE) { 
		logical_files[curr_client][listenlf[curr_client]].open=0;
		return -1;
	}
	
	if (logical_files[curr_client][listenlf[curr_client]].open == 1) {
		petscii2ascii(filespec,strlen(filespec));
		debug_msg ("*** Opening [%s] on channel $%02x\n", filespec, listenlf[curr_client]);
		set_error(0,0,0);
		return 0;
	}
	return -1;
}

int closefile() {
//	printf("talklf: %d\n",talklf[curr_client]);
	if (logical_files[curr_client][listenlf[curr_client]].open == 1 || (talklf[curr_client]==0x0f)) {
		/* have we sent the status lately? so reset status after closing #15 */
                fs64_closefile_g (&logical_files[curr_client][listenlf[curr_client]]);
		logical_files[curr_client][listenlf[curr_client]].open=0;
		/* will be reset by iec_talk anyway */
//		if(dos_stat_pos[curr_client]>0 && talklf[curr_client]==0x0f) {
//		       	set_error(0,0,0);
//		}
        }
	end_measure();
	return 1;
}
