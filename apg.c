/*
 * APG - ARP packet generator
 * Raffael Himmelreich <raffi@raffi.at>
 *
 * This software underlies the terms of the GPL
 * and is for testing purposes /only/.
 * Use it at your own risk!
*/

#ifndef __GNUC__
#error Sorry. GNU C extensions are required.
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <features.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <netinet/in.h>
#include <net/if_arp.h>
#include <stdlib.h>
#include <getopt.h>


struct arpdta {
	unsigned char ar_sha[6]; /* Sender hardware address. */
	unsigned char ar_sip[4]; /* Sender IP address.       */
	unsigned char ar_tha[6]; /* Target hardware address. */
	unsigned char ar_tip[4]; /* Target IP address.       */
};

int request, reply;
char *src_mac, *src_ip, *dst_mac, *dst_ip, *eth_src, *eth_dst;
char sha[6], sip[4], tha[6], tip[4];
char *device = "eth0";

int print_usage(char *exec_name)
{
	printf("Usage: %s <-r|-R> [option(s)]\n"\
	       "\n"\
	       "Valid options:\n"\
	       "\t-r, --quest\t\tSend ARP request.\n"\
	       "\t-R, --reply\t\tSend ARP reply.\n"\
	       "\t-e, --eth-src=MAC\tSet ethernet source address.\n"\
	       "\t-E, --eth-dst=MAC\tSet ethernet destination address.\n"\
	       "\t-m, --src-mac=MAC\tSet ARP source MAC address.\n"\
	       "\t-i, --src-ip=IP\t\tSet ARP source IP address.\n"\
	       "\t-M, --dst-mac=MAC\tSet ARP destination MAC address.\n"\
	       "\t-I, --dst-ip=IP\t\tSet ARP destination IP address.\n"\
	       "\t-d, --device=DEV\tSet device to use (default: eth0).\n"\
	       "\t-h, --help\t\tDisplay this help screen.\n", exec_name);
	return 0;
}

int parse_options(int argc, char **argv)
{
	int option;
	int option_index=0;
	struct option long_options[] = {
		{ "request", 0,	NULL, 'r' },
		{ "reply",   0,	NULL, 'R' },
		{ "src-mac", 1,	NULL, 'm' },
		{ "src-ip",  1,	NULL, 'i' },
		{ "dst-mac", 1,	NULL, 'M' },
		{ "dst-ip",  1,	NULL, 'I' },
		{ "eth-src", 1, NULL, 'e' },
		{ "eth-dst", 1, NULL, 'E' },
		{ "help",    0,	NULL, 'h' },
		{ "device",  1, NULL, 'd' },
		{  NULL,     0, NULL,  0  }
	};

	while (1) {
		option = getopt_long(argc, argv, "rhRm:i:M:I:e:E:d:", \
		                     long_options, &option_index);
		if (option == EOF) break;

		switch (option) {
			case 'r': request=1; break;
			case 'R': reply=1; break;
			case 'm': src_mac = optarg; break;
			case 'i': src_ip  = optarg; break;
			case 'M': dst_mac = optarg; break;
			case 'I': dst_ip  = optarg; break;
			case 'd': device  = optarg; break;
			case 'e': eth_src = optarg; break;
			case 'E': eth_dst = optarg; break;
			case 'h': print_usage(argv[0]); exit(0); break;
			default : exit(2);
		}
	 }

	return 0;
}


int read_hwadr(char *hwadr, char *dst)
{
	int i, dot_c=0, dig_c=0;
	char *oct, *tmp_oct;

	/* Check syntax of MAC address. */
	for (i=0; i<strlen(hwadr); i++) {
		switch (hwadr[i]) {
			case ':':         dot_c++;
			                  dig_c=-1;
			case 'A' ... 'F':
			case 'a' ... 'f':
			case '0' ... '9': break;
			default:          return -1;
		}
		dig_c++;
		if (dig_c > 2) return -1;
	}
	if (dot_c != 5) return -1;


	/* Convert strings to numbers and return them. */
	oct = hwadr;
	for (i=0; i<6; i++) {
		tmp_oct = strchr(oct, ':');
		if (i == 5) tmp_oct = strchr(oct, '\0');

		*tmp_oct = '\0';

		dst[i] = strtoul(oct, NULL, 16);
		oct = tmp_oct+1;
	}

	return 0;
}

int read_ipadr(char *ipadr, char *dst)
{
	char *oct, *tmp_oct;
	int i, dot_c=0, dig_c=0;

	/* Check syntax of IP address. */
	for (i=0; i<strlen(ipadr); i++) {
		switch (ipadr[i]) {
			case '.':         dot_c++;
			                  dig_c=-1;;
			case '0' ... '9': break;
			default:          return -1;
		}
		dig_c++;
		if (dig_c > 3) return -1;
	}
	if (dot_c != 3) return -1;


	/* Convert strings to numbers and return them. */
	oct = ipadr;
	for (i=0; i<4; i++) {
		tmp_oct = strchr(oct, '.');
		if (i == 3) tmp_oct = strchr(oct, '\0');

		*tmp_oct = '\0';

		dst[i] = strtoul(oct, NULL, 10);
		oct = tmp_oct+1;
	}

	return 0;
}


int init_socket(int *fd)
{
	*fd = socket(PF_PACKET, SOCK_RAW, 0);

	if (*fd < 0) {
		perror("socket");
		return -1;
	}

	return 0;
}

int init_device(char *device, int *ifindex, int fd_sock)
{
	struct ifreq ifr;
	
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, device, IFNAMSIZ-1);

	if (ioctl(fd_sock, SIOCGIFINDEX, &ifr) < 0) {
		fprintf(stderr, "unknown iface %s\n", device);
		return -1;
	}

	*ifindex = ifr.ifr_ifindex;

	ioctl(fd_sock, SIOCGIFADDR, &ifr);

	if (ioctl(fd_sock, SIOCGIFFLAGS, (char*)&ifr)) {
		perror("ioctl(SIOCGIFFLAGS)");
		return -1;
	}
	if (!(ifr.ifr_flags&IFF_UP)) {
		printf("Interface %s is down\n", device);
		return -1;
	}
	if (ifr.ifr_flags&(IFF_NOARP|IFF_LOOPBACK)) {
		printf("Interface %s is not ARPable\n", device);
		return -1;
	}

	ioctl(fd_sock, SIOCGIFHWADDR, &ifr);

	return 0;
}

int send_packet(int fd_sock, struct sockaddr_ll sa_dst, char *buf, int buflen)
{
	int n_bytes;

	if ((n_bytes = sendto(fd_sock, buf, buflen, \
	    0, (struct sockaddr *)&sa_dst, sizeof(sa_dst))) < 0) {
		perror("sendto");
		return -1;
	}

	printf("%d bytes sent.\n", n_bytes);

	return 0;
}


int main(int argc, char **argv)
{
	int fd_sock, ifindex;
	struct sockaddr_ll sa_dst;
	struct ether_header eth_hdr;
	struct arphdr arp_hdr;
	struct arpdta arp_dta;
	char buf[42];

	parse_options(argc, argv);

	if (!(request^reply)) {
		print_usage(*argv);
		exit(1);
	}
	if (init_socket(&fd_sock) < 0) {
		fprintf(stderr, "Socket initialisation failed!\n");
		exit(1);
	}
	if (init_device(device, &ifindex, fd_sock) < 0) {
		fprintf(stderr, "%s: Initialisation failed!\n", device);
		exit(1);
	}
	if (src_ip && read_ipadr(src_ip, sip) < 0) {
		fprintf(stderr, "Invalid ARP source IP address!\n");
		exit(1);
	}
	if (dst_ip && read_ipadr(dst_ip, tip) < 0) {
		fprintf(stderr, "Invalid ARP destination IP address!\n");
		exit(1);
	}
	if (src_mac && read_hwadr(src_mac, sha) < 0) {
		fprintf(stderr, "Invalid ARP source MAC address!\n");
		exit(1);
		
	}
	if (dst_mac && read_hwadr(dst_mac, tha) < 0) {
		fprintf(stderr, "Invalid ARP destination MAC address!\n");
		exit(1);
		
	}

	eth_hdr.ether_type = htons(ETHERTYPE_ARP);
	memset(eth_hdr.ether_dhost, 0, 6);
	memset(eth_hdr.ether_shost, 0, 6);

	if (eth_src && read_hwadr(eth_src, eth_hdr.ether_shost) < 0) {
		fprintf(stderr, "Invalid ethernet source MAC address!\n");
		exit(1);
	}
	if (eth_dst && read_hwadr(eth_dst, eth_hdr.ether_dhost) < 0) {
		fprintf(stderr, "Invalid ethernet destination MAC address!\n");
		exit(1);
	}

	
	arp_hdr.ar_hrd = htons(ARPHRD_ETHER);
	arp_hdr.ar_pro = htons(ETH_P_IP);
	arp_hdr.ar_hln = 6;
	arp_hdr.ar_pln = 4;
	if (request) arp_hdr.ar_op = htons(ARPOP_REQUEST);
	else         arp_hdr.ar_op = htons(ARPOP_REPLY);
		
	memset(arp_dta.ar_sha, 0, 6);
	memset(arp_dta.ar_sip, 0, 4);
	memset(arp_dta.ar_tha, 0, 6);
	memset(arp_dta.ar_tip, 0, 4);

	if (sha) memcpy(arp_dta.ar_sha, sha, 6);
	if (sip) memcpy(arp_dta.ar_sip, sip, 4);
	if (tha) memcpy(arp_dta.ar_tha, tha, 6);
	if (tip) memcpy(arp_dta.ar_tip, tip, 4);

	memcpy(buf, &eth_hdr, sizeof(eth_hdr));
	memcpy(buf+sizeof(eth_hdr), &arp_hdr, sizeof(arp_hdr));
	memcpy(buf+sizeof(eth_hdr)+sizeof(arp_hdr), &arp_dta, sizeof(arp_dta));

	sa_dst.sll_family = AF_PACKET;
	sa_dst.sll_ifindex = ifindex;
	sa_dst.sll_protocol = htons(ETH_P_ARP);
	if (sha) memcpy(sa_dst.sll_addr , sha, 6);
	sa_dst.sll_halen = 2;

	send_packet(fd_sock, sa_dst, buf, sizeof(buf));

	close(fd_sock);

	exit(0);
}
