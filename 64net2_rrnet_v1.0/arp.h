#include "config.h"
#ifdef LINUX
#include <linux/sockios.h>
#endif
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <fcntl.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <net/ethernet.h>
#include <netinet/if_ether.h>
#ifdef LINUX
#include <netinet/ether.h>
#endif

int arp_set(char*,char*);
int arp_del(char*);
