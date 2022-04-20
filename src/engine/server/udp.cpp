#include "udp.h"

/*
	Raw UDP sockets
*/
#include <stdint.h>
#include<string.h> //memset
#include <stdlib.h>

#if defined(CONF_FAMILY_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>

struct udphdr
{
	u_short source; /* src port */
	u_short dest; /* dst port */
	short len; /* udp length */
	u_short check; /* checksum */
};

struct iphdr
{
#if defined(CONF_ARCH_ENDIAN_LITTLE)
	uint8_t ihl:4,version:4;
#else
	uint8_t version:4,ihl:4;
#endif
	uint8_t tos;
	uint16_t tot_len;
	uint16_t id;
	uint16_t frag_off;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t check;
	uint32_t saddr;
	uint32_t daddr;
};

#else
#include<sys/socket.h> //for socket ofcourse
#include<netinet/udp.h> //Provides declarations for udp header
#include<netinet/ip.h> //Provides declarations for ip header
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // close()
#endif

#define PCKT_LEN 8192   // Buffer length. Note - This isn't how much data we're actually sending.

/*
	96 bit (12 bytes) pseudo header needed for udp header checksum calculation
*/
struct pseudo_header
{
	uint32_t source_address;
	uint32_t dest_address;
	uint8_t placeholder;
	uint8_t protocol;
	uint16_t udp_length;
};

/* Checksum function */
unsigned short csum(unsigned short *buf, int nwords)
{
	unsigned long sum;

	for (sum = 0; nwords > 0; nwords--)
	{
		sum += *buf++;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return (unsigned short) (~sum);
}

int SendPacketUDP(NETADDR *pSource, NETADDR *pDestination, void *pData, int Amount)
{
	// Create the socket.
	int sockfd = socket(PF_INET, SOCK_RAW, IPPROTO_UDP);
	if (sockfd == -1)
	{
		//socket creation failed, may be because of non-root privileges
		return 1;
	}

	// Let the socket know we want to pass our own headers.
	int one = 1;
	if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, (const char *)&one, sizeof(one)) < 0)
	{
		// socket option error (header include)
		return 1;
	}

	// Set packet buffer to 0's.
	char buffer[PCKT_LEN];
	memset(buffer, 0, PCKT_LEN);

	struct iphdr *iphdr = (struct iphdr *)buffer;
	struct udphdr *udphdr = (struct udphdr *)(buffer + sizeof(struct iphdr));
	char *pcktData = (char *)(buffer + sizeof(struct iphdr) + sizeof(struct udphdr));

	// Check for data.
	int pcktDataLen = 0;

	// Adds data to the packet's payload (after IP + UDP headers).
	for (int i = 0; i < (int)strlen((char *)pData); i++)
	{
		*pcktData++ = ((char *)pData)[i];
		pcktDataLen++;
	}

	char sourceaddr[32], destaddr[32];
	net_addr_str(pSource, sourceaddr, sizeof(sourceaddr), 0);
	net_addr_str(pDestination, destaddr, sizeof(destaddr), 0);

	// Fill out source sockaddr in (IPv4, source address, and source port).
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(sourceaddr);
	sin.sin_port = htons(pSource->port);
	memset(&sin.sin_zero, 0, sizeof(sin.sin_zero));

	// Let's fill out the IP and UDP headers C:

	// IP Header.
	iphdr->ihl = 5;
	iphdr->version = 4;
	iphdr->tos = 16;
	iphdr->tot_len = sizeof(struct iphdr) + sizeof(struct udphdr) + pcktDataLen;
	iphdr->id = htons(54321);
	iphdr->ttl = 64;
	iphdr->protocol = IPPROTO_UDP;
	iphdr->saddr = inet_addr(sourceaddr);
	iphdr->daddr = inet_addr(destaddr);

	// UDP Header.
	udphdr->source = htons(pSource->port);
	udphdr->dest = htons(pDestination->port);
	udphdr->len = htons(sizeof(struct udphdr) + pcktDataLen);

	// IP Checksum.
	iphdr->check = csum((unsigned short *)buffer, sizeof(struct iphdr) + sizeof(struct udphdr));

	// UDP Checksum
	struct pseudo_header pshdr;
	pshdr.source_address = inet_addr(sourceaddr);
	pshdr.dest_address = inet_addr(destaddr);
	pshdr.placeholder = 0;
	pshdr.protocol = IPPROTO_UDP;
	pshdr.udp_length = htons(sizeof(struct udphdr) + pcktDataLen);

	int pssize = sizeof(struct pseudo_header) + sizeof(struct udphdr) + pcktDataLen;
	char *pseudogram = (char *)malloc(pssize);

	memcpy(pseudogram, &pshdr, sizeof(struct pseudo_header));
	memcpy(pseudogram + sizeof(struct pseudo_header), udphdr, sizeof(struct udphdr) + pcktDataLen);
	udphdr->check = csum((unsigned short *)pseudogram, pssize);

	int Error = 0;
	for (int i = 0; i < Amount; i++)
		if (sendto(sockfd, buffer, iphdr->tot_len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		{
			Error = 1;
			break;
		}

#if defined(CONF_FAMILY_WINDOWS)
	closesocket(sockfd);
#else
	close(sockfd);
#endif
	return Error;
}
