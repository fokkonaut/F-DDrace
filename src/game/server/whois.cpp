// original version has been created by noby

#include "whois.h"
#include <game/server/gamecontext.h>
#include <engine/shared/config.h>

#include <base/detect.h>

#if defined(CONF_FAMILY_WINDOWS)
#include <io.h>
#else
#include <unistd.h>
#endif
#include <string.h>
#include <iostream>
#include<stdio.h>
#include<fcntl.h>

IServer *CWhoIs::Server() const { return GameServer()->Server(); }

void CWhoIs::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;

	num_nms = num_ips = 0;
	if (!(ipplayers = (struct player *)calloc(sizeof(struct player), GameServer()->Config()->m_SvWhoIsIPEntries))) {
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "whois", "init_ips: couldnt allocate list");
		return;
	}
	if (!(nmplayers = (struct player *)calloc(sizeof(struct player), GameServer()->Config()->m_SvWhoIsIPEntries))) {
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "whois", "init_ips: couldnt allocate list");
		return;
	}
	int ind, src_fd, rr;
	char addr_buf[32], name_buf[32];
	char aFile[128];
	str_format(aFile, sizeof(aFile), "%s/whois.txt", GameServer()->Config()->m_SvWhoIsFile);
	if ((src_fd = open(aFile, O_RDONLY, 0777)) < 0) {
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "whois", "init_ips: couldnt open");
		return;
	}
	char *dst = (char *)calloc(20000000, 1), *ptr = dst;	
	if (!dst) {
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "whois", "init_ips: couldnt allocate buffer");
		return;
	}
	if ((rr = (int)read(src_fd, dst, 20000000)) <= 0) {
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "whois", "init_ips: couldnt read");
		free(dst);
		return;
	}
	close(src_fd);
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "read %d bytes", rr);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "whois", aBuf);
	if (rr < 1) {
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "whois", "too small, waiting..");
		free(dst);
		return;
	}

	do {
		memset(addr_buf, 0, sizeof(addr_buf));
		memset(name_buf, 0, sizeof(name_buf));
		while (*ptr && !isdigit(*ptr) && *ptr != '\n' && (int)(ptr - dst) < (rr - 16))
			ptr++;
		if (!*ptr)
			break;
		for (ind = 0; *ptr && *ptr != ')' && ind < 15; )
			addr_buf[ind++] = *ptr++;
		if (!*ptr)
			break;
		addr_buf[ind] = 0;
		ptr += 3;
		for (ind = 0; *ptr && *ptr != '\n' && ind < 15; )
			name_buf[ind++] = *ptr++;
		if (!*ptr)
			break;
		name_buf[ind] = 0;
		ptr++;
		if (!*ptr)
			break;
		if (!((int)(ptr - dst) < (rr - 19)))
			break;
			
		if (AddGeneric(nmplayers, ipplayers, &num_ips, &num_nms, name_buf, addr_buf) < 0)
			break;
	} while ((int)(ptr - dst) < (rr - 20));
	free(dst);

	char buf[256];
	snprintf(buf, sizeof(buf), "read %d ips and %d names", num_ips, num_nms);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whois", buf);
	return;
}

void CWhoIs::Run(const char *name, int mode, int cutoff)
{
	char buf[1024] = { 0 }, buf2[64], nname[64] = { 0 };
	int lenc = (int)strlen(name), found = 0;
	int numi = (mode) ? num_nms : num_ips;

	if (!numi || !lenc) {
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whois", "invalid");
		return;
	}
	strncpy(nname, name, sizeof(nname));
	if (!mode && cutoff > 0 && cutoff < 3) {
		for (int i = 0; i < cutoff; i++) {
			int tmp = (int)strlen(nname) - 1;
			for ( ; nname[tmp] != '.' && tmp >= 0; --tmp) 
				;
			nname[tmp] = 0;
		}
	}

	if (!(lenc = (int)strlen(nname))) {
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whois", "invalid");
		return;
	}

	struct player *p = (mode) ? nmplayers : ipplayers;
	for (int i = 0; i < numi; i++) {
		if ((cutoff || (p[i].first3 == lenc)) && !strncmp(p[i].name, nname, lenc)) {
		    	snprintf(buf, sizeof(buf), "%s connected %d times with %d %s: ", 
		    		p[i].name, p[i].count, p[i].numn, mode ? "ips" : "names");
			for (int j = 0; j < p[i].numn; j++) {
				memset(buf2, 0, sizeof(buf2));
				snprintf(buf2, sizeof(buf2), "%s (%d)", 
					p[i].names[j].addr, p[i].names[j].count);
				if (strlen(buf) >= 200) {
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whois", buf);
					memset(buf, 0, sizeof(buf));
				} else {
					if (j > 0)
						strcat(buf, ", ");
				}
				strcat(buf, buf2);
			}
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whois", buf);
			found = 1;
		}
	}
	
	if (!found) {
		str_format(buf, sizeof(buf), "could not find %s %s", mode ? "name" : "ip", nname);
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whois", buf);
	}
}

void CWhoIs::AddEntry(int ClientID)
{
	char aIP[16] = { 0 }, aBuf[128] = { 0 };
    memset(aBuf, 0, sizeof(aBuf));
    Server()->GetClientAddr(ClientID, aIP, sizeof(aIP));
    char aname[16] = { 0 };
    strncpy(aname, Server()->ClientName(ClientID), 15);
    str_format(aBuf, sizeof(aBuf), "(%16s): %s\n", aIP, aname);
    int fd, sl = (int)strlen(aBuf);
    if (GameServer()->Config()->m_SvWhoIs) {
		char aFile[128];
		str_format(aFile, sizeof(aFile), "%s/whois.txt", GameServer()->Config()->m_SvWhoIsFile);
        if ((fd = open(aFile, O_RDWR|O_CREAT|O_APPEND, 0777)) < 0)
            perror("open");
        else
            if ((int)write(fd, aBuf, sl) != sl)
                perror("write");
        close(fd);

		AddEntry(Server()->ClientName(ClientID), aIP);
    }
}

int CWhoIs::AddGeneric(struct player *names, struct player *addrs, int *ppl, int *ppl1, const char *name_buf, const char *addr_buf)
{
	struct player *tp;
	int name_bufl = (int)strlen(name_buf);
	int addr_bufl = (int)strlen(addr_buf);
	int pl = *ppl;
	int pl1 = *ppl1;
	if (!name_bufl || !addr_bufl || name_bufl > 15 || addr_bufl > 15 ||
	    pl >= GameServer()->Config()->m_SvWhoIsIPEntries || pl1 >= GameServer()->Config()->m_SvWhoIsIPEntries)
		return 0;
	
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
	if ((tp = FindEntry(addrs, pl, addr_buf)) == NULL) {
		struct player *dst = &(addrs[pl]);
		strncpy(dst->name, addr_buf, 16);
		strncpy(dst->names[0].addr, name_buf, 16);
		dst->first3 = addr_bufl;
		dst->names[0].first3 = name_bufl;
		dst->count = dst->names[0].count = dst->numn = 1;
		if (pl >= GameServer()->Config()->m_SvWhoIsIPEntries)
			return -1;
		tp = &addrs[pl++];
	} else {
		AddName(tp, name_buf);
		tp->count++;
	}
	
	if ((tp = FindEntry(names, pl1, name_buf)) == NULL) {
		struct player *dst = &(names[pl1]);
		strncpy(dst->name, name_buf, 16);
		strncpy(dst->names[0].addr, addr_buf, 16);
		dst->first3 = name_bufl;
		dst->names[0].first3 = addr_bufl;
		dst->count = dst->names[0].count = dst->numn = 1;
		if (pl1 >= GameServer()->Config()->m_SvWhoIsIPEntries)
			return -1;
		tp = &names[pl1++];
	} else {
		AddName(tp, addr_buf);
		tp->count++;
	}
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif

	*ppl1 = pl1;
	*ppl = pl;
	return 0;
}

struct CWhoIs::player *CWhoIs::FindEntry(struct player *src, int num, const char *name)
{
	if (!src || !name)
		return NULL;
	int len = (int)strlen(name);
	if (len > 16 || len <= 0)
		return NULL;
	
	//char tmp[16] = { 0 };
	//int ind, f3 = -1;
	////strncpy(tmp, name, 16);
	//for (ind = 0; tmp[ind] != '.' && ind < 16; ind++)
	//	;
	//tmp[ind] = 0;
	//
	for (int ind = 0; ind < num; ind++)
		if (len == src[ind].first3 && 
		    !memcmp(name, src[ind].name, len))
			return &src[ind];
	return NULL;
}

int CWhoIs::AddName(struct CWhoIs::player *src, const char *name)
{
	if (!src || !name)
		return -1;
	int len = (int)strlen(name), i;
	if (len > 16 || len <= 0)
		return -1;
	for (i = 0; i < src->numn; i++) {
		if (len == src->names[i].first3 && 
		    !memcmp(name, src->names[i].addr, len)) {
			src->names[i].count++;
			return 1;
		}
	}
	if (i < NAMEC) {
		strncpy(src->names[i].addr, name, 16);
		src->names[i].count = 1;
		src->names[i].first3 = len;
		src->numn++;
		return 0;
	}
	return -1;
}

int CWhoIs::AddEntry(const char *name, const char *addr)
{
	int retn = AddGeneric(nmplayers, ipplayers, &num_ips, &num_nms, name, addr);
	if (retn < 0)
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "whois", "error adding");

	return retn;
}
