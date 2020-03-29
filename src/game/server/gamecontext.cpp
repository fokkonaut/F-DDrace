/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <engine/shared/config.h>
#include <engine/shared/memheap.h>
#include <engine/shared/datafile.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <engine/map.h>

#include <generated/server_data.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/version.h>

#include "entities/character.h"
#include "gamemodes/DDRace.h"
#include "gamecontext.h"
#include "player.h"

#include <game/server/entities/flag.h>
#include <game/server/entities/lasertext.h>
#include <fstream>
#include <limits>
#include <string>
#include <stdio.h>
#include <stdlib.h>

#include "score.h"
#include "score/file_score.h"

#if defined (CONF_SQL)
	#include "score/sql_score.h"
#endif

#include <engine/server/server.h>
#include <string.h>

enum
{
	RESET,
	NO_RESET
};

void CGameContext::Construct(int Resetting)
{
	m_Resetting = 0;
	m_pServer = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_apPlayers[i] = 0;

	m_pController = 0;
	m_VoteCloseTime = 0;
	m_VoteCancelTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	m_LastMapVote = 0;
	m_LockTeams = 0;

	if(Resetting==NO_RESET)
	{
		m_pVoteOptionHeap = new CHeap();
		m_pScore = 0;
		m_NumMutes = 0;
		m_NumVoteMutes = 0;
		m_pShop = 0;
	}

	m_aDeleteTempfile[0] = 0;
	m_ChatResponseTargetID = -1;
	m_TeeHistorianActive = false;

	m_pRandomMapResult = nullptr;
	m_pMapVoteResult = nullptr;

	// F-DDrace
	m_pShop = 0;
}

CGameContext::CGameContext(int Resetting)
{
	Construct(Resetting);
}

CGameContext::CGameContext()
{
	Construct(NO_RESET);
}

CGameContext::~CGameContext()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		delete m_apPlayers[i];
	if(!m_Resetting)
		delete m_pVoteOptionHeap;

	if (m_pScore)
		delete m_pScore;
	if (m_pShop)
		delete m_pShop;
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;

	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new (this) CGameContext(RESET);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;
}


void CGameContext::TeeHistorianWrite(const void *pData, int DataSize, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;
	io_write(pSelf->m_TeeHistorianFile, pData, DataSize);
}

void CGameContext::CommandCallback(int ClientID, int FlagMask, const char *pCmd, IConsole::IResult *pResult, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;
	if(pSelf->m_TeeHistorianActive)
	{
		pSelf->m_TeeHistorian.RecordConsoleCommand(ClientID, FlagMask, pCmd, pResult);
	}
}

class CCharacter *CGameContext::GetPlayerChar(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return 0;
	return m_apPlayers[ClientID]->GetCharacter();
}

void CGameContext::CreateDamage(vec2 Pos, int Id, vec2 Source, int HealthAmount, int ArmorAmount, bool Self, int64_t Mask)
{
	float f = angle(Source);
	CNetEvent_Damage *pEvent = (CNetEvent_Damage *)m_Events.Create(NETEVENTTYPE_DAMAGE, sizeof(CNetEvent_Damage), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = Id;
		pEvent->m_Angle = (int)(f*256.0f);
		pEvent->m_HealthAmount = clamp(HealthAmount, 0, 9);
		pEvent->m_ArmorAmount = clamp(ArmorAmount, 0, 9);
		pEvent->m_Self = Self;
	}
}

void CGameContext::CreateHammerHit(vec2 Pos, int64_t Mask)
{
	// create the event
	CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}


void CGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, int64_t Mask)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	// deal damage
	CCharacter *apEnts[MAX_CLIENTS];
	float Radius = g_pData->m_Explosion.m_Radius;
	float InnerRadius = 48.0f;
	int Num = m_World.FindEntities(Pos, Radius, (CEntity * *)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	int64_t TeamMask = -1;
	for (int i = 0; i < Num; i++)
	{
		vec2 Diff = apEnts[i]->GetPos() - Pos;
		vec2 ForceDir(0, 1);
		float l = length(Diff);
		if (l)
			ForceDir = normalize(Diff);
		l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		float Strength;
		if (Owner == -1 || !m_apPlayers[Owner] || !m_apPlayers[Owner]->m_TuneZone)
			Strength = Tuning()->m_ExplosionStrength;
		else
			Strength = TuningList()[m_apPlayers[Owner]->m_TuneZone].m_ExplosionStrength;

		float Dmg = Strength * l;
		if (!(int)Dmg) continue;

		if ((GetPlayerChar(Owner) ? !(GetPlayerChar(Owner)->m_Hit & CCharacter::DISABLE_HIT_GRENADE) : Config()->m_SvHit || NoDamage) || Owner == apEnts[i]->GetPlayer()->GetCID())
		{
			if (Owner != -1 && apEnts[i]->IsAlive() && !apEnts[i]->CanCollide(Owner)) continue;
			if (Owner == -1 && ActivatedTeam != -1 && apEnts[i]->IsAlive() && apEnts[i]->Team() != ActivatedTeam) continue;

			// Explode at most once per team
			int PlayerTeam = ((CGameControllerDDRace*)m_pController)->m_Teams.m_Core.Team(apEnts[i]->GetPlayer()->GetCID());
			if (GetPlayerChar(Owner) ? GetPlayerChar(Owner)->m_Hit & CCharacter::DISABLE_HIT_GRENADE : !Config()->m_SvHit || NoDamage)
			{
				if (!CmaskIsSet(TeamMask, PlayerTeam)) continue;
				TeamMask = CmaskUnset(TeamMask, PlayerTeam);
			}

			apEnts[i]->TakeDamage(ForceDir * Dmg * 2, ForceDir*-1, (int)Dmg, Owner, Weapon);
		}
	}
}

void CGameContext::CreatePlayerSpawn(vec2 Pos, int64_t Mask)
{
	// create the event
	CNetEvent_Spawn *pEvent = (CNetEvent_Spawn *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDeath(vec2 Pos, int ClientID, int64_t Mask)
{
	// create the event
	CNetEvent_Death *pEvent = (CNetEvent_Death *)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(CNetEvent_Death), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = ClientID;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, int64_t Mask)
{
	if (Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SoundID = Sound;
	}
}

void CGameContext::SendChatTarget(int To, const char *pText)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = CHAT_ALL;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = pText;
	Msg.m_TargetID = To;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
}

void CGameContext::SendChatTeam(int Team, const char *pText)
{
	for(int i = 0; i<MAX_CLIENTS; i++)
		if(((CGameControllerDDRace*)m_pController)->m_Teams.m_Core.Team(i) == Team)
			SendChatTarget(i, pText);
}

void CGameContext::SendChat(int ChatterClientID, int Mode, int To, const char *pText, int SpamProtectionClientID)
{
	if (SpamProtectionClientID >= 0 && SpamProtectionClientID < MAX_CLIENTS)
		if (ProcessSpamProtection(SpamProtectionClientID))
			return;

	char aBuf[256], aText[256];
	str_copy(aText, pText, sizeof(aText));
	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Mode, Server()->ClientName(ChatterClientID), aText);
	else if (ChatterClientID == -2)
	{
		str_format(aBuf, sizeof(aBuf), "### %s", aText);
		str_copy(aText, aBuf, sizeof(aText));
		ChatterClientID = -1;
	}
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", aText);

	const char *pModeStr;
	if(Mode == CHAT_WHISPER)
		pModeStr = 0;
	else if(Mode == CHAT_TEAM)
		pModeStr = "teamchat";
	else if(Mode == CHAT_SINGLE)
		pModeStr = "chat-single";
	else
		pModeStr = "chat";

	if(pModeStr)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, pModeStr, aBuf);
	}


	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = Mode;
	Msg.m_ClientID = ChatterClientID;
	Msg.m_pMessage = aText;
	Msg.m_TargetID = -1;

	if(Mode == CHAT_ALL)
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
	else if(Mode == CHAT_TEAM)
	{
		CTeamsCore* Teams = &((CGameControllerDDRace*)m_pController)->m_Teams.m_Core;
		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NOSEND, -1);

		// send to the clients
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] != 0)
			{
				if(m_apPlayers[ChatterClientID]->GetTeam() == TEAM_SPECTATORS)
				{
					if(m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
					{
						Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
					}
				}
				else
				{
					if(Teams->Team(i) == GetDDRaceTeam(ChatterClientID) && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
					{
						Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
					}
				}
			}
		}
	}
	else if (Mode == CHAT_SINGLE)
	{
		// send to the clients
		Msg.m_TargetID = To;
		Msg.m_Mode = CHAT_ALL;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
		if (ChatterClientID >= 0)
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ChatterClientID);
	}
	else // Mode == CHAT_WHISPER
	{
		// send to the clients
		Msg.m_TargetID = To;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ChatterClientID);
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
	}
}

void CGameContext::SendBroadcast(const char* pText, int ClientID, bool IsImportant)
{
	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = pText;

	if (ClientID == -1)
	{
		dbg_assert(IsImportant, "broadcast messages to all players must be important");
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i])
			{
				m_apPlayers[i]->m_LastBroadcastImportance = true;
				m_apPlayers[i]->m_LastBroadcast = Server()->Tick();
			}
		}
		return;
	}

	if (!m_apPlayers[ClientID])
		return;

	if (!IsImportant && m_apPlayers[ClientID]->m_LastBroadcastImportance && m_apPlayers[ClientID]->m_LastBroadcast > Server()->Tick() - Server()->TickSpeed() * 10)
		return;

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	m_apPlayers[ClientID]->m_LastBroadcast = Server()->Tick();
	m_apPlayers[ClientID]->m_LastBroadcastImportance = IsImportant;
}

void CGameContext::SendEmoticon(int ClientID, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Emoticon = Emoticon;
	if (m_apPlayers[ClientID])
	{
		if (m_apPlayers[ClientID]->m_SpookyGhost || (GetPlayerChar(ClientID) && GetPlayerChar(ClientID)->m_Spooky))
			Msg.m_Emoticon = EMOTICON_GHOST;
		else if (GetPlayerChar(ClientID) && GetPlayerChar(ClientID)->GetActiveWeapon() == WEAPON_HEART_GUN)
			Msg.m_Emoticon = EMOTICON_HEARTS;
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendWeaponPickup(int ClientID, int Weapon)
{
	if (Weapon >= NUM_VANILLA_WEAPONS)
	{
		if (GetPlayerChar(ClientID))
			GetPlayerChar(ClientID)->SetWeapon(Weapon);
	}
	else
	{
		CNetMsg_Sv_WeaponPickup Msg;
		Msg.m_Weapon = Weapon;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
}

void CGameContext::SendSettings(int ClientID)
{
	CNetMsg_Sv_ServerSettings Msg;
	Msg.m_KickVote = Config()->m_SvVoteKick;
	Msg.m_KickMin = Config()->m_SvVoteKickMin;
	Msg.m_SpecVote = Config()->m_SvVoteSpectate;
	Msg.m_TeamLock = m_LockTeams != 0;
	Msg.m_TeamBalance = 0;
	Msg.m_PlayerSlots = Config()->m_SvPlayerSlots;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendSkinChange(CPlayer::TeeInfos pTeeInfos, int ClientID, int TargetID)
{
	CNetMsg_Sv_SkinChange Msg;
	Msg.m_ClientID = ClientID;
	for(int p = 0; p < NUM_SKINPARTS; p++)
	{
		Msg.m_apSkinPartNames[p] = pTeeInfos.m_aaSkinPartNames[p];
		Msg.m_aUseCustomColors[p] = pTeeInfos.m_aUseCustomColors[p];
		Msg.m_aSkinPartColors[p] = pTeeInfos.m_aSkinPartColors[p];
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, TargetID);

	// F-DDrace
	if (m_apPlayers[ClientID])
		m_apPlayers[ClientID]->m_CurrentInfo.m_TeeInfos = pTeeInfos;
}

void CGameContext::SendGameMsg(int GameMsgID, int ClientID)
{
	CMsgPacker Msg(NETMSGTYPE_SV_GAMEMSG);
	Msg.AddInt(GameMsgID);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendGameMsg(int GameMsgID, int ParaI1, int ClientID)
{
	CMsgPacker Msg(NETMSGTYPE_SV_GAMEMSG);
	Msg.AddInt(GameMsgID);
	Msg.AddInt(ParaI1);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendGameMsg(int GameMsgID, int ParaI1, int ParaI2, int ParaI3, int ClientID)
{
	CMsgPacker Msg(NETMSGTYPE_SV_GAMEMSG);
	Msg.AddInt(GameMsgID);
	Msg.AddInt(ParaI1);
	Msg.AddInt(ParaI2);
	Msg.AddInt(ParaI3);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendChatCommand(const CCommandManager::CCommand *pCommand, int ClientID)
{
	CNetMsg_Sv_CommandInfo Msg;
	Msg.m_Name = pCommand->m_aName;
	Msg.m_HelpText = pCommand->m_aHelpText;
	Msg.m_ArgsFormat = pCommand->m_aArgsFormat;

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendChatCommands(int ClientID)
{
	// F-DDrace
	// Remove the clientside commands (expect w and whisper)
	{
		SendRemoveChatCommand("all", ClientID);
		SendRemoveChatCommand("friend", ClientID);
		SendRemoveChatCommand("m", ClientID);
		SendRemoveChatCommand("mute", ClientID);
		SendRemoveChatCommand("r", ClientID);
		SendRemoveChatCommand("team", ClientID);
	}

	for(int i = 0; i < CommandManager()->CommandCount(); i++)
	{
		SendChatCommand(CommandManager()->GetCommand(i), ClientID);
	}
}

void CGameContext::SendRemoveChatCommand(const CCommandManager::CCommand *pCommand, int ClientID)
{
	CNetMsg_Sv_CommandInfoRemove Msg;
	Msg.m_Name = pCommand->m_aName;

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendRemoveChatCommand(const char *pName, int ClientID)
{
	CNetMsg_Sv_CommandInfoRemove Msg;
	Msg.m_Name = pName;

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

//
void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->m_Vote = 0;
			m_apPlayers[i]->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq()*VOTE_TIME;
	m_VoteCancelTime = time_get() + time_freq()*VOTE_CANCEL_TIME;
	str_copy(m_aVoteDescription, pDesc, sizeof(m_aVoteDescription));
	str_copy(m_aVoteCommand, pCommand, sizeof(m_aVoteCommand));
	str_copy(m_aVoteReason, pReason, sizeof(m_aVoteReason));
	SendVoteSet(m_VoteType, -1);
	m_VoteUpdate = true;
}

void CGameContext::EndVote(int Type, bool Force)
{
	m_VoteCloseTime = 0;
	m_VoteCancelTime = 0;
	if(Force)
		m_VoteCreator = -1;
	SendVoteSet(Type, -1);
}

void CGameContext::ForceVote(int Type, const char *pDescription, const char *pReason)
{
	CNetMsg_Sv_VoteSet Msg;
	Msg.m_Type = Type;
	Msg.m_Timeout = 0;
	Msg.m_ClientID = -1;
	Msg.m_pDescription = pDescription;
	Msg.m_pReason = pReason;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendVoteSet(int Type, int ToClientID)
{
	CNetMsg_Sv_VoteSet Msg;
	if(m_VoteCloseTime)
	{
		Msg.m_ClientID = m_VoteCreator;
		Msg.m_Type = Type;
		Msg.m_Timeout = (m_VoteCloseTime-time_get())/time_freq();
		Msg.m_pDescription = m_aVoteDescription;
		Msg.m_pReason = m_aVoteReason;
	}
	else
	{
		Msg.m_Type = Type;
		Msg.m_Timeout = 0;
		Msg.m_ClientID = m_VoteCreator;
		Msg.m_pDescription = "";
		Msg.m_pReason = "";
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ToClientID);
}

void CGameContext::SendVoteStatus(int ClientID, int Total, int Yes, int No)
{
	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes+No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

}

void CGameContext::AbortVoteOnDisconnect(int ClientID)
{
	if(m_VoteCloseTime && ClientID == m_VoteClientID && (str_startswith(m_aVoteCommand, "kick ") ||
		str_startswith(m_aVoteCommand, "set_team ") || (str_startswith(m_aVoteCommand, "ban ") && Server()->IsBanned(ClientID))))
		m_VoteCloseTime = -1;
}

void CGameContext::AbortVoteOnTeamChange(int ClientID)
{
	if(m_VoteCloseTime && ClientID == m_VoteClientID && str_startswith(m_aVoteCommand, "set_team "))
		m_VoteCloseTime = -1;
}

void CGameContext::SendTuningParams(int ClientID, int Zone)
{
	if (ClientID == -1)
	{
		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (m_apPlayers[i])
			{
				if (m_apPlayers[i]->GetCharacter())
				{
					if (m_apPlayers[i]->GetCharacter()->m_TuneZone == Zone)
						SendTuningParams(i, Zone);
				}
				else if (m_apPlayers[i]->m_TuneZone == Zone)
				{
					SendTuningParams(i, Zone);
				}
			}
		}
		return;
	}

	static CTuningParams Tuning;

	if (Zone == 0)
		Tuning = m_Tuning;
	else
		Tuning = m_aTuningList[Zone];

	// F-DDrace
	CCharacter *pChr = GetPlayerChar(ClientID);
	if (pChr)
	{
		if (pChr->m_FakeTuneCollision)
			Tuning.m_PlayerCollision = 0.f;
		if (pChr->m_Passive && !pChr->m_Super)
			Tuning.m_PlayerHooking = 0.f;

		if ((pChr->m_FreezeTime && Config()->m_SvFreezePrediction) || pChr->GetPlayer()->m_TeeControllerID != -1)
		{
			Tuning.m_GroundControlSpeed = 0.f;
			Tuning.m_GroundJumpImpulse = 0.f;
			Tuning.m_GroundControlAccel = 0.f;
			Tuning.m_AirControlSpeed = 0.f;
			Tuning.m_AirJumpImpulse = 0.f;
			Tuning.m_AirControlAccel = 0.f;
			Tuning.m_HookFireSpeed = 0.f;
		}
	}

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int* pParams = (int*)&Tuning;

	unsigned int last = sizeof(m_Tuning) / sizeof(int);
	for (unsigned i = 0; i < last; i++)
		Msg.AddInt(pParams[i]);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::OnTick()
{
	if(m_TeeHistorianActive)
	{
		if(!m_TeeHistorian.Starting())
		{
			m_TeeHistorian.EndInputs();
			m_TeeHistorian.EndTick();
		}
		io_flush(m_TeeHistorianFile);
		m_TeeHistorian.BeginTick(Server()->Tick());
		m_TeeHistorian.BeginPlayers();
	}

	// copy tuning
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();

	//if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	if(m_TeeHistorianActive)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && m_apPlayers[i]->GetCharacter())
			{
				CNetObj_CharacterCore Char;
				m_apPlayers[i]->GetCharacter()->GetCore().Write(&Char);
				m_TeeHistorian.RecordPlayer(i, &Char);
			}
			else
			{
				m_TeeHistorian.RecordDeadPlayer(i);
			}
		}
		m_TeeHistorian.EndPlayers();
		io_flush(m_TeeHistorianFile);
		m_TeeHistorian.BeginInputs();
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			// send vote options
			ProgressVoteOptions(i);

			m_apPlayers[i]->Tick();
			m_apPlayers[i]->PostTick();

			// F-DDrace
			m_pShop->Tick(i);
		}
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i])
			m_apPlayers[i]->PostPostTick();
	}

	// update voting
	if(m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if(m_VoteCloseTime == -1)
			EndVote(VOTE_END_ABORT, false);
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
				for(int i = 0; i < MAX_CLIENTS; i++)
					if(m_apPlayers[i])
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
				bool aVoteChecked[MAX_CLIENTS] = {0};
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!m_apPlayers[i] || m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || aVoteChecked[i])	// don't count in votes by spectators
						continue;

					int ActVote = m_apPlayers[i]->m_Vote;
					int ActVotePos = m_apPlayers[i]->m_VotePos;

					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i+1; j < MAX_CLIENTS; ++j)
					{
						if(!m_apPlayers[j] || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]))
							continue;

						aVoteChecked[j] = true;
						if(m_apPlayers[j]->m_Vote && (!ActVote || ActVotePos > m_apPlayers[j]->m_VotePos))
						{
							ActVote = m_apPlayers[j]->m_Vote;
							ActVotePos = m_apPlayers[j]->m_VotePos;
						}
					}

					Total++;
					if(ActVote > 0)
						Yes++;
					else if(ActVote < 0)
						No++;
				}
			}

			if(m_VoteEnforce == VOTE_ENFORCE_YES || (m_VoteUpdate && Yes >= Total/2+1))
			{
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				if(m_VoteCreator != -1 && m_apPlayers[m_VoteCreator])
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;

				EndVote(VOTE_END_PASS, m_VoteEnforce==VOTE_ENFORCE_YES);
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || (m_VoteUpdate && No >= (Total+1)/2) || time_get() > m_VoteCloseTime)
				EndVote(VOTE_END_FAIL, m_VoteEnforce==VOTE_ENFORCE_NO);
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}

	for (int i = 0; i < m_NumMutes; i++)
	{
		if (m_aMutes[i].m_Expire <= Server()->Tick())
		{
			m_NumMutes--;
			m_aMutes[i] = m_aMutes[m_NumMutes];
		}
	}
	for (int i = 0; i < m_NumVoteMutes; i++)
	{
		if (m_aVoteMutes[i].m_Expire <= Server()->Tick())
		{
			m_NumVoteMutes--;
			m_aVoteMutes[i] = m_aVoteMutes[m_NumVoteMutes];
		}
	}

	if (Server()->Tick() % (Config()->m_SvAnnouncementInterval * Server()->TickSpeed() * 60) == 0)
	{
		const char* Line = Server()->GetAnnouncementLine(Config()->m_SvAnnouncementFileName);
		if (Line)
			SendChat(-1, CHAT_ALL, -1, Line);
	}

	if (Collision()->m_NumSwitchers > 0)
		for (int i = 0; i < Collision()->m_NumSwitchers + 1; ++i)
		{
			for (int j = 0; j < MAX_CLIENTS; ++j)
			{
				// F-DDrace
				// set current switcher client id to -1 if the player doesnt exist OR it is a non-timed switch and the player is not on the switch anymore
				if (!m_apPlayers[Collision()->m_pSwitchers[i].m_ClientID[j]] || (Collision()->m_pSwitchers[i].m_StartTick[j] < Server()->Tick() && Collision()->m_pSwitchers[i].m_EndTick[j] == 0))
					Collision()->m_pSwitchers[i].m_ClientID[j] = -1;

				// if it is a timed switch, the client id will be reset after the time is over here
				if (Collision()->m_pSwitchers[i].m_EndTick[j] <= Server()->Tick() && Collision()->m_pSwitchers[i].m_Type[j] == TILE_SWITCHTIMEDOPEN)
				{
					Collision()->m_pSwitchers[i].m_Status[j] = false;
					Collision()->m_pSwitchers[i].m_EndTick[j] = 0;
					Collision()->m_pSwitchers[i].m_Type[j] = TILE_SWITCHCLOSE;
					Collision()->m_pSwitchers[i].m_ClientID[j] = -1;
				}
				else if (Collision()->m_pSwitchers[i].m_EndTick[j] <= Server()->Tick() && Collision()->m_pSwitchers[i].m_Type[j] == TILE_SWITCHTIMEDCLOSE)
				{
					Collision()->m_pSwitchers[i].m_Status[j] = true;
					Collision()->m_pSwitchers[i].m_EndTick[j] = 0;
					Collision()->m_pSwitchers[i].m_Type[j] = TILE_SWITCHOPEN;
					Collision()->m_pSwitchers[i].m_ClientID[j] = -1;
				}
			}
		}

	if (m_pRandomMapResult && m_pRandomMapResult->m_Done)
	{
		str_copy(Config()->m_SvMap, m_pRandomMapResult->m_aMap, sizeof(Config()->m_SvMap));
		m_pRandomMapResult = nullptr;
	}

	if (m_pMapVoteResult && m_pMapVoteResult->m_Done)
	{
		m_VoteKick = false;
		m_VoteSpec = false;
		m_LastMapVote = time_get();

		char aCmd[256];
		str_format(aCmd, sizeof(aCmd), "sv_reset_file types/%s/flexreset.cfg; change_map \"%s\"", m_pMapVoteResult->m_aServer, m_pMapVoteResult->m_aMap);

		char aChatmsg[512];
		str_format(aChatmsg, sizeof(aChatmsg), "'%s' called vote to change server option '%s' (%s)", Server()->ClientName(m_pMapVoteResult->m_ClientID), m_pMapVoteResult->m_aMap, "/map");

		CallVote(m_pMapVoteResult->m_ClientID, m_pMapVoteResult->m_aMap, aCmd, "/map", aChatmsg);

		m_pMapVoteResult = nullptr;
	}

	// F-DDrace
	if (Server()->Tick() > m_LastAccSaveTick + Server()->TickSpeed() * Config()->m_SvAccSaveInterval * 60)
	{
		// save all accounts
		dbg_msg("acc", "automatic account saving...");
		for (unsigned int i = ACC_START; i < m_Accounts.size(); i++)
			WriteAccountStats(i);
		m_LastAccSaveTick = Server()->Tick();
	}

	// minigames
	if (!m_aMinigameDisabled[MINIGAME_SURVIVAL])
		SurvivalTick();

	for (int i = 0; i < 2; i++)
		if (!m_aMinigameDisabled[i == 0 ? MINIGAME_INSTAGIB_BOOMFNG : MINIGAME_INSTAGIB_FNG])
			InstagibTick(i);

#ifdef CONF_DEBUG
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->IsDummy())
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i&1)?-1:1;
			m_apPlayers[i]->OnPredictedInput(&Input);
		}
	}
#endif
}

// Server hooks
void CGameContext::OnClientDirectInput(int ClientID, void *pInput)
{
	int NumFailures = m_NetObjHandler.NumObjFailures();
	if(m_NetObjHandler.ValidateObj(NETOBJTYPE_PLAYERINPUT, pInput, sizeof(CNetObj_PlayerInput)) == -1)
	{
		if(Config()->m_Debug && NumFailures != m_NetObjHandler.NumObjFailures())
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "NETOBJTYPE_PLAYERINPUT failed on '%s'", m_NetObjHandler.FailedObjOn());
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}
	}
	else
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)pInput);

	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerInput(ClientID, (CNetObj_PlayerInput *)pInput);
	}
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
	{
		int NumFailures = m_NetObjHandler.NumObjFailures();
		if(m_NetObjHandler.ValidateObj(NETOBJTYPE_PLAYERINPUT, pInput, sizeof(CNetObj_PlayerInput)) == -1)
		{
			if(Config()->m_Debug && NumFailures != m_NetObjHandler.NumObjFailures())
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "NETOBJTYPE_PLAYERINPUT corrected on '%s'", m_NetObjHandler.FailedObjOn());
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
			}
		}
		else
			m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput *)pInput);
	}
}

struct CVoteOptionServer *CGameContext::GetVoteOption(int Index)
{
	CVoteOptionServer *pCurrent;
	for (pCurrent = m_pVoteOptionFirst;
			Index > 0 && pCurrent;
			Index--, pCurrent = pCurrent->m_pNext);

	if (Index > 0)
		return 0;
	return pCurrent;
}

void CGameContext::ProgressVoteOptions(int ClientID)
{
	CPlayer *pPl = m_apPlayers[ClientID];

	if (pPl->m_SendVoteIndex == -1)
		return;

	//TODO: removing votes can cause this
	dbg_assert(pPl->m_SendVoteIndex <= m_NumVoteOptions, "m_SendVoteIndex invalid");

	int VotesLeft = m_NumVoteOptions - pPl->m_SendVoteIndex;
	int NumVotesToSend = min(Config()->m_SvVotesPerTick, VotesLeft);

	if (!VotesLeft)
	{
		// player has up to date vote option list
		return;
	}

	// build vote option list msg
	int CurIndex = 0;

	// get current vote option by index
	CVoteOptionServer *pCurrent = GetVoteOption(pPl->m_SendVoteIndex);

	// pack and send vote list packet
	CMsgPacker Msg(NETMSGTYPE_SV_VOTEOPTIONLISTADD);
	Msg.AddInt(NumVotesToSend);
	while(pCurrent && CurIndex < NumVotesToSend)
	{
		Msg.AddString(pCurrent->m_aDescription, VOTE_DESC_LENGTH);
		pCurrent = pCurrent->m_pNext;
		CurIndex++;
	}
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);

	pPl->m_SendVoteIndex += NumVotesToSend;
}

void CGameContext::OnClientEnter(int ClientID)
{
	// F-DDrace
	str_copy(m_apPlayers[ClientID]->m_CurrentInfo.m_aName, Server()->ClientName(ClientID), MAX_NAME_LENGTH);
	str_copy(m_apPlayers[ClientID]->m_CurrentInfo.m_aClan, Server()->ClientClan(ClientID), MAX_CLAN_LENGTH);
	m_apPlayers[ClientID]->m_CurrentInfo.m_TeeInfos = m_apPlayers[ClientID]->m_TeeInfos;

	m_apPlayers[ClientID]->Respawn();

	// load score
	{
		Score()->PlayerData(ClientID)->Reset();
		Score()->LoadScore(ClientID);
		Score()->PlayerData(ClientID)->m_CurrentTime = Score()->PlayerData(ClientID)->m_BestTime;
		m_apPlayers[ClientID]->m_Score = !Score()->PlayerData(ClientID)->m_BestTime ? -1 : Score()->PlayerData(ClientID)->m_BestTime;
	}

	SendChatTarget(ClientID, "F-DDrace Mod. Version: " GAME_VERSION ", by fokkonaut");
	if (Config()->m_SvWelcome[0] != 0)
		SendChatTarget(ClientID, Config()->m_SvWelcome);

	m_VoteUpdate = true;

	// update client infos (others before local)
	CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = ClientID;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = m_apPlayers[ClientID]->GetTeam();
	NewClientInfoMsg.m_pName = Server()->ClientName(ClientID);
	NewClientInfoMsg.m_pClan = Server()->ClientClan(ClientID);
	NewClientInfoMsg.m_Country = Server()->ClientCountry(ClientID);
	NewClientInfoMsg.m_Silent = false;

	if(Config()->m_SvSilentSpectatorMode && m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS)
		NewClientInfoMsg.m_Silent = true;

	for(int p = 0; p < NUM_SKINPARTS; p++)
	{
		NewClientInfoMsg.m_apSkinPartNames[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aaSkinPartNames[p];
		NewClientInfoMsg.m_aUseCustomColors[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aUseCustomColors[p];
		NewClientInfoMsg.m_aSkinPartColors[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aSkinPartColors[p];
	}


	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(i == ClientID || !m_apPlayers[i] || (!Server()->ClientIngame(i) && !m_apPlayers[i]->IsDummy()))
			continue;

		// new info for others
		if(Server()->ClientIngame(i))
			Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);

		// existing infos for new player
		CNetMsg_Sv_ClientInfo ClientInfoMsg;
		ClientInfoMsg.m_ClientID = i;
		ClientInfoMsg.m_Local = 0;
		ClientInfoMsg.m_Team = m_apPlayers[i]->GetTeam();
		ClientInfoMsg.m_pName = m_apPlayers[i]->m_CurrentInfo.m_aName;
		ClientInfoMsg.m_pClan = m_apPlayers[i]->m_CurrentInfo.m_aClan;
		ClientInfoMsg.m_Country = Server()->ClientCountry(i);
		ClientInfoMsg.m_Silent = false;
		for(int p = 0; p < NUM_SKINPARTS; p++)
		{
			ClientInfoMsg.m_apSkinPartNames[p] = m_apPlayers[i]->m_CurrentInfo.m_TeeInfos.m_aaSkinPartNames[p];
			ClientInfoMsg.m_aUseCustomColors[p] = m_apPlayers[i]->m_CurrentInfo.m_TeeInfos.m_aUseCustomColors[p];
			ClientInfoMsg.m_aSkinPartColors[p] = m_apPlayers[i]->m_CurrentInfo.m_TeeInfos.m_aSkinPartColors[p];
		}
		Server()->SendPackMsg(&ClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);
	}

	// local info
	NewClientInfoMsg.m_Local = 1;
	Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);

	if(Server()->DemoRecorder_IsRecording())
	{
		CNetMsg_De_ClientEnter Msg;
		Msg.m_pName = NewClientInfoMsg.m_pName;
		Msg.m_ClientID = ClientID;
		Msg.m_Team = NewClientInfoMsg.m_Team;
		Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
	}

	// F-DDrace
	UpdateHidePlayers();

	if (m_apPlayers[ClientID]->m_IsDummy) // dummies dont need these information
		return;

	m_apPlayers[ClientID]->CheckClanProtection();

	m_pController->UpdateGameInfo(ClientID);

	// send chat commands
	SendChatCommands(ClientID);
}

void CGameContext::OnClientConnected(int ClientID, bool Dummy, bool AsSpec)
{
	{
		bool Empty = true;
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i])
			{
				Empty = false;
				break;
			}
		}
		if (Empty)
		{
			m_NonEmptySince = Server()->Tick();
		}
	}

	if(m_apPlayers[ClientID])
	{
		dbg_assert(m_apPlayers[ClientID]->IsDummy(), "invalid clientID");
		OnClientDrop(ClientID, "removing dummy");
	}

	m_apPlayers[ClientID] = new(ClientID) CPlayer(this, ClientID, Dummy, AsSpec);

	if(Dummy)
		return;

	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(m_VoteType, ClientID);

	// send motd
	SendMotd(FormatMotd(Config()->m_SvMotd), ClientID);

	// send settings
	SendSettings(ClientID);
}

void CGameContext::OnClientTeamChange(int ClientID)
{
	if(m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS)
		AbortVoteOnTeamChange(ClientID);
}

void CGameContext::OnClientDrop(int ClientID, const char *pReason)
{
	m_apPlayers[ClientID]->OnDisconnect();

	AbortVoteOnDisconnect(ClientID);

	// update clients on drop
	if(Server()->ClientIngame(ClientID))
	{
		if(Server()->DemoRecorder_IsRecording())
		{
			CNetMsg_De_ClientLeave Msg;
			Msg.m_pName = Server()->ClientName(ClientID);
			Msg.m_pReason = pReason;
			Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
		}

		CNetMsg_Sv_ClientDrop Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_pReason = pReason;
		Msg.m_Silent = false;
		if(Config()->m_SvSilentSpectatorMode && m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS)
			Msg.m_Silent = true;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, -1);
	}

	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	m_VoteUpdate = true;
}

void CGameContext::OnClientEngineJoin(int ClientID)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerJoin(ClientID);
	}
}

void CGameContext::OnClientEngineDrop(int ClientID, const char *pReason)
{
	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.RecordPlayerDrop(ClientID, pReason);
	}
}

void CGameContext::OnClientAuth(int ClientID, int Level)
{
	if(m_TeeHistorianActive)
	{
		if(Level)
		{
			m_TeeHistorian.RecordAuthLogin(ClientID, Level, Server()->AuthName(ClientID));
		}
		else
		{
			m_TeeHistorian.RecordAuthLogout(ClientID);
		}
	}
}

void *CGameContext::UnpackMsg(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	if (Server()->IsSevendown(ClientID))
	{
		if (MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			static CNetMsg_Cl_StartInfo Msg;

			const char *apSkinPartNames[6];
			int aUseCustomColors[6];
			int aSkinPartColors[6];

			Msg.m_pName = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			Msg.m_pClan = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			Msg.m_Country = pUnpacker->GetInt();

			for (int p = 0; p < NUM_SKINPARTS; p++)
			{
				Msg.m_apSkinPartNames[p] = "standard";
				Msg.m_aUseCustomColors[p] = 0;
				Msg.m_aSkinPartColors[p] = 0;
			}

			Msg.m_apSkinPartNames[SKINPART_BODY] = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			Msg.m_aUseCustomColors[SKINPART_BODY] = Msg.m_aUseCustomColors[SKINPART_FEET] = pUnpacker->GetInt();
			Msg.m_aSkinPartColors[SKINPART_BODY] = pUnpacker->GetInt();
			Msg.m_aSkinPartColors[SKINPART_FEET] = pUnpacker->GetInt();

			return &Msg;
		}
		else if (MsgID == NETMSGTYPE_CL_SAY)
		{
			static CNetMsg_Cl_Say Msg;
			Msg.m_Mode = pUnpacker->GetInt() ? CHAT_TEAM : CHAT_ALL;
			Msg.m_pMessage = pUnpacker->GetString(CUnpacker::SANITIZE_CC);
			return &Msg;
		}
		else if (MsgID == NETMSGTYPE_CL_SETSPECTATORMODE)
		{
			static CNetMsg_Cl_SetSpectatorMode Msg;
			Msg.m_SpectatorID = pUnpacker->GetInt();
			Msg.m_SpecMode = Msg.m_SpectatorID == -1 ? SPEC_FREEVIEW : SPEC_PLAYER;
			return &Msg;
		}
	}

	return m_NetObjHandler.SecureUnpackMsg(MsgID, pUnpacker);
}

void CGameContext::OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	void *pRawMsg = UnpackMsg(MsgID, pUnpacker, ClientID);
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(m_TeeHistorianActive)
	{
		if(m_NetObjHandler.TeeHistorianRecordMsg(MsgID))
		{
			m_TeeHistorian.RecordPlayerMessage(ClientID, pUnpacker->CompleteData(), pUnpacker->CompleteSize());
		}
	}

	if (!pRawMsg)
	{
		if (Config()->m_Debug)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler.GetMsgName(MsgID), MsgID, m_NetObjHandler.FailedMsgOn());
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}
		return;
	}

	if(Server()->ClientIngame(ClientID))
	{
		if(MsgID == NETMSGTYPE_CL_SAY)
		{
			CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;

			// trim right and set maximum length to 128 utf8-characters
			int Length = 0;
			const char *p = pMsg->m_pMessage;
			const char *pEnd = 0;
			while(*p)
			{
				const char *pStrOld = p;
				int Code = str_utf8_decode(&p);

				// check if unicode is not empty
				if(!str_utf8_is_whitespace(Code))
				{
					pEnd = 0;
				}
				else if(pEnd == 0)
					pEnd = pStrOld;

				if(++Length >= 127)
				{
					*(const_cast<char *>(p)) = 0;
					break;
				}
			}
			if(pEnd != 0)
				*(const_cast<char *>(pEnd)) = 0;

			// drop empty and autocreated spam messages (more than 32 characters per second)
			if (Length == 0 || (pMsg->m_pMessage[0] != '/' && (Config()->m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat + Server()->TickSpeed() * ((31 + Length) / 32) > Server()->Tick())))
				return;

			// don't allow spectators to disturb players during a running game in tournament mode
			int Mode = pMsg->m_Mode;
			if((Config()->m_SvTournamentMode == 2) &&
				pPlayer->GetTeam() == TEAM_SPECTATORS &&
				!Server()->GetAuthedState(ClientID))
			{
				if(Mode != CHAT_WHISPER)
					Mode = CHAT_TEAM;
				else if(m_apPlayers[pMsg->m_Target] && m_apPlayers[pMsg->m_Target]->GetTeam() != TEAM_SPECTATORS)
					Mode = CHAT_NONE;
			}

			if (pMsg->m_pMessage[0] == '/')
			{
				CPlayer* pPlayer = m_apPlayers[ClientID];

				if (Config()->m_SvSpamprotection && !str_startswith(pMsg->m_pMessage + 1, "timeout ")
					&& pPlayer->m_LastCommands[0] && pPlayer->m_LastCommands[0] + Server()->TickSpeed() > Server()->Tick()
					&& pPlayer->m_LastCommands[1] && pPlayer->m_LastCommands[1] + Server()->TickSpeed() > Server()->Tick()
					&& pPlayer->m_LastCommands[2] && pPlayer->m_LastCommands[2] + Server()->TickSpeed() > Server()->Tick()
					&& pPlayer->m_LastCommands[3] && pPlayer->m_LastCommands[3] + Server()->TickSpeed() > Server()->Tick()
					)
					return;

				int64 Now = Server()->Tick();
				pPlayer->m_LastCommands[pPlayer->m_LastCommandPos] = Now;
				pPlayer->m_LastCommandPos = (pPlayer->m_LastCommandPos + 1) % 4;

				m_ChatResponseTargetID = ClientID;
				Server()->RestrictRconOutput(ClientID);
				Console()->SetFlagMask(CFGFLAG_CHAT);

				int Authed = Server()->GetAuthedState(ClientID);
				if (Authed)
					Console()->SetAccessLevel(Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD : IConsole::ACCESS_LEVEL_HELPER);
				else
					Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_USER);
				Console()->SetPrintOutputLevel(m_ChatPrintCBIndex, 0);

				Console()->ExecuteLine(pMsg->m_pMessage + 1, ClientID, false);
				// m_apPlayers[ClientID] can be NULL, if the player used a
				// timeout code and replaced another client.
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%d used %s", ClientID, pMsg->m_pMessage);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "chat-command", aBuf);

				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				Console()->SetFlagMask(CFGFLAG_SERVER);
				m_ChatResponseTargetID = -1;
				Server()->RestrictRconOutput(-1);
			}
			else if (!pPlayer->m_ShowName)
			{
				pPlayer->m_ChatFix.m_Mode = Mode;
				pPlayer->m_ChatFix.m_Target = pMsg->m_Target;
				str_copy(pPlayer->m_ChatFix.m_Message, pMsg->m_pMessage, sizeof(pPlayer->m_ChatFix.m_Message));
				pPlayer->FixForNoName(FIX_CHAT_MSG);
			}
			else if(Mode != CHAT_NONE)
				SendChat(ClientID, Mode, pMsg->m_Target, pMsg->m_pMessage, ClientID);
		}
		else if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			int64 Now = Server()->Tick();
			int64 TickSpeed = Server()->TickSpeed();

			if (Config()->m_SvRconVote && !Server()->GetAuthedState(ClientID))
			{
				SendChatTarget(ClientID, "You can only vote after logging in.");
				return;
			}

			if (Config()->m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry + TickSpeed * 3 > Now)
				return;

			pPlayer->m_LastVoteTry = Now;
			if (Config()->m_SvSpectatorVotes == 0 && pPlayer->GetTeam() == TEAM_SPECTATORS)
			{
				SendChatTarget(ClientID, "Spectators aren't allowed to start a vote.");
				return;
			}

			if (m_VoteCloseTime)
			{
				SendChatTarget(ClientID, "Wait for current vote to end before calling a new one.");
				return;
			}

			if (Now < pPlayer->m_FirstVoteTick)
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "You must wait %d seconds before making your first vote.", (int)((pPlayer->m_FirstVoteTick - Now) / TickSpeed) + 1);
				SendChatTarget(ClientID, aBuf);
				return;
			}

			int TimeLeft = pPlayer->m_LastVoteCall + TickSpeed * Config()->m_SvVoteDelay - Now;
			if (pPlayer->m_LastVoteCall && TimeLeft > 0)
			{
				char aChatmsg[64];
				str_format(aChatmsg, sizeof(aChatmsg), "You must wait %d seconds before making another vote.", (int)(TimeLeft / TickSpeed) + 1);
				SendChatTarget(ClientID, aChatmsg);
				return;
			}

			NETADDR Addr;
			Server()->GetClientAddr(ClientID, &Addr);
			int VoteMuted = 0;
			for (int i = 0; i < m_NumVoteMutes && !VoteMuted; i++)
				if (!net_addr_comp_noport(&Addr, &m_aVoteMutes[i].m_Addr))
					VoteMuted = (m_aVoteMutes[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
			if (VoteMuted > 0)
			{
				char aChatmsg[64];
				str_format(aChatmsg, sizeof(aChatmsg), "You are not permitted to vote for the next %d seconds.", VoteMuted);
				SendChatTarget(ClientID, aChatmsg);
				return;
			}

			char aChatmsg[512] = {0};
			m_VoteType = VOTE_UNKNOWN;
			char aDesc[VOTE_DESC_LENGTH] = {0};
			char aCmd[VOTE_CMD_LENGTH] = {0};
			char aReason[VOTE_REASON_LENGTH] = "No reason given";
			CNetMsg_Cl_CallVote* pMsg = (CNetMsg_Cl_CallVote*)pRawMsg;
			if (!str_utf8_check(pMsg->m_Type)
				|| !str_utf8_check(pMsg->m_Reason)
				|| !str_utf8_check(pMsg->m_Value))
			{
				return;
			}
			if (pMsg->m_Reason[0])
			{
				str_copy(aReason, pMsg->m_Reason, sizeof(aReason));
			}

			if(str_comp_nocase(pMsg->m_Type, "option") == 0)
			{
				int Authed = Server()->GetAuthedState(ClientID);
				CVoteOptionServer *pOption = m_pVoteOptionFirst;
				while(pOption)
				{
					if(str_comp_nocase(pMsg->m_Value, pOption->m_aDescription) == 0)
					{
						if(!Authed && (str_startswith(pOption->m_aCommand, "sv_map ") || str_startswith(pOption->m_aCommand, "change_map ") || str_startswith(pOption->m_aCommand, "random_map") || str_startswith(pOption->m_aCommand, "random_unfinished_map")) && time_get() < m_LastMapVote + (time_freq() * Config()->m_SvVoteMapTimeDelay))
						{
							str_format(aChatmsg, sizeof(aChatmsg), "There's a %d second delay between map-votes, please wait %d seconds.", Config()->m_SvVoteMapTimeDelay, (int)(((m_LastMapVote+(Config()->m_SvVoteMapTimeDelay * time_freq()))/time_freq())-(time_get()/time_freq())));
							SendChatTarget(ClientID, aChatmsg);
							return;
						}
						str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);
						str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
						char aBuf[128];
						str_format(aBuf, sizeof(aBuf),
							"'%d:%s' voted %s '%s' reason='%s' cmd='%s' force=%d",
							ClientID, Server()->ClientName(ClientID), pMsg->m_Type,
							aDesc, aReason, aCmd, pMsg->m_Force
						);
						Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
						if(pMsg->m_Force)
						{
							Server()->SetRconCID(ClientID);
							Console()->ExecuteLine(aCmd);
							Server()->SetRconCID(IServer::RCON_CID_SERV);
							ForceVote(VOTE_START_OP, aDesc, aReason);
							return;
						}
						m_VoteType = VOTE_START_OP;
						m_LastMapVote = time_get();
						break;
					}

					pOption = pOption->m_pNext;
				}

				if(!pOption)
					return;
			}
			else if(str_comp_nocase(pMsg->m_Type, "kick") == 0)
			{
				if(!Config()->m_SvVoteKick)
					return;

				int KickID = str_toint(pMsg->m_Value);
				if(KickID < 0 || KickID >= MAX_CLIENTS || !m_apPlayers[KickID] || KickID == ClientID || Server()->GetAuthedState(KickID))
					return;

				str_format(aDesc, sizeof(aDesc), "%2d: %s", KickID, Server()->ClientName(KickID));
				if (!Config()->m_SvVoteKickBantime)
					str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickID);
				else
				{
					char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
					Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
					str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, Config()->m_SvVoteKickBantime);
				}
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf),
					"'%d:%s' voted %s '%d:%s' reason='%s' cmd='%s' force=%d",
					ClientID, Server()->ClientName(ClientID), pMsg->m_Type,
					KickID, Server()->ClientName(KickID), aReason, aCmd, pMsg->m_Force
				);
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				if(pMsg->m_Force)
				{
					Server()->SetRconCID(ClientID);
					Console()->ExecuteLine(aCmd);
					Server()->SetRconCID(IServer::RCON_CID_SERV);
					return;
				}
				if (Config()->m_SvVoteKickMin && !GetDDRaceTeam(ClientID))
				{
					char aaAddresses[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = { {0} };
					for (int i = 0; i < MAX_CLIENTS; i++)
					{
						if (m_apPlayers[i])
						{
							Server()->GetClientAddr(i, aaAddresses[i], NETADDR_MAXSTRSIZE);
						}
					}
					int NumPlayers = 0;
					for (int i = 0; i < MAX_CLIENTS; ++i)
					{
						if (m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && !GetDDRaceTeam(i))
						{
							NumPlayers++;
							for (int j = 0; j < i; j++)
							{

								if (m_apPlayers[j] && m_apPlayers[j]->GetTeam() != TEAM_SPECTATORS && !GetDDRaceTeam(j))
								{
									if (str_comp(aaAddresses[i], aaAddresses[j]) == 0)
									{
										NumPlayers--;
										break;
									}
								}
							}
						}
					}

					if (NumPlayers < Config()->m_SvVoteKickMin)
					{
						str_format(aChatmsg, sizeof(aChatmsg), "Kick voting requires %d players", Config()->m_SvVoteKickMin);
						SendChatTarget(ClientID, aChatmsg);
						return;
					}
				}

				if (KickID < 0 || KickID >= MAX_CLIENTS || !m_apPlayers[KickID])
				{
					SendChatTarget(ClientID, "Invalid client id to kick");
					return;
				}
				if (KickID == ClientID)
				{
					SendChatTarget(ClientID, "You can't kick yourself");
					return;
				}
				int KickedAuthed = Server()->GetAuthedState(KickID);
				if (KickedAuthed > Server()->GetAuthedState(ClientID))
				{
					SendChatTarget(ClientID, "You can't kick authorized players");
					char aBufKick[128];
					str_format(aBufKick, sizeof(aBufKick), "'%s' called for vote to kick you", Server()->ClientName(ClientID));
					SendChatTarget(KickID, aBufKick);
					return;
				}

				// Don't allow kicking if a player has no character
				if (!GetPlayerChar(ClientID) || !GetPlayerChar(KickID) || GetDDRaceTeam(ClientID) != GetDDRaceTeam(KickID))
				{
					SendChatTarget(ClientID, "You can kick only your team member");
					return;
				}
				if (m_apPlayers[KickID]->m_IsDummy)
				{
					SendChatTarget(ClientID, "You can't kick dummies");
					return;
				}

				m_VoteType = VOTE_START_KICK;
				m_VoteClientID = KickID;
			}
			else if(str_comp_nocase(pMsg->m_Type, "spectate") == 0)
			{
				if(!Config()->m_SvVoteSpectate)
					return;

				int SpectateID = str_toint(pMsg->m_Value);
				if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] || m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS || SpectateID == ClientID)
					return;

				if (m_apPlayers[SpectateID]->m_IsDummy)
				{
					SendChatTarget(ClientID, "You can't set dummies to spectator");
					return;
				}

				str_format(aDesc, sizeof(aDesc), "%2d: %s", SpectateID, Server()->ClientName(SpectateID));
				str_format(aCmd, sizeof(aCmd), "set_team %d -1 %d", SpectateID, Config()->m_SvVoteSpectateRejoindelay);
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf),
					"'%d:%s' voted %s '%d:%s' reason='%s' cmd='%s' force=%d",
					ClientID, Server()->ClientName(ClientID), pMsg->m_Type,
					SpectateID, Server()->ClientName(SpectateID), aReason, aCmd, pMsg->m_Force
				);
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				if(pMsg->m_Force)
				{
					Server()->SetRconCID(ClientID);
					Console()->ExecuteLine(aCmd);
					Server()->SetRconCID(IServer::RCON_CID_SERV);
					ForceVote(VOTE_START_SPEC, aDesc, aReason);
					return;
				}
				m_VoteType = VOTE_START_SPEC;
				m_VoteClientID = SpectateID;
			}

			if(m_VoteType != VOTE_UNKNOWN)
			{
				m_VoteCreator = ClientID;
				StartVote(aDesc, aCmd, aReason);
				pPlayer->m_Vote = 1;
				pPlayer->m_VotePos = m_VotePos = 1;
				pPlayer->m_LastVoteCall = Now;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
			CCharacter *pChr = pPlayer->GetCharacter();
			CCharacter *pControlledTee = pPlayer->m_pControlledTee ? pPlayer->m_pControlledTee->GetCharacter() : 0;

			if (pMsg->m_Vote == 1) //vote yes (f3)
			{
				if (pPlayer->m_TeeControlMode)
				{
					if (pControlledTee)
						pControlledTee->DropFlag();
				}
				else if (pChr)
				{
					if (m_pShop->IsInShop(ClientID))
						m_pShop->OnKeyPress(ClientID, pMsg->m_Vote);
					else
						pChr->DropFlag();
				}
			}
			else if (pMsg->m_Vote == -1) //vote no (f4)
			{
				if (pPlayer->m_TeeControlMode)
				{
					if (pControlledTee)
						pControlledTee->DropWeapon(pControlledTee->GetActiveWeapon(), false);
				}
				else if (pChr)
				{
					if (m_pShop->IsInShop(ClientID))
						m_pShop->OnKeyPress(ClientID, pMsg->m_Vote);
					else
						pChr->DropWeapon(pChr->GetActiveWeapon(), false);
				}
			}

			if(!m_VoteCloseTime)
				return;

			if(pPlayer->m_Vote == 0)
			{
				if(!pMsg->m_Vote)
					return;

				pPlayer->m_Vote = pMsg->m_Vote;
				pPlayer->m_VotePos = ++m_VotePos;
				m_VoteUpdate = true;
			}
			else if(m_VoteCreator == pPlayer->GetCID())
			{
				CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
				if(pMsg->m_Vote != -1 || m_VoteCancelTime<time_get())
					return;

				m_VoteCloseTime = -1;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_SETTEAM)
		{
			CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

			if (pPlayer->m_HasTeeControl && !pPlayer->IsPaused())
			{
				bool SetTeeControl = pMsg->m_Team == TEAM_SPECTATORS;
				if (!SetTeeControl)
					pPlayer->UnsetTeeControl();
				pPlayer->m_TeeControlMode = SetTeeControl;
				SendTeamChange(ClientID, SetTeeControl ? TEAM_SPECTATORS : pPlayer->GetTeam(), true, Server()->Tick(), ClientID);
				SendChatTarget(ClientID, pPlayer->m_TeeControlMode ? "You are now using the tee controller" : "You are no longer using the tee controller");
				if (pPlayer->GetCharacter())
					SendTuningParams(ClientID, pPlayer->GetCharacter()->m_TuneZone);
				if (pPlayer->m_TeeControlForcedID != -1 && SetTeeControl)
					pPlayer->SetTeeControl(m_apPlayers[pPlayer->m_TeeControlForcedID]);
				return;
			}

			if (pPlayer->GetTeam() == pMsg->m_Team
				|| (Config()->m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam + Server()->TickSpeed() * Config()->m_SvTeamChangeDelay > Server()->Tick())
				|| pPlayer->m_TeamChangeTick > Server()->Tick())
				return;

			CCharacter* pChr = pPlayer->GetCharacter();
			if (pChr)
			{
				int CurrTime = (Server()->Tick() - pChr->m_StartTime) / Server()->TickSpeed();
				if (Config()->m_SvKillProtection != 0 && CurrTime >= (60 * Config()->m_SvKillProtection) && pChr->m_DDRaceState == DDRACE_STARTED)
				{
					SendChatTarget(ClientID, "Kill Protection enabled. If you really want to join the spectators, first type /kill");
					return;
				}
			}

			pPlayer->m_LastSetTeam = Server()->Tick();

			// Switch team on given client and kill/respawn him
			if(m_pController->CanJoinTeam(pMsg->m_Team, ClientID))
			{
				if (pPlayer->IsPaused())
					SendChatTarget(ClientID, "Use /pause first then you can kill");
				else
				{
					if (pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
						m_VoteUpdate = true;
					pPlayer->m_TeamChangeTick = Server()->Tick() + Server()->TickSpeed() * Config()->m_SvTeamChangeDelay;
					pPlayer->SetTeam(pMsg->m_Team);
				}
			}
			else
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "Only %d active players are allowed", Config()->m_SvMaxClients - Config()->m_SvSpectatorSlots);
				SendBroadcast(aBuf, ClientID);
			}
		}
		else if (MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			if(Config()->m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode+Server()->TickSpeed()/4 > Server()->Tick())
				return;

			pPlayer->m_LastSetSpectatorMode = Server()->Tick();

			if (pPlayer->m_TeeControlMode && pPlayer->GetTeam() != TEAM_SPECTATORS && !pPlayer->IsPaused())
			{
				if (pPlayer->m_TeeControlForcedID == -1)
				{
					switch (pMsg->m_SpecMode)
					{
					case SPEC_FREEVIEW:
						pPlayer->UnsetTeeControl();
						break;
					case SPEC_PLAYER:
						pPlayer->SetTeeControl(m_apPlayers[pMsg->m_SpectatorID]);
						break;
					case SPEC_FLAGRED:
					case SPEC_FLAGBLUE:
						for (int i = 0; i < 2; i++)
						{
							CFlag* F = ((CGameControllerDDRace*)m_pController)->m_apFlags[i];
							if (!F || !F->GetCarrier())
								continue;

							if ((pMsg->m_SpecMode == SPEC_FLAGRED && i == TEAM_RED) || (pMsg->m_SpecMode == SPEC_FLAGBLUE && i == TEAM_BLUE))
								pPlayer->SetTeeControl(F->GetCarrier()->GetPlayer());
						} break;
					}
				}
			}
			else
				pPlayer->SetSpectatorID(pMsg->m_SpecMode, pMsg->m_SpectatorID);
		}
		else if (MsgID == NETMSGTYPE_CL_EMOTICON && !m_World.m_Paused)
		{
			CNetMsg_Cl_Emoticon *pMsg = (CNetMsg_Cl_Emoticon *)pRawMsg;

			if(Config()->m_SvSpamprotection && pPlayer->m_LastEmote && pPlayer->m_LastEmote+Server()->TickSpeed()*Config()->m_SvEmoticonDelay > Server()->Tick())
				return;

			pPlayer->m_LastEmote = Server()->Tick();

			SendEmoticon(ClientID, pMsg->m_Emoticon);
			CCharacter *pChr = pPlayer->GetCharacter();
			if(pChr && Config()->m_SvEmotionalTees && pPlayer->m_EyeEmote)
			{
				switch(pMsg->m_Emoticon)
				{
				case EMOTICON_EXCLAMATION:
				case EMOTICON_GHOST:
				case EMOTICON_QUESTION:
				case EMOTICON_WTF:
						pChr->SetEmoteType(EMOTE_SURPRISE);
						break;
				case EMOTICON_DOTDOT:
				case EMOTICON_DROP:
				case EMOTICON_ZZZ:
						pChr->SetEmoteType(EMOTE_BLINK);
						break;
				case EMOTICON_EYES:
				case EMOTICON_HEARTS:
				case EMOTICON_MUSIC:
						pChr->SetEmoteType(EMOTE_HAPPY);
						break;
				case EMOTICON_OOP:
				case EMOTICON_SORRY:
				case EMOTICON_SUSHI:
						pChr->SetEmoteType(EMOTE_PAIN);
						break;
				case EMOTICON_DEVILTEE:
				case EMOTICON_SPLATTEE:
				case EMOTICON_ZOMG:
						pChr->SetEmoteType(EMOTE_ANGRY);
						break;
					default:
						pChr->SetEmoteType(EMOTE_NORMAL);
						break;
				}
				if (pPlayer->m_SpookyGhost || pChr->m_Spooky)
					pChr->SetEmoteType(EMOTE_SURPRISE);
				else if (pChr->GetActiveWeapon() == WEAPON_HEART_GUN)
					pChr->SetEmoteType(EMOTE_HAPPY);
				pChr->SetEmoteStop(Server()->Tick() + 2 * Server()->TickSpeed());
			}
		}
		else if (MsgID == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
		{
			if (m_VoteCloseTime && m_VoteCreator == ClientID && GetDDRaceTeam(ClientID) && (m_VoteKick || m_VoteSpec))
			{
				SendChatTarget(ClientID, "You are running a vote please try again after the vote is done!");
				return;
			}
			if (pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed() * Config()->m_SvKillDelay > Server()->Tick())
				return;
			if (pPlayer->IsPaused())
				return;

			CCharacter* pChr = pPlayer->GetCharacter();
			if (!pChr)
				return;

			//Kill Protection
			int CurrTime = (Server()->Tick() - pChr->m_StartTime) / Server()->TickSpeed();
			if (Config()->m_SvKillProtection != 0 && CurrTime >= (60 * Config()->m_SvKillProtection) && pChr->m_DDRaceState == DDRACE_STARTED)
			{
				SendChatTarget(ClientID, "Kill Protection enabled. If you really want to kill, type /kill");
				return;
			}

			pPlayer->m_LastKill = Server()->Tick();

			CPlayer *pControlledTee = pPlayer->m_pControlledTee;
			if (pControlledTee && pControlledTee->m_IsDummy)
			{
				pControlledTee->KillCharacter(WEAPON_SELF, false);
				pControlledTee->Respawn();
			}
			else
			{
				pPlayer->KillCharacter(WEAPON_SELF);
				pPlayer->Respawn();
			}
		}
		else if (MsgID == NETMSGTYPE_CL_READYCHANGE)
		{
			if(pPlayer->m_LastReadyChange && pPlayer->m_LastReadyChange+Server()->TickSpeed()*1 > Server()->Tick())
				return;

			pPlayer->m_LastReadyChange = Server()->Tick();
			if (Config()->m_SvPlayerReadyMode && pPlayer->GetTeam() != TEAM_SPECTATORS)
			{
				// change players ready state
				pPlayer->m_IsReadyToPlay = !pPlayer->m_IsReadyToPlay;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_SKINCHANGE)
		{
			if(pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo+Server()->TickSpeed()*5 > Server()->Tick())
				return;

			pPlayer->m_LastChangeInfo = Server()->Tick();
			CNetMsg_Cl_SkinChange *pMsg = (CNetMsg_Cl_SkinChange *)pRawMsg;

			for(int p = 0; p < NUM_SKINPARTS; p++)
			{
				str_copy(pPlayer->m_TeeInfos.m_aaSkinPartNames[p], pMsg->m_apSkinPartNames[p], 24);
				pPlayer->m_TeeInfos.m_aUseCustomColors[p] = pMsg->m_aUseCustomColors[p];
				pPlayer->m_TeeInfos.m_aSkinPartColors[p] = pMsg->m_aSkinPartColors[p];
			}

			// F-DDrace
			pPlayer->CheckClanProtection();

			if (pPlayer->m_SpookyGhost || pPlayer->m_TeeInfos.m_aSkinName[0] != '\0')
				return;

			// update all clients
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				if(!m_apPlayers[i] || (!Server()->ClientIngame(i) && !m_apPlayers[i]->IsDummy()) || Server()->GetClientVersion(i) < MIN_SKINCHANGE_CLIENTVERSION)
					continue;

				SendSkinChange(pPlayer->m_TeeInfos, pPlayer->GetCID(), i);
			}
		}
		else if (MsgID == NETMSGTYPE_CL_COMMAND)
		{
			CNetMsg_Cl_Command *pMsg = (CNetMsg_Cl_Command*)pRawMsg;
			CommandManager()->OnCommand(pMsg->m_Name, pMsg->m_Arguments, ClientID);
		}
		else if (MsgID == NETMSGTYPE_CL_EXPLAYERINFO)
		{
			CNetMsg_Cl_ExPlayerInfo *pMsg = (CNetMsg_Cl_ExPlayerInfo *)pRawMsg;
			pPlayer->m_Aim = (bool)pMsg->m_Flags&EXPLAYERFLAG_AIM;
		}
		else if (MsgID == NETMSGTYPE_CL_ISDDRACE)
		{
			int Version = pUnpacker->GetInt();

			if (pUnpacker->Error())
			{
				if (pPlayer->m_DDraceVersion < VERSION_DDRACE_TEAMS)
					pPlayer->m_DDraceVersion = VERSION_DDRACE_TEAMS;
			}
			else if(pPlayer->m_DDraceVersion < Version)
				pPlayer->m_DDraceVersion = Version;

			dbg_msg("fclient", "%d using custom client. version: %x, ddrace: %d", ClientID, Server()->GetClientVersion(ClientID), pPlayer->m_DDraceVersion);

			if (pPlayer->m_DDraceVersion >= VERSION_DDRACE_TEAMS)
				((CGameControllerDDRace*)m_pController)->m_Teams.SendTeamsState(ClientID);
		}
	}
	else
	{
		if (MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			if(pPlayer->m_IsReadyToEnter)
				return;

			CNetMsg_Cl_StartInfo *pMsg = (CNetMsg_Cl_StartInfo *)pRawMsg;
			pPlayer->m_LastChangeInfo = Server()->Tick();

			// set start infos
			Server()->SetClientName(ClientID, pMsg->m_pName);
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);

			for(int p = 0; p < NUM_SKINPARTS; p++)
			{
				str_copy(pPlayer->m_TeeInfos.m_aaSkinPartNames[p], pMsg->m_apSkinPartNames[p], 24);
				pPlayer->m_TeeInfos.m_aUseCustomColors[p] = pMsg->m_aUseCustomColors[p];
				pPlayer->m_TeeInfos.m_aSkinPartColors[p] = pMsg->m_aSkinPartColors[p];
			}

			// send clear vote options
			CNetMsg_Sv_VoteClearOptions ClearMsg;
			Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

			// begin sending vote options
			pPlayer->m_SendVoteIndex = 0;

			// send tuning parameters to client
			SendTuningParams(ClientID);

			// client is ready to enter
			pPlayer->m_IsReadyToEnter = true;
			CNetMsg_Sv_ReadyToEnter m;
			Server()->SendPackMsg(&m, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);
		}
	}
}

void CGameContext::ConTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float NewValue = pResult->NumArguments() == 2 ? pResult->GetFloat(1) : -1;
	char aBuf[256];
	float Value;

	if(NewValue != -1 && pSelf->Tuning()->Set(pParamName, NewValue))
	{
		str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		pSelf->SendTuningParams(-1);
	}
	else if (pSelf->Tuning()->Get(pParamName, &Value))
	{
		str_format(aBuf, sizeof(aBuf), "Value: %.2f", Value);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
}

void CGameContext::ConToggleTuneParam(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	const char* pParamName = pResult->GetString(0);
	float OldValue;

	if (!pSelf->Tuning()->Get(pParamName, &OldValue))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
		return;
	}

	float NewValue = fabs(OldValue - pResult->GetFloat(1)) < 0.0001f
		? pResult->GetFloat(2)
		: pResult->GetFloat(1);

	pSelf->Tuning()->Set(pParamName, NewValue);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	pSelf->SendTuningParams(-1);
}

void CGameContext::ConTuneReset(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	pSelf->ResetTuning();
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");
}

void CGameContext::ConTunes(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	char aBuf[256];
	for (int i = 0; i < pSelf->Tuning()->Num(); i++)
	{
		float Value;
		pSelf->Tuning()->Get(i, &Value);
		str_format(aBuf, sizeof(aBuf), "%s %.2f", pSelf->Tuning()->ms_apNames[i], Value);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConTuneZone(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int List = pResult->GetInteger(0);
	const char* pParamName = pResult->GetString(1);
	float NewValue = pResult->GetFloat(2);
	char aBuf[256];
	float Value;

	if (List >= 0 && List < NUM_TUNEZONES)
	{
		if (pSelf->TuningList()[List].Set(pParamName, NewValue))
		{
			str_format(aBuf, sizeof(aBuf), "%s in zone %d changed to %.2f", pParamName, List, NewValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
			pSelf->SendTuningParams(-1, List);
		}
		else if (pSelf->TuningList()[List].Get(pParamName, &Value))
		{
			str_format(aBuf, sizeof(aBuf), "Value: %.2f", Value);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", aBuf);
		}
		else
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
	}
}

void CGameContext::ConTuneDumpZone(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int List = pResult->GetInteger(0);
	char aBuf[256];
	if (List >= 0 && List < NUM_TUNEZONES)
	{
		for (int i = 0; i < pSelf->TuningList()[List].Num(); i++)
		{
			float v;
			pSelf->TuningList()[List].Get(i, &v);
			str_format(aBuf, sizeof(aBuf), "zone %d: %s %.2f", List, pSelf->TuningList()[List].ms_apNames[i], v);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		}
	}
}

void CGameContext::ConTuneResetZone(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	CTuningParams TuningParams;
	if (pResult->NumArguments())
	{
		int List = pResult->GetInteger(0);
		if (List >= 0 && List < NUM_TUNEZONES)
		{
			pSelf->TuningList()[List] = TuningParams;
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "Tunezone %d reset", List);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
			pSelf->SendTuningParams(-1, List);
		}
	}
	else
	{
		for (int i = 0; i < NUM_TUNEZONES; i++)
		{
			*(pSelf->TuningList() + i) = TuningParams;
			pSelf->SendTuningParams(-1, i);
		}
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "All Tunezones reset");
	}
}

void CGameContext::ConTuneSetZoneMsgEnter(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	if (pResult->NumArguments())
	{
		int List = pResult->GetInteger(0);
		if (List >= 0 && List < NUM_TUNEZONES)
		{
			str_copy(pSelf->m_aaZoneEnterMsg[List], pResult->GetString(1), sizeof(pSelf->m_aaZoneEnterMsg[List]));
		}
	}
}

void CGameContext::ConTuneSetZoneMsgLeave(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	if (pResult->NumArguments())
	{
		int List = pResult->GetInteger(0);
		if (List >= 0 && List < NUM_TUNEZONES)
		{
			str_copy(pSelf->m_aaZoneLeaveMsg[List], pResult->GetString(1), sizeof(pSelf->m_aaZoneLeaveMsg[List]));
		}
	}
}

void CGameContext::ConSwitchOpen(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Switch = pResult->GetInteger(0);

	if (pSelf->Collision()->m_NumSwitchers > 0 && Switch >= 0 && Switch < pSelf->Collision()->m_NumSwitchers + 1)
	{
		pSelf->Collision()->m_pSwitchers[Switch].m_Initial = false;
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "switch %d opened by default", Switch);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	}
}

void CGameContext::ConPause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->m_World.m_Paused ^= 1;
}

void CGameContext::ConChangeMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->ChangeMap(pResult->NumArguments() ? pResult->GetString(0) : "");
}

void CGameContext::ConRestart(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->StartRound();
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChat(-1, CHAT_ALL, -1, pResult->GetString(0));
}

void CGameContext::ConBroadcast(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendBroadcast(pResult->GetString(0), -1);
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS - 1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	int Delay = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : 0;
	if (!pSelf->m_apPlayers[ClientID])
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientID, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientID]->Pause(CPlayer::PAUSE_NONE, false); // reset /spec and /pause to allow rejoin
	pSelf->m_apPlayers[ClientID]->m_TeamChangeTick = pSelf->Server()->Tick() + pSelf->Server()->TickSpeed() * Delay * 60;
	pSelf->m_apPlayers[ClientID]->SetTeam(Team);
	if (Team == TEAM_SPECTATORS)
		pSelf->m_apPlayers[ClientID]->Pause(CPlayer::PAUSE_NONE, true);
}

void CGameContext::ConSetTeamAll(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Team = clamp(pResult->GetInteger(0), -1, 1);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "All players were moved to the %s", pSelf->m_pController->GetTeamName(Team));
	pSelf->SendChat(-1, CHAT_ALL, -1, aBuf);

	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (pSelf->m_apPlayers[i])
			pSelf->m_apPlayers[i]->SetTeam(Team, false);
}

void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	if(pSelf->m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of vote options reached");
		return;
	}

	// check for valid option
	if(!pSelf->Console()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	while(*pDescription == ' ')
		pDescription++;
	if(str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// check for duplicate entry
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}

	// add the option
	++pSelf->m_NumVoteOptions;
	int Len = str_length(pCommand);

	pOption = (CVoteOptionServer *)pSelf->m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = pSelf->m_pVoteOptionLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	pSelf->m_pVoteOptionLast = pOption;
	if(!pSelf->m_pVoteOptionFirst)
		pSelf->m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, pCommand, Len+1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConRemoveVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);

	// check for valid option
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			break;
		pOption = pOption->m_pNext;
	}
	if(!pOption)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "option '%s' does not exist", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// inform clients about removed option
	CNetMsg_Sv_VoteOptionRemove OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);

	// TODO: improve this
	// remove the option
	--pSelf->m_NumVoteOptions;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "removed option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	CHeap *pVoteOptionHeap = new CHeap();
	CVoteOptionServer *pVoteOptionFirst = 0;
	CVoteOptionServer *pVoteOptionLast = 0;
	int NumVoteOptions = pSelf->m_NumVoteOptions;
	for(CVoteOptionServer *pSrc = pSelf->m_pVoteOptionFirst; pSrc; pSrc = pSrc->m_pNext)
	{
		if(pSrc == pOption)
			continue;

		// copy option
		int Len = str_length(pSrc->m_aCommand);
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
		pDst->m_pNext = 0;
		pDst->m_pPrev = pVoteOptionLast;
		if(pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pVoteOptionLast = pDst;
		if(!pVoteOptionFirst)
			pVoteOptionFirst = pDst;

		str_copy(pDst->m_aDescription, pSrc->m_aDescription, sizeof(pDst->m_aDescription));
		mem_copy(pDst->m_aCommand, pSrc->m_aCommand, Len+1);
	}

	// clean up
	delete pSelf->m_pVoteOptionHeap;
	pSelf->m_pVoteOptionHeap = pVoteOptionHeap;
	pSelf->m_pVoteOptionFirst = pVoteOptionFirst;
	pSelf->m_pVoteOptionLast = pVoteOptionLast;
	pSelf->m_NumVoteOptions = NumVoteOptions;
}

void CGameContext::ConClearVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "cleared votes");
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	pSelf->m_pVoteOptionHeap->Reset();
	pSelf->m_pVoteOptionFirst = 0;
	pSelf->m_pVoteOptionLast = 0;
	pSelf->m_NumVoteOptions = 0;

	// reset sending of vote options
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(pSelf->m_apPlayers[i])
			pSelf->m_apPlayers[i]->m_SendVoteIndex = 0;
	}
}

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	// check if there is a vote running
	if(!pSelf->m_VoteCloseTime)
		return;

	if(str_comp_nocase(pResult->GetString(0), "yes") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_YES;
	else if(str_comp_nocase(pResult->GetString(0), "no") == 0)
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pResult->GetString(0));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConchainUpdateHidePlayers(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if (pResult->NumArguments())
	{
		CGameContext* pSelf = (CGameContext*)pUserData;
		pSelf->UpdateHidePlayers();
	}
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		char aMotd[900];
		str_copy(aMotd, pSelf->FormatMotd(pSelf->Config()->m_SvMotd), sizeof(aMotd));
		for (int i = 0; i < MAX_CLIENTS; ++i)
			if (pSelf->m_apPlayers[i])
				pSelf->SendMotd(aMotd, i);
	}
}

void CGameContext::ConchainSettingUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		pSelf->SendSettings(-1);
	}
}

void CGameContext::ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGameContext *pSelf = (CGameContext *)pUserData;
		if(pSelf->m_pController)
			pSelf->m_pController->UpdateGameInfo(-1);
	}
}

void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	m_ChatPrintCBIndex = Console()->RegisterPrintCallback(0, SendChatResponse, this);

	Console()->Register("tune", "s[tuning] ?i[value]", CFGFLAG_SERVER|CFGFLAG_GAME, ConTuneParam, this, "Tune variable to value", AUTHED_ADMIN);
	Console()->Register("tune_reset", "", CFGFLAG_SERVER|CFGFLAG_GAME, ConTuneReset, this, "Reset all tuning variables to defaults", AUTHED_ADMIN);
	Console()->Register("tunes", "", CFGFLAG_SERVER, ConTunes, this, "List all tuning variables and their values", AUTHED_HELPER);
	Console()->Register("tune_zone", "i[zone] s[tuning] ?i[value]", CFGFLAG_SERVER|CFGFLAG_GAME, ConTuneZone, this, "Tune in zone a variable to value", AUTHED_ADMIN);
	Console()->Register("tune_zone_dump", "i[zone]", CFGFLAG_SERVER, ConTuneDumpZone, this, "Dump zone tuning in zone x", AUTHED_HELPER);
	Console()->Register("tune_zone_reset", "?i[zone]", CFGFLAG_SERVER, ConTuneResetZone, this, "reset zone tuning in zone x or in all zones", AUTHED_ADMIN);
	Console()->Register("tune_zone_enter", "i[zone] s[message]", CFGFLAG_SERVER|CFGFLAG_GAME, ConTuneSetZoneMsgEnter, this, "which message to display on zone enter; use 0 for normal area", AUTHED_ADMIN);
	Console()->Register("tune_zone_leave", "i[zone] s[message]", CFGFLAG_SERVER|CFGFLAG_GAME, ConTuneSetZoneMsgLeave, this, "which message to display on zone leave; use 0 for normal area", AUTHED_ADMIN);
	Console()->Register("switch_open", "i[switch]", CFGFLAG_SERVER|CFGFLAG_GAME, ConSwitchOpen, this, "Whether a switch is deactivated by default (otherwise activated)", AUTHED_ADMIN);

	Console()->Register("pausegame", "?i[on/off]", CFGFLAG_SERVER|CFGFLAG_STORE, ConPause, this, "Pause/unpause game", AUTHED_ADMIN);
	Console()->Register("change_map", "?r[map]", CFGFLAG_SERVER|CFGFLAG_STORE, ConChangeMap, this, "Change map", AUTHED_ADMIN);
	Console()->Register("restart", "?i[seconds]", CFGFLAG_SERVER|CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)", AUTHED_ADMIN);
	Console()->Register("say", "r[message]", CFGFLAG_SERVER, ConSay, this, "Say in chat", AUTHED_MOD);
	Console()->Register("broadcast", "r[message]", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message", AUTHED_MOD);
	Console()->Register("set_team", "i[id] i[team-id] ?i[delay in minutes]", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team", AUTHED_ADMIN);
	Console()->Register("set_team_all", "i[team-id]", CFGFLAG_SERVER, ConSetTeamAll, this, "Set team of all players to team", AUTHED_ADMIN);

	Console()->Register("add_vote", "s[name] r[command]", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option", AUTHED_ADMIN);
	Console()->Register("remove_vote", "s[name]", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option", AUTHED_ADMIN);
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options", AUTHED_ADMIN);
	Console()->Register("vote", "r['yes'|'no']", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no", AUTHED_ADMIN);

	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);

	Console()->Chain("sv_vote_kick", ConchainSettingUpdate, this);
	Console()->Chain("sv_vote_kick_min", ConchainSettingUpdate, this);
	Console()->Chain("sv_vote_spectate", ConchainSettingUpdate, this);
	Console()->Chain("sv_player_slots", ConchainSettingUpdate, this);
	Console()->Chain("sv_max_clients", ConchainSettingUpdate, this);

	Console()->Chain("sv_scorelimit", ConchainGameinfoUpdate, this);
	Console()->Chain("sv_timelimit", ConchainGameinfoUpdate, this);

	Console()->Register("random_map", "?i[stars]", CFGFLAG_SERVER, ConRandomMap, this, "Random map", AUTHED_ADMIN);
	Console()->Register("random_unfinished_map", "?i[stars]", CFGFLAG_SERVER, ConRandomUnfinishedMap, this, "Random unfinished map", AUTHED_ADMIN);

	// F-DDrace
	Console()->Chain("sv_hide_minigame_players", ConchainUpdateHidePlayers, this);
	Console()->Chain("sv_hide_dummies", ConchainUpdateHidePlayers, this);

	#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help, accesslevel) m_pConsole->Register(name, params, flags, callback, userdata, help, accesslevel);
	#include <game/ddracecommands.h>
	#undef CONSOLE_COMMAND

	// Keep this for backwards compatibility
	#define CHAT_COMMAND(name, params, flags, callback, userdata, help, accesslevel) m_pConsole->Register(name, params, flags, callback, userdata, help, accesslevel);
	#include "ddracechat.h"
	#undef CHAT_COMMAND
}

void CGameContext::NewCommandHook(const CCommandManager::CCommand *pCommand, void *pContext)
{
	CGameContext *pSelf = (CGameContext *)pContext;
	pSelf->SendChatCommand(pCommand, -1);
}

void CGameContext::RemoveCommandHook(const CCommandManager::CCommand *pCommand, void *pContext)
{
	CGameContext *pSelf = (CGameContext *)pContext;
	pSelf->SendRemoveChatCommand(pCommand, -1);
}

struct SLegacyCommandContext {
	CGameContext *m_pGameContext;
	IConsole::FCommandCallback m_pfnCallback;
	void *m_pOriginalContext;
};

void CGameContext::LegacyCommandCallback(IConsole::IResult *pResult, void *pContext)
{
	CCommandManager::SCommandContext *pComContext = (CCommandManager::SCommandContext *)pContext;
	SLegacyCommandContext *pLegacyContext = (SLegacyCommandContext *)pComContext->m_pContext;
	CGameContext *pSelf = pLegacyContext->m_pGameContext;

	//Do Spam protection
	CPlayer *pPlayer = pSelf->m_apPlayers[pComContext->m_ClientID];
	const int64 Now = pSelf->Server()->Tick();
	const int64 TickSpeed = pSelf->Server()->TickSpeed();

	if (pSelf->Config()->m_SvSpamprotection && !str_startswith(pComContext->m_pCommand, "timeout ")
		&& pPlayer->m_LastCommands[0] && pPlayer->m_LastCommands[0] + TickSpeed > Now
		&& pPlayer->m_LastCommands[1] && pPlayer->m_LastCommands[1] + TickSpeed > Now
		&& pPlayer->m_LastCommands[2] && pPlayer->m_LastCommands[2] + TickSpeed > Now
		&& pPlayer->m_LastCommands[3] && pPlayer->m_LastCommands[3] + TickSpeed > Now
		)
		return;

	pPlayer->m_LastCommands[pPlayer->m_LastCommandPos] = Now;
	pPlayer->m_LastCommandPos = (pPlayer->m_LastCommandPos + 1) % 4;

	// Patch up the Result
	pResult->m_ClientID = pComContext->m_ClientID;

	// Set up the console output
	pSelf->m_ChatResponseTargetID = pComContext->m_ClientID;
	pSelf->Server()->RestrictRconOutput(pComContext->m_ClientID);
	pSelf->Console()->SetFlagMask(CFGFLAG_CHAT);

	int Authed = pSelf->Server()->GetAuthedState(pComContext->m_ClientID);
	if (Authed)
		pSelf->Console()->SetAccessLevel(Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD : IConsole::ACCESS_LEVEL_HELPER);
	else
		pSelf->Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_USER);
	pSelf->Console()->SetPrintOutputLevel(pSelf->m_ChatPrintCBIndex, 0);

	pLegacyContext->m_pfnCallback(pResult, pLegacyContext->m_pOriginalContext);

	// Fix the console output
	pSelf->Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
	pSelf->Console()->SetFlagMask(CFGFLAG_SERVER);
	pSelf->m_ChatResponseTargetID = -1;
	pSelf->Server()->RestrictRconOutput(-1);
}

void CGameContext::RegisterLegacyDDRaceCommands()
{
	#define CHAT_COMMAND(name, params, flags, callback, userdata, help, accesslevel) \
	{ \
		static SLegacyCommandContext Context = { this, callback, userdata}; \
		CommandManager()->AddCommand(name, help, params, LegacyCommandCallback, &Context); \
	}

	#include "ddracechat.h"
	#undef CHAT_COMMAND
}

void CGameContext::OnInit()
{
	// init everything
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConfig = Kernel()->RequestInterface<IConfigManager>()->Values();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);
	m_CommandManager.Init(m_pConsole, this, NewCommandHook, RemoveCommandHook);

	m_GameUuid = RandomUuid();
	Console()->SetTeeHistorianCommandCallback(CommandCallback, this);
	Console()->SetIsDummyCallback(ConsoleIsDummyCallback, this);

	DeleteTempfile();

	// HACK: only set static size for items, which were available in the first 0.7 release
	// so new items don't break the snapshot delta
	static const int OLD_NUM_NETOBJTYPES = 23;
	for(int i = 0; i < OLD_NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers, m_pConfig);

	// Reset Tunezones
	CTuningParams TuningParams;
	for (int i = 0; i < NUM_TUNEZONES; i++)
	{
		TuningList()[i] = TuningParams;
		TuningList()[i].Set("gun_curvature", 0);
		TuningList()[i].Set("gun_speed", 1400);
		TuningList()[i].Set("shotgun_curvature", 0);
		TuningList()[i].Set("shotgun_speed", 500);
	}

	for (int i = 0; i < NUM_TUNEZONES; i++)
	{
		// Send no text by default when changing tune zones.
		m_aaZoneEnterMsg[i][0] = 0;
		m_aaZoneLeaveMsg[i][0] = 0;
	}
	// Reset Tuning
	if (Config()->m_SvTuneReset)
	{
		ResetTuning();
	}
	else
	{
		Tuning()->Set("gun_speed", 1400);
		Tuning()->Set("gun_curvature", 0);
		Tuning()->Set("shotgun_speed", 500);
		Tuning()->Set("shotgun_curvature", 0);
	}

	if (Config()->m_SvDDRaceTuneReset)
	{
		Config()->m_SvHit = 1;
		Config()->m_SvEndlessDrag = 0;
		Config()->m_SvOldLaser = 0;
		Config()->m_SvOldTeleportHook = 0;
		Config()->m_SvOldTeleportWeapons = 0;
		Config()->m_SvTeleportHoldHook = 0;
		Config()->m_SvTeam = 1;
		Config()->m_SvShowOthersDefault = 0;

		if (Collision()->m_NumSwitchers > 0)
			for (int i = 0; i < Collision()->m_NumSwitchers + 1; ++i)
				Collision()->m_pSwitchers[i].m_Initial = true;
	}

	Console()->ExecuteFile(Config()->m_SvResetFile, -1);

	LoadMapSettings();

	m_pController = new CGameControllerDDRace(this);
	((CGameControllerDDRace*)m_pController)->m_Teams.Reset();
	m_pController->RegisterChatCommands(CommandManager());

	RegisterLegacyDDRaceCommands();

	m_TeeHistorianActive = Config()->m_SvTeeHistorian;
	if(m_TeeHistorianActive)
	{
		char aGameUuid[UUID_MAXSTRSIZE];
		FormatUuid(m_GameUuid, aGameUuid, sizeof(aGameUuid));

		char aFilename[64];
		str_format(aFilename, sizeof(aFilename), "teehistorian/%s.teehistorian", aGameUuid);

		m_TeeHistorianFile = Kernel()->RequestInterface<IStorage>()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!m_TeeHistorianFile)
		{
			dbg_msg("teehistorian", "failed to open '%s'", aFilename);
			exit(1);
		}
		else
		{
			dbg_msg("teehistorian", "recording to '%s'", aFilename);
		}

		char aVersion[128];
		str_format(aVersion, sizeof(aVersion), "%s", GAME_VERSION);
		CTeeHistorian::CGameInfo GameInfo;
		GameInfo.m_GameUuid = m_GameUuid;
		GameInfo.m_pServerVersion = aVersion;
		GameInfo.m_StartTime = time(0);

		GameInfo.m_pServerName = Config()->m_SvName;
		GameInfo.m_ServerPort = Config()->m_SvPort;
		GameInfo.m_pGameType = m_pController->GetGameType();

		GameInfo.m_pConfig = Config();
		GameInfo.m_pTuning = Tuning();
		GameInfo.m_pUuids = &g_UuidManager;

		char aMapName[128];
		Server()->GetMapInfo(aMapName, sizeof(aMapName), &GameInfo.m_MapSize, &GameInfo.m_MapSha256, &GameInfo.m_MapCrc);
		GameInfo.m_pMapName = aMapName;

		m_TeeHistorian.Reset(&GameInfo, TeeHistorianWrite, this);

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			int Level = Server()->GetAuthedState(i);
			if(Level)
			{
				m_TeeHistorian.RecordAuthInitial(i, Level, Server()->AuthName(i));
			}
		}
		io_flush(m_TeeHistorianFile);
	}

	if (Config()->m_SvSoloServer)
	{
		Config()->m_SvTeam = 3;
		Config()->m_SvShowOthersDefault = 1;

		Tuning()->Set("player_collision", 0);
		Tuning()->Set("player_hooking", 0);

		for (int i = 0; i < NUM_TUNEZONES; i++)
		{
			TuningList()[i].Set("player_collision", 0);
			TuningList()[i].Set("player_hooking", 0);
		}
	}

	// delete old score object
	if (m_pScore)
		delete m_pScore;

	// create score object (add sql later)
#if defined(CONF_SQL)
	if(Config()->m_SvUseSQL)
		m_pScore = new CSqlScore(this);
	else
#endif
		m_pScore = new CFileScore(this);

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);

	CTile* pFront = 0;
	CSwitchTile* pSwitch = 0;
	if (m_Layers.FrontLayer())
		pFront = (CTile*)Kernel()->RequestInterface<IMap>()->GetData(m_Layers.FrontLayer()->m_Front);
	if (m_Layers.SwitchLayer())
		pSwitch = (CSwitchTile*)Kernel()->RequestInterface<IMap>()->GetData(m_Layers.SwitchLayer()->m_Switch);

	// F-DDrace
	Collision()->m_vTiles.clear();
	Collision()->m_vTiles.resize(NUM_INDICES);

	for (int y = 0; y < pTileMap->m_Height; y++)
	{
		for (int x = 0; x < pTileMap->m_Width; x++)
		{
			int Index = pTiles[y * pTileMap->m_Width + x].m_Index;
			vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);

			Collision()->m_vTiles[Index].push_back(Pos);

			if (Index == TILE_OLDLASER)
			{
				Config()->m_SvOldLaser = 1;
				dbg_msg("game layer", "found old laser tile");
			}
			else if (Index == TILE_NPC)
			{
				m_Tuning.Set("player_collision", 0);
				dbg_msg("game layer", "found no collision tile");
			}
			else if (Index == TILE_EHOOK)
			{
				Config()->m_SvEndlessDrag = 1;
				dbg_msg("game layer", "found unlimited hook time tile");
			}
			else if (Index == TILE_NOHIT)
			{
				Config()->m_SvHit = 0;
				dbg_msg("game layer", "found no weapons hitting others tile");
			}
			else if (Index == TILE_NPH)
			{
				m_Tuning.Set("player_hooking", 0);
				dbg_msg("game layer", "found no player hooking tile");
			}

			if (Index >= ENTITY_OFFSET)
			{
				m_pController->OnEntity(Index, Pos, LAYER_GAME, pTiles[y * pTileMap->m_Width + x].m_Flags);
			}

			if (pFront)
			{
				Index = pFront[y * pTileMap->m_Width + x].m_Index;

				Collision()->m_vTiles[Index].push_back(vec2(x*32.0f+16.0f, y*32.0f+16.0f));

				if (Index == TILE_OLDLASER)
				{
					Config()->m_SvOldLaser = 1;
					dbg_msg("front layer", "found old laser tile");
				}
				else if (Index == TILE_NPC)
				{
					m_Tuning.Set("player_collision", 0);
					dbg_msg("front layer", "found no collision tile");
				}
				else if (Index == TILE_EHOOK)
				{
					Config()->m_SvEndlessDrag = 1;
					dbg_msg("front layer", "found unlimited hook time tile");
				}
				else if (Index == TILE_NOHIT)
				{
					Config()->m_SvHit = 0;
					dbg_msg("front layer", "found no weapons hitting others tile");
				}
				else if (Index == TILE_NPH)
				{
					m_Tuning.Set("player_hooking", 0);
					dbg_msg("front layer", "found no player hooking tile");
				}
				if (Index >= ENTITY_OFFSET)
				{
					m_pController->OnEntity(Index, Pos, LAYER_FRONT, pFront[y * pTileMap->m_Width + x].m_Flags);
				}
			}
			if (pSwitch)
			{
				Index = pSwitch[y * pTileMap->m_Width + x].m_Type;
				// TODO: Add off by default door here
				// if (Index == TILE_DOOR_OFF)
				if (Index >= ENTITY_OFFSET)
				{
					m_pController->OnEntity(Index, Pos, LAYER_SWITCH, pSwitch[y * pTileMap->m_Width + x].m_Flags, pSwitch[y * pTileMap->m_Width + x].m_Number);
				}
			}
		}
	}

	// clamp sv_player_slots to 0..MaxClients
	if(Config()->m_SvMaxClients < Config()->m_SvPlayerSlots)
		Config()->m_SvPlayerSlots = Config()->m_SvMaxClients;


	// F-DDrace

	// check if there are minigame spawns available (survival and instagib are checked in their own ticks)
	m_aMinigameDisabled[MINIGAME_BLOCK] = Collision()->GetRandomTile(TILE_MINIGAME_BLOCK) == vec2(-1, -1);

	m_SurvivalGameState = SURVIVAL_OFFLINE;
	m_SurvivalBackgroundState = SURVIVAL_OFFLINE;
	m_SurvivalTick = 0;
	m_SurvivalWinner = -1;

	if (Config()->m_SvDefaultDummies)
		ConnectDefaultDummies();
	SetV3Offset(Config()->m_V3OffsetX, Config()->m_V3OffsetY);


	int NeededXP[] = { 5000, 15000, 25000, 35000, 50000, 65000, 80000, 100000, 120000, 130000, 160000, 200000, 240000, 280000, 325000, 370000, 420000, 470000, 520000, 600000,
	680000, 760000, 850000, 950000, 1200000, 1400000, 1600000, 1800000, 2000000, 2210000, 2430000, 2660000, 2900000, 3150000, 3500000, 3950000, 4500000, 5250000, 6100000, 7000000,
	8000000, 9000000, 10000000, 11000000, 12000000, 13000000, 14000000, 15000000, 16000000, 17000000, 18000000, 19000000, 20000000, 21000000, 22000000, 23000000, 24000000, 25000000,
	26000000, 27000000, 28000000, 29000000, 30000000, 31000000, 32000000, 33000000, 34000000, 35000000, 36000000, 37000000, 38000000, 39000000, 40000000, 41010000, 42020000, 43030000,
	44040000, 45050000, 46060000, 47070000, 48080000, 49090000, 50100000, 51110000, 52120000, 53130000, 54140000, 55150000, 56160000, 57170000, 58180000, 59190000, 60200000, 61300000,
	62400000, 63500000, 64600000, 65700000, 66800000, 67900000 };

	for (int i = 0; i < MAX_LEVEL; i++)
		m_aNeededXP[i] = NeededXP[i];
	m_aNeededXP[MAX_LEVEL] = NeededXP[MAX_LEVEL-1];

	int TaserPrice[] = { 50000, 75000, 100000, 150000, 200000, 200000, 200000 };
	for (int i = 0; i < NUM_TASER_LEVELS; i++)
		m_aTaserPrice[i] = TaserPrice[i];

	int PoliceLevel[] = { 18, 25, 30, 40, 50 };
	for (int i = 0; i < NUM_POLICE_LEVELS; i++)
		m_aPoliceLevel[i] = PoliceLevel[i];

	AddAccount(); // account id 0 means not logged in, so we add an unused account with id 0
	m_LogoutAccountsPort = Config()->m_SvPort; // set before calling LogoutAccountsCallback
	Storage()->ListDirectory(IStorage::TYPE_ALL, Config()->m_SvAccFilePath, LogoutAccountsCallback, this);

	m_LastAccSaveTick = Server()->Tick();

	if (m_pShop)
		delete m_pShop;
	m_pShop = new CShop(this);


#ifdef CONF_DEBUG
	// clamp dbg_dummies to 0..MAX_CLIENTS-1
	if(MAX_CLIENTS <= Config()->m_DbgDummies)
		Config()->m_DbgDummies = MAX_CLIENTS;
	if(Config()->m_DbgDummies)
	{
		for(int i = 0; i < Config()->m_DbgDummies ; i++)
			OnClientConnected(MAX_CLIENTS -i-1, true, false);
	}
#endif
}

void CGameContext::DeleteTempfile()
{
	if (m_aDeleteTempfile[0] != 0)
	{
		Storage()->RemoveFile(m_aDeleteTempfile, IStorage::TYPE_SAVE);
		m_aDeleteTempfile[0] = 0;
	}
}

void CGameContext::OnMapChange(char* pNewMapName, int MapNameSize)
{
	for (unsigned int i = ACC_START; i < m_Accounts.size(); i++)
		Logout(i);

	char aConfig[128];
	char aTemp[128];
	str_format(aConfig, sizeof(aConfig), "maps/%s.cfg", Config()->m_SvMap);
	str_format(aTemp, sizeof(aTemp), "%s.temp.%d", pNewMapName, pid());

	IOHANDLE File = Storage()->OpenFile(aConfig, IOFLAG_READ, IStorage::TYPE_ALL);
	if (!File)
	{
		// No map-specific config, just return.
		return;
	}
	CLineReader LineReader;
	LineReader.Init(File);

	array<char*> aLines;
	char* pLine;
	int TotalLength = 0;
	while ((pLine = LineReader.Get()))
	{
		int Length = str_length(pLine) + 1;
		char* pCopy = (char*)malloc(Length);
		mem_copy(pCopy, pLine, Length);
		aLines.add(pCopy);
		TotalLength += Length;
	}
	io_close(File);

	char* pSettings = (char*)malloc(TotalLength);
	int Offset = 0;
	for (int i = 0; i < aLines.size(); i++)
	{
		int Length = str_length(aLines[i]) + 1;
		mem_copy(pSettings + Offset, aLines[i], Length);
		Offset += Length;
		free(aLines[i]);
	}

	CDataFileReader Reader;
	Reader.Open(Storage(), pNewMapName, IStorage::TYPE_ALL);

	CDataFileWriter Writer;
	Writer.Init();

	int SettingsIndex = Reader.NumData();
	bool FoundInfo = false;
	for (int i = 0; i < Reader.NumItems(); i++)
	{
		int TypeID;
		int ItemID;
		int* pData = (int*)Reader.GetItem(i, &TypeID, &ItemID);
		int Size = Reader.GetItemSize(i);
		CMapItemInfoSettings MapInfo;
		if (TypeID == MAPITEMTYPE_INFO && ItemID == 0)
		{
			FoundInfo = true;
			CMapItemInfoSettings* pInfo = (CMapItemInfoSettings*)pData;
			if (Size >= (int)sizeof(CMapItemInfoSettings))
			{
				if (pInfo->m_Settings > -1)
				{
					SettingsIndex = pInfo->m_Settings;
					char* pMapSettings = (char*)Reader.GetData(SettingsIndex);
					int DataSize = Reader.GetDataSize(SettingsIndex);
					if (DataSize == TotalLength && mem_comp(pSettings, pMapSettings, DataSize) == 0)
					{
						// Configs coincide, no need to update map.
						return;
					}
					Reader.UnloadData(pInfo->m_Settings);
				}
				else
				{
					MapInfo = *pInfo;
					MapInfo.m_Settings = SettingsIndex;
					pData = (int*)& MapInfo;
					Size = sizeof(MapInfo);
				}
			}
			else
			{
				*(CMapItemInfo*)& MapInfo = *(CMapItemInfo*)pInfo;
				MapInfo.m_Settings = SettingsIndex;
				pData = (int*)& MapInfo;
				Size = sizeof(MapInfo);
			}
		}
		Writer.AddItem(TypeID, ItemID, Size, pData);
	}

	if (!FoundInfo)
	{
		CMapItemInfoSettings Info;
		Info.m_Version = 1;
		Info.m_Author = -1;
		Info.m_MapVersion = -1;
		Info.m_Credits = -1;
		Info.m_License = -1;
		Info.m_Settings = SettingsIndex;
		Writer.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Info), &Info);
	}

	for (int i = 0; i < Reader.NumData() || i == SettingsIndex; i++)
	{
		if (i == SettingsIndex)
		{
			Writer.AddData(TotalLength, pSettings);
			continue;
		}
		unsigned char* pData = (unsigned char*)Reader.GetData(i);
		int Size = Reader.GetDataSize(i);
		Writer.AddData(Size, pData);
		Reader.UnloadData(i);
	}

	dbg_msg("mapchange", "imported settings");
	Reader.Close();
	Writer.OpenFile(Storage(), aTemp);
	Writer.Finish();

	str_copy(pNewMapName, aTemp, MapNameSize);
	str_copy(m_aDeleteTempfile, aTemp, sizeof(m_aDeleteTempfile));
}

void CGameContext::OnShutdown(bool FullShutdown)
{
	for (unsigned int i = ACC_START; i < m_Accounts.size(); i++)
		Logout(i);

	if (FullShutdown)
		Score()->OnShutdown();

	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.Finish();
		io_close(m_TeeHistorianFile);
	}

	DeleteTempfile();
	Console()->ResetServerGameSettings();
	Collision()->Dest();
	delete m_pController;
	m_pController = 0;
	delete m_pShop;
	m_pShop = 0;
	Clear();
}

void CGameContext::LoadMapSettings()
{
	IMap *pMap = Kernel()->RequestInterface<IMap>();
	int Start, Num;
	pMap->GetType(MAPITEMTYPE_INFO, &Start, &Num);
	for(int i = Start; i < Start + Num; i++)
	{
		int ItemID;
		CMapItemInfoSettings *pItem = (CMapItemInfoSettings *)pMap->GetItem(i, 0, &ItemID);
		int ItemSize = pMap->GetItemSize(i);
		if(!pItem || ItemID != 0)
			continue;

		if(ItemSize < (int)sizeof(CMapItemInfoSettings))
			break;
		if(!(pItem->m_Settings > -1))
			break;

		int Size = pMap->GetDataSize(pItem->m_Settings);
		char *pSettings = (char *)pMap->GetData(pItem->m_Settings);
		char *pNext = pSettings;
		while(pNext < pSettings + Size)
		{
			int StrSize = str_length(pNext) + 1;
			Console()->ExecuteLine(pNext, IConsole::CLIENT_ID_GAME);
			pNext += StrSize;
		}
		pMap->UnloadData(pItem->m_Settings);
		break;
	}

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "maps/%s.map.cfg", Config()->m_SvMap);
	Console()->ExecuteFile(aBuf, IConsole::CLIENT_ID_NO_GAME);
}

void CGameContext::OnSnap(int ClientID)
{
	// add tuning to demo
	CTuningParams StandardTuning;
	if(ClientID == -1 && Server()->DemoRecorder_IsRecording() && mem_comp(&StandardTuning, &m_Tuning, sizeof(CTuningParams)) != 0)
	{
		CNetObj_De_TuneParams *pTuneParams = static_cast<CNetObj_De_TuneParams *>(Server()->SnapNewItem(NETOBJTYPE_DE_TUNEPARAMS, 0, sizeof(CNetObj_De_TuneParams)));
		if(!pTuneParams)
			return;

		mem_copy(pTuneParams->m_aTuneParams, &m_Tuning, sizeof(pTuneParams->m_aTuneParams));
	}

	m_World.Snap(ClientID);
	m_pController->Snap(ClientID);
	m_Events.Snap(ClientID);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
			m_apPlayers[i]->Snap(ClientID);
	}
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	m_World.PostSnap();
	m_Events.Clear();
}

bool CGameContext::IsClientReady(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsReadyToEnter;
}

bool CGameContext::IsClientPlayer(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() != TEAM_SPECTATORS;
}

bool CGameContext::IsClientSpectator(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS;
}

const CUuid CGameContext::GameUuid() const { return m_GameUuid; }
const char *CGameContext::GameType() const { return m_pController && m_pController->GetGameType() ? m_pController->GetGameType() : ""; }
const char *CGameContext::Version() const { return GAME_VERSION; }
const char *CGameContext::NetVersion() const { return GAME_NETVERSION; }
const char *CGameContext::NetVersionSevendown() const { return GAME_NETVERSION_SEVENDOWN; }

IGameServer *CreateGameServer() { return new CGameContext; }

void CGameContext::SendChatResponseAll(const char* pLine, void* pUser)
{
	CGameContext* pSelf = (CGameContext*)pUser;

	static volatile int ReentryGuard = 0;
	const char* pLineOrig = pLine;

	if (ReentryGuard)
		return;
	ReentryGuard++;

	if (*pLine == '[')
		do
			pLine++;
	while ((pLine - 2 < pLineOrig || *(pLine - 2) != ':') && *pLine != 0);//remove the category (e.g. [Console]: No Such Command)

	pSelf->SendChat(-1, CHAT_ALL, -1, pLine);

	ReentryGuard--;
}

void CGameContext::SendChatResponse(const char* pLine, void* pUser, bool Highlighted)
{
	CGameContext* pSelf = (CGameContext*)pUser;
	int ClientID = pSelf->m_ChatResponseTargetID;

	if (ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;

	const char* pLineOrig = pLine;

	static volatile int ReentryGuard = 0;

	if (ReentryGuard)
		return;
	ReentryGuard++;

	if (pLine[0] == '[')
	{
		// Remove time and category: [20:39:00][Console]
		pLine = str_find(pLine, "]: ");
		if (pLine)
			pLine += 3;
		else
			pLine = pLineOrig;
	}

	pSelf->SendChatTarget(ClientID, pLine);

	ReentryGuard--;
}

bool CGameContext::PlayerCollision()
{
	float Temp;
	m_Tuning.Get("player_collision", &Temp);
	return Temp != 0.0f;
}

bool CGameContext::PlayerHooking()
{
	float Temp;
	m_Tuning.Get("player_hooking", &Temp);
	return Temp != 0.0f;
}

float CGameContext::PlayerJetpack()
{
	float Temp;
	m_Tuning.Get("player_jetpack", &Temp);
	return Temp;
}

int CGameContext::ProcessSpamProtection(int ClientID)
{
	if(!m_apPlayers[ClientID])
		return 0;
	if(Config()->m_SvSpamprotection && m_apPlayers[ClientID]->m_LastChat
		&& m_apPlayers[ClientID]->m_LastChat + Server()->TickSpeed() * Config()->m_SvChatDelay > Server()->Tick())
		return 1;
	else
		m_apPlayers[ClientID]->m_LastChat = Server()->Tick();
	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);
	int Muted = 0;

	for(int i = 0; i < m_NumMutes && !Muted; i++)
	{
		if(!net_addr_comp_noport(&Addr, &m_aMutes[i].m_Addr))
			Muted = (m_aMutes[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
	}

	if (Muted > 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof aBuf, "You are not permitted to talk for the next %d seconds.", Muted);
		SendChatTarget(ClientID, aBuf);
		return 1;
	}

	if ((m_apPlayers[ClientID]->m_ChatScore += Config()->m_SvChatPenalty) > Config()->m_SvChatThreshold)
	{
		Mute(&Addr, Config()->m_SvSpamMuteDuration, Server()->ClientName(ClientID));
		m_apPlayers[ClientID]->m_ChatScore = 0;
		return 1;
	}

	return 0;
}

int CGameContext::GetDDRaceTeam(int ClientID)
{
	CGameControllerDDRace* pController = (CGameControllerDDRace*)m_pController;
	return pController->m_Teams.m_Core.Team(ClientID);
}

void CGameContext::ResetTuning()
{
	CTuningParams TuningParams;
	m_Tuning = TuningParams;
	Tuning()->Set("gun_speed", 1400);
	Tuning()->Set("gun_curvature", 0);
	Tuning()->Set("shotgun_speed", 500);
	Tuning()->Set("shotgun_curvature", 0);
	SendTuningParams(-1);
}

void CGameContext::List(int ClientID, const char* pFilter)
{
	int Total = 0;
	char aBuf[256];
	int Bufcnt = 0;
	if (pFilter[0])
		str_format(aBuf, sizeof(aBuf), "Listing players with \"%s\" in name:", pFilter);
	else
		str_format(aBuf, sizeof(aBuf), "Listing all players:");
	SendChatTarget(ClientID, aBuf);
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i])
		{
			Total++;
			const char* pName = Server()->ClientName(i);
			if (str_find_nocase(pName, pFilter) == NULL)
				continue;
			if (Bufcnt + str_length(pName) + 4 > 256)
			{
				SendChatTarget(ClientID, aBuf);
				Bufcnt = 0;
			}
			if (Bufcnt != 0)
			{
				str_format(&aBuf[Bufcnt], sizeof(aBuf) - Bufcnt, ", %s", pName);
				Bufcnt += 2 + str_length(pName);
			}
			else
			{
				str_format(&aBuf[Bufcnt], sizeof(aBuf) - Bufcnt, "%s", pName);
				Bufcnt += str_length(pName);
			}
		}
	}
	if (Bufcnt != 0)
		SendChatTarget(ClientID, aBuf);
	str_format(aBuf, sizeof(aBuf), "%d players online", Total);
	SendChatTarget(ClientID, aBuf);
}

void CGameContext::ForceVote(int EnforcerID, bool Success)
{
	// check if there is a vote running
	if (!m_VoteCloseTime)
		return;

	m_VoteEnforce = Success ? CGameContext::VOTE_ENFORCE_YES_ADMIN : CGameContext::VOTE_ENFORCE_NO_ADMIN;
	m_VoteEnforcer = EnforcerID;

	char aBuf[256];
	const char* pOption = Success ? "yes" : "no";
	str_format(aBuf, sizeof(aBuf), "authorized player forced vote %s", pOption);
	SendChatTarget(-1, aBuf);
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pOption);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

// DDRace

void CGameContext::CallVote(int ClientID, const char *pDesc, const char *pCmd, const char *pReason, const char *pChatmsg)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	int64 Now = Server()->Tick();
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(!pPlayer)
		return;

	SendChat(-1, CHAT_ALL, -1, pChatmsg);
	StartVote(pDesc, pCmd, pReason);
	pPlayer->m_Vote = 1;
	pPlayer->m_VotePos = m_VotePos = 1;
	m_VoteCreator = ClientID;
	pPlayer->m_LastVoteCall = Now;
}

void CGameContext::ConRandomMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int Stars = pResult->NumArguments() ? pResult->GetInteger(0) : -1;

	pSelf->m_pScore->RandomMap(&pSelf->m_pRandomMapResult, pSelf->m_VoteCreator, Stars);
}

void CGameContext::ConRandomUnfinishedMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	int Stars = pResult->NumArguments() ? pResult->GetInteger(0) : -1;

	pSelf->m_pScore->RandomUnfinishedMap(&pSelf->m_pRandomMapResult, pSelf->m_VoteCreator, Stars);
}

// F-DDrace

void CGameContext::UpdateTopAccounts(int Type)
{
	// update top accounts with all currently online accs so we get correct and up-to-date information
	for (unsigned int i = ACC_START; i < m_Accounts.size(); i++)
		SetTopAccStats(i);

	switch (Type)
	{
	case TOP_LEVEL:		std::sort(m_TopAccounts.begin(), m_TopAccounts.end(), [](const TopAccounts& a, const TopAccounts& b) -> bool { return a.m_Level > b.m_Level; }); break;
	case TOP_POINTS:	std::sort(m_TopAccounts.begin(), m_TopAccounts.end(), [](const TopAccounts& a, const TopAccounts& b) -> bool { return a.m_Points > b.m_Points; }); break;
	case TOP_MONEY:		std::sort(m_TopAccounts.begin(), m_TopAccounts.end(), [](const TopAccounts& a, const TopAccounts& b) -> bool { return a.m_Money > b.m_Money; }); break;
	case TOP_SPREE:		std::sort(m_TopAccounts.begin(), m_TopAccounts.end(), [](const TopAccounts& a, const TopAccounts& b) -> bool { return a.m_KillStreak > b.m_KillStreak; }); break;
	}
}

int CGameContext::LogoutAccountsCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;

	if (!IsDir && str_endswith(pName, ".acc"))
	{
		char aUsername[64];
		str_copy(aUsername, pName, str_length(pName) - 3); // remove the .acc

		int ID = pSelf->GetAccount(aUsername);
		if (ID < ACC_START)
			return 0;

		// load all accounts into the top account list too
		pSelf->SetTopAccStats(ID);

		if (pSelf->m_Accounts[ID].m_LoggedIn && pSelf->m_Accounts[ID].m_Port == pSelf->m_LogoutAccountsPort)
			pSelf->Logout(ID, true);
		else
			pSelf->FreeAccount(ID, true);
	}

	return 0;
}

void CGameContext::SetTopAccStats(int FromID)
{
	for (unsigned int i = 0; i < m_TopAccounts.size(); i++)
	{
		// update if we have it in already
		if (!str_comp(m_Accounts[FromID].m_Username, m_TopAccounts[i].m_aAccountName))
		{
			m_TopAccounts[i].m_Level = m_Accounts[FromID].m_Level;
			m_TopAccounts[i].m_Points = m_Accounts[FromID].m_BlockPoints;
			m_TopAccounts[i].m_Money = m_Accounts[FromID].m_Money;
			m_TopAccounts[i].m_KillStreak = m_Accounts[FromID].m_KillingSpreeRecord;
			str_copy(m_TopAccounts[i].m_aUsername, m_Accounts[FromID].m_aLastPlayerName, sizeof(m_TopAccounts[i].m_aUsername));
			return;
		}
	}

	// if not existing in m_TopAccounts yet, add it
	CGameContext::TopAccounts Account;
	Account.m_Level = m_Accounts[FromID].m_Level;
	Account.m_Points = m_Accounts[FromID].m_BlockPoints;
	Account.m_Money = m_Accounts[FromID].m_Money;
	Account.m_KillStreak = m_Accounts[FromID].m_KillingSpreeRecord;
	str_copy(Account.m_aUsername, m_Accounts[FromID].m_aLastPlayerName, sizeof(Account.m_aUsername));
	str_copy(Account.m_aAccountName, m_Accounts[FromID].m_Username, sizeof(Account.m_aAccountName));
	m_TopAccounts.push_back(Account);
}

int CGameContext::AddAccount()
{
	AccountInfo Account;
	Account.m_Port = Config()->m_SvPort;
	Account.m_LoggedIn = false;
	Account.m_Disabled = false;
	Account.m_Password[0] = '\0';
	Account.m_Username[0] = '\0';
	Account.m_ClientID = -1;
	Account.m_Level = 0;
	Account.m_XP = 0;
	Account.m_Money = 0;
	Account.m_Kills = 0;
	Account.m_Deaths = 0;
	Account.m_PoliceLevel = 0;
	Account.m_SurvivalKills = 0;
	Account.m_SurvivalWins = 0;
	Account.m_aLastMoneyTransaction[0][0] = '\0';
	Account.m_aLastMoneyTransaction[1][0] = '\0';
	Account.m_aLastMoneyTransaction[2][0] = '\0';
	Account.m_aLastMoneyTransaction[3][0] = '\0';
	Account.m_aLastMoneyTransaction[4][0] = '\0';
	Account.m_SpookyGhost = false;
	Account.m_VIP = false;
	Account.m_BlockPoints = 0;
	Account.m_InstagibKills = 0;
	Account.m_InstagibWins = 0;
	Account.m_SpawnWeapon[0] = 0;
	Account.m_SpawnWeapon[1] = 0;
	Account.m_SpawnWeapon[2] = 0;
	Account.m_Ninjajetpack = false;
	Account.m_aLastPlayerName[0] = '\0';
	Account.m_SurvivalDeaths = 0;
	Account.m_InstagibDeaths = 0;
	Account.m_TaserLevel = 0;
	Account.m_KillingSpreeRecord = 0;

	m_Accounts.push_back(Account);
	return m_Accounts.size()-1;
}

void CGameContext::ReadAccountStats(int ID, const char *pName)
{
	std::string data;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s/%s.acc", Config()->m_SvAccFilePath, pName);
	std::fstream AccFile(aBuf);

	for (int i = 0; i < NUM_ACCOUNT_VARIABLES; i++)
	{
		getline(AccFile, data);
		const char *pData = data.c_str();

		switch (i)
		{
		case ACC_PORT:						m_Accounts[ID].m_Port = atoi(pData); break;
		case ACC_LOGGED_IN:					m_Accounts[ID].m_LoggedIn = atoi(pData); break;
		case ACC_DISABLED:					m_Accounts[ID].m_Disabled = atoi(pData); break;
		case ACC_PASSWORD:					str_copy(m_Accounts[ID].m_Password, pData, sizeof(m_Accounts[ID].m_Password)); break;
		case ACC_USERNAME:					str_copy(m_Accounts[ID].m_Username, pData, sizeof(m_Accounts[ID].m_Username)); break;
		case ACC_CLIENT_ID:					m_Accounts[ID].m_ClientID = atoi(pData); break;
		case ACC_LEVEL:						m_Accounts[ID].m_Level = atoi(pData); break;
		case ACC_XP:						m_Accounts[ID].m_XP = atoi(pData); break;
		case ACC_MONEY:						m_Accounts[ID].m_Money = atoi(pData); break;
		case ACC_KILLS:						m_Accounts[ID].m_Kills = atoi(pData); break;
		case ACC_DEATHS:					m_Accounts[ID].m_Deaths = atoi(pData); break;
		case ACC_POLICE_LEVEL:				m_Accounts[ID].m_PoliceLevel = atoi(pData); break;
		case ACC_SURVIVAL_KILLS:			m_Accounts[ID].m_SurvivalKills = atoi(pData); break;
		case ACC_SURVIVAL_WINS:				m_Accounts[ID].m_SurvivalWins = atoi(pData); break;
		case ACC_SPOOKY_GHOST:				m_Accounts[ID].m_SpookyGhost = atoi(pData); break;
		case ACC_LAST_MONEY_TRANSACTION_0:	str_copy(m_Accounts[ID].m_aLastMoneyTransaction[0], pData, sizeof(m_Accounts[ID].m_aLastMoneyTransaction[0])); break;
		case ACC_LAST_MONEY_TRANSACTION_1:	str_copy(m_Accounts[ID].m_aLastMoneyTransaction[1], pData, sizeof(m_Accounts[ID].m_aLastMoneyTransaction[1])); break;
		case ACC_LAST_MONEY_TRANSACTION_2:	str_copy(m_Accounts[ID].m_aLastMoneyTransaction[2], pData, sizeof(m_Accounts[ID].m_aLastMoneyTransaction[2])); break;
		case ACC_LAST_MONEY_TRANSACTION_3:	str_copy(m_Accounts[ID].m_aLastMoneyTransaction[3], pData, sizeof(m_Accounts[ID].m_aLastMoneyTransaction[3])); break;
		case ACC_LAST_MONEY_TRANSACTION_4:	str_copy(m_Accounts[ID].m_aLastMoneyTransaction[4], pData, sizeof(m_Accounts[ID].m_aLastMoneyTransaction[4])); break;
		case ACC_VIP:						m_Accounts[ID].m_VIP = atoi(pData); break;
		case ACC_BLOCK_POINTS:				m_Accounts[ID].m_BlockPoints = atoi(pData); break;
		case ACC_INSTAGIB_KILLS:			m_Accounts[ID].m_InstagibKills = atoi(pData); break;
		case ACC_INSTAGIB_WINS:				m_Accounts[ID].m_InstagibWins = atoi(pData); break;
		case ACC_SPAWN_WEAPON_0:			m_Accounts[ID].m_SpawnWeapon[0] = atoi(pData); break;
		case ACC_SPAWN_WEAPON_1:			m_Accounts[ID].m_SpawnWeapon[1] = atoi(pData); break;
		case ACC_SPAWN_WEAPON_2:			m_Accounts[ID].m_SpawnWeapon[2] = atoi(pData); break;
		case ACC_NINJAJETPACK:				m_Accounts[ID].m_Ninjajetpack = atoi(pData); break;
		case ACC_LAST_PLAYER_NAME:			str_copy(m_Accounts[ID].m_aLastPlayerName, pData, sizeof(m_Accounts[ID].m_aLastPlayerName)); break;
		case ACC_SURVIVAL_DEATHS:			m_Accounts[ID].m_SurvivalDeaths = atoi(pData); break;
		case ACC_INSTAGIB_DEATHS:			m_Accounts[ID].m_InstagibDeaths = atoi(pData); break;
		case ACC_TASER_LEVEL:				m_Accounts[ID].m_TaserLevel = atoi(pData); break;
		case ACC_KILLING_SPREE_RECORD:		m_Accounts[ID].m_KillingSpreeRecord = atoi(pData); break;
		}
	}
}

void CGameContext::WriteAccountStats(int ID)
{
	std::string data;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s/%s.acc", Config()->m_SvAccFilePath, m_Accounts[ID].m_Username);
	std::ofstream AccFile(aBuf);

	if (AccFile.is_open())
	{
		AccFile << Config()->m_SvPort << "\n";
		AccFile << m_Accounts[ID].m_LoggedIn << "\n";
		AccFile << m_Accounts[ID].m_Disabled << "\n";
		AccFile << m_Accounts[ID].m_Password << "\n";
		AccFile << m_Accounts[ID].m_Username << "\n";
		AccFile << m_Accounts[ID].m_ClientID << "\n";
		AccFile << m_Accounts[ID].m_Level << "\n";
		AccFile << m_Accounts[ID].m_XP << "\n";
		AccFile << m_Accounts[ID].m_Money << "\n";
		AccFile << m_Accounts[ID].m_Kills << "\n";
		AccFile << m_Accounts[ID].m_Deaths << "\n";
		AccFile << m_Accounts[ID].m_PoliceLevel << "\n";
		AccFile << m_Accounts[ID].m_SurvivalKills << "\n";
		AccFile << m_Accounts[ID].m_SurvivalWins << "\n";
		AccFile << m_Accounts[ID].m_SpookyGhost << "\n";
		AccFile << m_Accounts[ID].m_aLastMoneyTransaction[0] << "\n";
		AccFile << m_Accounts[ID].m_aLastMoneyTransaction[1] << "\n";
		AccFile << m_Accounts[ID].m_aLastMoneyTransaction[2] << "\n";
		AccFile << m_Accounts[ID].m_aLastMoneyTransaction[3] << "\n";
		AccFile << m_Accounts[ID].m_aLastMoneyTransaction[4] << "\n";
		AccFile << m_Accounts[ID].m_VIP << "\n";
		AccFile << m_Accounts[ID].m_BlockPoints << "\n";
		AccFile << m_Accounts[ID].m_InstagibKills << "\n";
		AccFile << m_Accounts[ID].m_InstagibWins << "\n";
		AccFile << m_Accounts[ID].m_SpawnWeapon[0] << "\n";
		AccFile << m_Accounts[ID].m_SpawnWeapon[1] << "\n";
		AccFile << m_Accounts[ID].m_SpawnWeapon[2] << "\n";
		AccFile << m_Accounts[ID].m_Ninjajetpack << "\n";
		AccFile << m_Accounts[ID].m_aLastPlayerName << "\n";
		AccFile << m_Accounts[ID].m_SurvivalDeaths << "\n";
		AccFile << m_Accounts[ID].m_InstagibDeaths << "\n";
		AccFile << m_Accounts[ID].m_TaserLevel << "\n";
		AccFile << m_Accounts[ID].m_KillingSpreeRecord << "\n";

		dbg_msg("acc", "saved acc '%s'", m_Accounts[ID].m_Username);
	}
	AccFile.close();
}

void CGameContext::Logout(int ID, bool Silent)
{
	if (m_Accounts[ID].m_ClientID >= 0)
	{
		if (m_apPlayers[m_Accounts[ID].m_ClientID])
		{
			m_apPlayers[m_Accounts[ID].m_ClientID]->OnLogout();
			SendChatTarget(m_Accounts[ID].m_ClientID, "Successfully logged out");
		}
	}
	m_Accounts[ID].m_LoggedIn = false;
	m_Accounts[ID].m_ClientID = -1;
	WriteAccountStats(ID);
	FreeAccount(ID, Silent);
}

int CGameContext::GetAccount(const char* pUsername)
{
	bool AccountLoggedIn = false;
	int ID = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i] && m_apPlayers[i]->GetAccID() >= ACC_START && !str_comp(m_Accounts[m_apPlayers[i]->GetAccID()].m_Username, pUsername))
		{
			ID = m_apPlayers[i]->GetAccID();
			AccountLoggedIn = true;
			break;
		}
	}

	if (!AccountLoggedIn)
	{
		ID = AddAccount();
		ReadAccountStats(ID, pUsername);
	}

	if (m_Accounts[ID].m_Username[0] == '\0')
	{
		FreeAccount(ID);
		return 0;
	}

	return ID;
}

void CGameContext::FreeAccount(int ID, bool Silent)
{
	if (!Silent)
		dbg_msg("acc", "freed account %d", ID);
	m_Accounts.erase(m_Accounts.begin() + ID);
}

int CGameContext::GetNextClientID(bool Inverted)
{
	if (!Inverted)
	{
		for (int i = 0; i < Config()->m_SvMaxClients; i++)
			if (((CServer*)Server())->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
				return i;
	}
	else
	{
		for (int i = MAX_CLIENTS-1; i > -1; i--)
			if (((CServer*)Server())->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
				return i;
	}
	return -1;
}

int CGameContext::GetCIDByName(const char *pName)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (m_apPlayers[i] && !str_comp(pName, Server()->ClientName(i)))
			return i;
	return -1;
}

void CGameContext::CreateSoundGlobal(int Sound)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (m_apPlayers[i])
			CreateSound(m_apPlayers[i]->m_ViewPos, Sound, CmaskOne(i));
}

void CGameContext::CreateSound(int Sound, int ClientID)
{
	CreateSound(m_apPlayers[ClientID]->m_ViewPos, Sound, CmaskOne(ClientID));
}

const char *CGameContext::FormatMotd(const char *pMsg)
{
	char aTemp[64];
	char aTemp2[64];
	char aMotd[900];
	static char aRet[900];
	str_copy(aMotd, pMsg, sizeof(aMotd));
	str_copy(aRet, pMsg, sizeof(aRet));
	if (aMotd[0])
	{
		int count = 0;
		int MotdLen = str_length(aMotd) + 1;
		for (int i = 0, s = 0; i < MotdLen; i++)
		{
			s++;
			if ((aMotd[i] == '\\' && aMotd[i + 1] == 'n') || aMotd[i] == '\n')
			{
				count++;
				s = 0;
			}
			if (s == 35)
			{
				count++;
				s = 0;
			}
		}

		for (int i = MotdLen; i > 0; i--)
		{
			if ((aMotd[i - 1] == '\\' && aMotd[i] == 'n') || aMotd[i] == '\n' || count > 20)
			{
				aMotd[i] = '\0';
				aMotd[i - 1] = '\0';
			}
			else
				break;
		}

		if (count > 20)
			count = 20;

		aTemp[0] = 0;
		for (int i = 0; i < 22 - count; i++)
		{
			str_format(aTemp2, sizeof(aTemp2), "%s", aTemp);
			str_format(aTemp, sizeof(aTemp), "%s%s", aTemp2, "\n");
		}
		str_format(aRet, sizeof(aRet), "%s%sF-DDrace is a mod by fokkonaut\nF-DDrace Mod. Ver.: %s", aMotd, aTemp, GAME_VERSION);
	}
	return aRet;
}

const char *CGameContext::FormatExperienceBroadcast(const char *pMsg)
{
	char pTextColor[5] = { '^', Config()->m_SvExpMsgColorText[0], Config()->m_SvExpMsgColorText[1], Config()->m_SvExpMsgColorText[2] };
	char pSymbolColor[5] = { '^', Config()->m_SvExpMsgColorSymbol[0], Config()->m_SvExpMsgColorSymbol[1], Config()->m_SvExpMsgColorSymbol[2] };
	char pValueColor[5] = { '^', Config()->m_SvExpMsgColorValue[0], Config()->m_SvExpMsgColorValue[1], Config()->m_SvExpMsgColorValue[2] };

	const int ColorOffset = 4;
	int s = ColorOffset;

	static char aRet[256];
	str_copy(aRet, pTextColor, sizeof(aRet));

	int BroadcastLen = str_length(pMsg) + 1;
	for (int i = 0; i < BroadcastLen; i++)
	{
		aRet[s] = pMsg[i];
		s++;

		int Found = 0;
		if (pMsg[i+1] == '[' || pMsg[i+1] == ']' || pMsg[i+1] == '/')
			Found = 1;
		else if (pMsg[i] == '[' || pMsg[i] == '/')
			Found = 2;
		else if (pMsg[i] == ']')
			Found = 3;

		if (Found)
		{
			s += ColorOffset;
			const char *pColorCode = Found == 1 ? pSymbolColor : Found == 2 ? pValueColor : pTextColor;
			str_append(aRet, pColorCode, sizeof(aRet));
		}
	}

	return aRet;
}

void CGameContext::ConnectDummy(int Dummymode, vec2 Pos)
{
	int DummyID = GetNextClientID();
	if (DummyID < 0 || DummyID >= MAX_CLIENTS || m_apPlayers[DummyID])
		return;

	CPlayer *pDummy = m_apPlayers[DummyID] = new(DummyID) CPlayer(this, DummyID, false);
	Server()->DummyJoin(DummyID);
	pDummy->m_IsDummy = true;
	pDummy->m_Dummymode = Dummymode;
	pDummy->m_ForceSpawnPos = Pos;
	pDummy->m_Afk = false; // players are marked as afk when they first enter. dummies dont send real inputs, thats why we need to make them non-afk again

	if (pDummy->m_Dummymode == DUMMYMODE_V3_BLOCKER && Collision()->GetRandomTile(TILE_MINIGAME_BLOCK) != vec2(-1, -1))
		pDummy->m_Minigame = MINIGAME_BLOCK;
	else if (pDummy->m_Dummymode == DUMMYMODE_SHOP_DUMMY && Collision()->GetRandomTile(ENTITY_SHOP_DUMMY_SPAWN) != vec2(-1, -1))
		pDummy->m_Minigame = -1;

	pDummy->m_TeeInfos = m_pSkins.GetSkin(SKIN_DUMMY);

	OnClientEnter(DummyID);

	dbg_msg("dummy", "Dummy connected: %d, Dummymode: %d", DummyID, Dummymode);
}

bool CGameContext::IsShopDummy(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_Dummymode == DUMMYMODE_SHOP_DUMMY;
}

void CGameContext::ConsoleIsDummyCallback(int ClientID, bool *IsDummy, void *pUser)
{
	CGameContext* pSelf = (CGameContext*)pUser;
	*IsDummy = pSelf->m_apPlayers[ClientID] && pSelf->m_apPlayers[ClientID]->m_IsDummy;
}

int CGameContext::GetShopDummy()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (IsShopDummy(i))
			return i;
	return -1;
}

void CGameContext::ConnectDefaultDummies()
{
	if (GetShopDummy() == -1 && Collision()->GetRandomTile(TILE_SHOP) != vec2(-1, -1))
		ConnectDummy(DUMMYMODE_SHOP_DUMMY);

	if (!str_comp(Config()->m_SvMap, "ChillBlock5"))
	{
		ConnectDummy(DUMMYMODE_CHILLBOCK5_POLICE);
		ConnectDummy(DUMMYMODE_CHILLBLOCK5_BLOCKER);
		ConnectDummy(DUMMYMODE_CHILLBLOCK5_BLOCKER);
		ConnectDummy(DUMMYMODE_CHILLBLOCK5_RACER);
	}
	else if (!str_comp(Config()->m_SvMap, "BlmapChill"))
	{
		ConnectDummy(DUMMYMODE_BLMAPCHILL_POLICE);
	}
	else if (!str_comp(Config()->m_SvMap, "blmapV3RoyalX"))
	{
		ConnectDummy(DUMMYMODE_V3_BLOCKER);
	}

	if (Collision()->GetRandomTile(TILE_MINIGAME_BLOCK) != vec2(-1, -1))
		ConnectDummy(DUMMYMODE_V3_BLOCKER);
}

void CGameContext::SetV3Offset(int X, int Y)
{
	if (!str_comp(Config()->m_SvMap, "ChillBlock5"))
	{
		X = 374;
		Y = 59;
	}
	else if (!str_comp(Config()->m_SvMap, "blmapV3RoyalX"))
	{
		X = 97;
		Y = 19;
	}
	else if (!str_comp(Config()->m_SvMap, "BlmapChill"))
	{
		X = 696;
		Y = 617;
	}
	Config()->m_V3OffsetX = X;
	Config()->m_V3OffsetY = Y;
}

void CGameContext::CreateLaserText(vec2 Pos, int Owner, const char *pText)
{
	Pos.y -= 40.0 * 2.5;
	new CLaserText(&m_World, Pos, Owner, Server()->TickSpeed() * 3, pText, (int)(strlen(pText)));
}

void CGameContext::UpdateHidePlayers(int UpdateID)
{
	if (UpdateID == -1)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
			UpdateHidePlayers(i);
		return;
	}

	if (!m_apPlayers[UpdateID])
		return;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (i == UpdateID || !m_apPlayers[i] || m_apPlayers[UpdateID]->GetTeam() == TEAM_SPECTATORS || m_apPlayers[i]->m_IsDummy)
			continue;

		int Team = TEAM_RED;

		if ((Config()->m_SvHideDummies && m_apPlayers[UpdateID]->m_IsDummy)
			|| (Config()->m_SvHideMinigamePlayers && m_apPlayers[UpdateID]->m_Minigame != m_apPlayers[i]->m_Minigame))
			Team = TEAM_BLUE;

		// only update the team when its not the same as before
		if (m_apPlayers[i]->m_HidePlayerTeam[UpdateID] == Team)
			continue;

		m_apPlayers[i]->m_HidePlayerTeam[UpdateID] = Team;

		SendTeamChange(UpdateID, Team, true, Server()->Tick(), i);
	}
}

void CGameContext::UnsetTelekinesis(CEntity *pEntity)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChr = GetPlayerChar(i);
		if (pChr && pChr->m_TelekinesisEntity == pEntity)
		{
			pChr->m_TelekinesisEntity = 0;
			break; // can break here, every entity can only be picked by one player using telekinesis at the time
		}
	}
}

void CGameContext::SendMotd(const char *pMsg, int ClientID)
{
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = pMsg;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendTeamChange(int ClientID, int Team, bool Silent, int CooldownTick, int ToClientID)
{
	CNetMsg_Sv_Team Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Team = Team;
	Msg.m_Silent = (int)Silent;
	Msg.m_CooldownTick = CooldownTick;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ToClientID);
}

const char *CGameContext::GetWeaponName(int Weapon)
{
	switch (Weapon)
	{
	case -2:
		return "Heart";
	case -1:
		return "Armor";
	case WEAPON_HAMMER:
		return "Hammer";
	case WEAPON_GUN:
		return "Gun";
	case WEAPON_SHOTGUN:
		return "Shotgun";
	case WEAPON_GRENADE:
		return "Grenade";
	case WEAPON_LASER:
		return "Rifle";
	case WEAPON_NINJA:
		return "Ninja";
	case WEAPON_TASER:
		return "Taser";
	case WEAPON_HEART_GUN:
		return "Heart Gun";
	case WEAPON_PLASMA_RIFLE:
		return "Plasma Rifle";
	case WEAPON_STRAIGHT_GRENADE:
		return "Straight Grenade";
	case WEAPON_TELEKINESIS:
		return "Telekinesis";
	case WEAPON_LIGHTSABER:
		return "Lightsaber";
	case WEAPON_TELE_RIFLE:
		return "Tele Rifle";
	case WEAPON_PROJECTILE_RIFLE:
		return "Projectile Rifle";
	case WEAPON_BALL_GRENADE:
		return "Ball Greande";
	}
	return "Unknown";
}

int CGameContext::GetRealWeapon(int Weapon)
{
	switch (Weapon)
	{
	case WEAPON_TASER:
		return WEAPON_LASER;
	case WEAPON_HEART_GUN:
		return WEAPON_GUN;
	case WEAPON_PLASMA_RIFLE:
		return WEAPON_LASER;
	case WEAPON_STRAIGHT_GRENADE:
		return WEAPON_GRENADE;
	case WEAPON_TELEKINESIS:
		return WEAPON_NINJA;
	case WEAPON_LIGHTSABER:
		return WEAPON_GUN;
	case WEAPON_TELE_RIFLE:
		return WEAPON_LASER;
	case WEAPON_PROJECTILE_RIFLE:
		return WEAPON_LASER;
	case WEAPON_BALL_GRENADE:
		return WEAPON_GRENADE;
	}
	return Weapon;
}

int CGameContext::GetPickupType(int Type, int Subtype)
{
	if (Type == POWERUP_NINJA)
		return PICKUP_NINJA;
	if (Type != POWERUP_WEAPON)
		return Type;

	Subtype = GetRealWeapon(Subtype);
	switch (Subtype)
	{
	case WEAPON_GUN:
		return PICKUP_GUN;
	case WEAPON_HAMMER:
		return PICKUP_HAMMER;
	case WEAPON_SHOTGUN:
		return PICKUP_SHOTGUN;
	case WEAPON_GRENADE:
		return PICKUP_GRENADE;
	case WEAPON_LASER:
		return PICKUP_LASER;
	case WEAPON_NINJA:
		return PICKUP_NINJA;
	}
	return Subtype;
}

void CGameContext::SendExtraMessage(int Extra, int ToID, bool Set, int FromID, bool Silent, int Special)
{
	if (Silent)
		return;

	char aMsg[128];
	str_copy(aMsg, CreateExtraMessage(Extra, Set, FromID, ToID, Special), sizeof(aMsg));
	SendChatTarget(ToID, aMsg);
	if (FromID >= 0 && FromID != ToID)
		SendChatTarget(FromID, aMsg);
}

const char *CGameContext::CreateExtraMessage(int Extra, bool Set, int FromID, int ToID, int Special)
{
	char aInfinite[16];
	char aItem[64];
	static char aMsg[128];

	// infinite
	if (Set && (Extra == INF_RAINBOW || Extra == INF_METEOR))
		str_format(aInfinite, sizeof(aInfinite), "Infinite ");
	else
		aInfinite[0] = 0;

	// get item name
	char aTemp[64];
	str_copy(aTemp, GetExtraName(Extra, Special), sizeof(aItem));
	str_format(aItem, sizeof(aItem), "%s%s", aInfinite, aTemp);

	// message without a sender
	if (FromID == -1 || FromID == ToID)
	{
		if (Extra == JETPACK || Extra == ATOM || Extra == TRAIL || Extra == METEOR || Extra == INF_METEOR || Extra == SCROLL_NINJA || Extra == HOOK_POWER || Extra == SPREAD_WEAPON || Extra == FREEZE_HAMMER || Extra == ITEM || Extra == TELE_WEAPON || Extra == DOOR_HAMMER)
			str_format(aMsg, sizeof(aMsg), "You %s %s", Set ? "have a" : "lost your", aItem);
		else if (Extra == VANILLA_MODE || Extra == DDRACE_MODE)
			str_format(aMsg, sizeof(aMsg), "You are now in %s", aItem);
		else if (Extra == PASSIVE)
			str_format(aMsg, sizeof(aMsg), "You are %s in %s", Set ? "now" : "no longer", aItem);
		else if (Extra == ENDLESS_HOOK)
			str_format(aMsg, sizeof(aMsg), "%s has been %s", aItem, Set ? "activated" : "deactivated");
		else if (Extra == INFINITE_JUMPS)
			str_format(aMsg, sizeof(aMsg), "You %shave %s", Set ? "" : "don't ", aItem);
		else if (Extra == TEE_CONTROL)
			str_format(aMsg, sizeof(aMsg), "You are %s permitted to use the tee controller", Set ? "now" : "no longer");
		else
			str_format(aMsg, sizeof(aMsg), "You %s %s", Set ? "have" : "lost", aItem);
	}

	// message with a sender
	else if (FromID >= 0)
		str_format(aMsg, sizeof(aMsg), "%s was %s '%s' by '%s'", aItem, Set ? "given to" : "removed from", Server()->ClientName(ToID), Server()->ClientName(FromID));

	return aMsg;
}

const char *CGameContext::GetExtraName(int Extra, int Special)
{
	switch (Extra)
	{
	case HOOK_NORMAL:
		return "Normal";
	case JETPACK:
		return "Jetpack Gun";
	case RAINBOW:
		return "Rainbow";
	case INF_RAINBOW:
		return "Rainbow";
	case ATOM:
		return "Atom";
	case TRAIL:
		return "Trail";
	case SPOOKY_GHOST:
		return "Spooky Ghost";
	case SPOOKY:
		return "Spooky Mode";
	case METEOR:
		return "Meteor";
	case INF_METEOR:
		return "Meteor";
	case PASSIVE:
		return "Passive Mode";
	case VANILLA_MODE:
		return "Vanilla Mode";
	case DDRACE_MODE:
		return "DDrace Mode";
	case BLOODY:
		return "Bloody";
	case STRONG_BLOODY:
		return "Strong Bloody";
	case SCROLL_NINJA:
		return "Scroll Ninja";
	case HOOK_POWER:
		{
			static char aPower[64];
			str_format(aPower, sizeof(aPower), "%s Hook", GetExtraName(Special));
			return aPower;
		}
	case ENDLESS_HOOK:
		return "Endless Hook";
	case INFINITE_JUMPS:
		return "Unlimited Air Jumps";
	case SPREAD_WEAPON:
		{
			static char aWeapon[64];
			str_format(aWeapon, sizeof(aWeapon), "Spread %s", GetWeaponName(Special));
			return aWeapon;
		}
	case FREEZE_HAMMER:
		return "Freeze Hammer";
	case INVISIBLE:
		return "Invisibility";
	case ITEM:
		{
			static char aItem[64];
			str_format(aItem, sizeof(aItem), "%s%sItem", Special > -3 ? GetWeaponName(Special) : "", Special > -3 ? " " : "");
			return aItem;
		}
	case TELE_WEAPON:
		{
			static char aWeapon[64];
			str_format(aWeapon, sizeof(aWeapon), "Tele %s", GetWeaponName(Special));
			return aWeapon;
		}
	case ALWAYS_TELE_WEAPON:
		return "Always Tele Weapon";
	case DOOR_HAMMER:
		return "Door Hammer";
	case AIM_CLOSEST:
		return "Aim Closest";
	case SPIN_BOT:
		return "Spin Bot";
	case TEE_CONTROL:
		return "Tee Control";
	}
	return "Unknown";
}

int CGameContext::CountConnectedPlayers(bool CountSpectators, bool ExcludeDummies)
{
	int Count = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (((CServer*)Server())->m_aClients[i].m_State != CServer::CClient::STATE_EMPTY)
		{
			if (m_apPlayers[i])
			{
				if (ExcludeDummies && m_apPlayers[i]->m_IsDummy)
					continue;
				if (!CountSpectators && m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
					continue;
			}
			Count++;
		}
	}
	return Count;
}

bool CGameContext::IsValidHookPower(int HookPower)
{
	return HookPower == HOOK_NORMAL
		|| HookPower == RAINBOW
		|| HookPower == BLOODY
		|| HookPower == ATOM
		|| HookPower == TRAIL;
}

const char *CGameContext::GetMinigameName(int Minigame)
{
	switch (Minigame)
	{
	case MINIGAME_NONE:
		return "None";
	case MINIGAME_BLOCK:
		return "Block";
	case MINIGAME_SURVIVAL:
		return "Survival";
	case MINIGAME_INSTAGIB_BOOMFNG:
		return "Instagib Boom FNG";
	case MINIGAME_INSTAGIB_FNG:
		return "Instagib FNG";
	}
	return "Unknown";
}

const char* CGameContext::GetMinigameCommand(int Minigame)
{
	switch (Minigame)
	{
	case MINIGAME_NONE:
		return "none";
	case MINIGAME_BLOCK:
		return "block";
	case MINIGAME_SURVIVAL:
		return "survival";
	case MINIGAME_INSTAGIB_BOOMFNG:
		return "boomfng";
	case MINIGAME_INSTAGIB_FNG:
		return "fng";
	}
	return "unknown";
}

void CGameContext::SurvivalTick()
{
	// if there are no spawn tiles, we cant play the game
	if (!m_aMinigameDisabled[MINIGAME_SURVIVAL] && (Collision()->GetRandomTile(TILE_SURVIVAL_LOBBY) == vec2(-1, -1) || Collision()->GetRandomTile(TILE_SURVIVAL_SPAWN) == vec2(-1, -1) || Collision()->GetRandomTile(TILE_SURVIVAL_DEATHMATCH) == vec2(-1, -1)))
	{
		m_aMinigameDisabled[MINIGAME_SURVIVAL] = true;
		return;
	}

	// set the mode to lobby, if the game is offline and there are now players
	if (m_SurvivalGameState == SURVIVAL_OFFLINE)
		m_SurvivalGameState = SURVIVAL_LOBBY;

	// check if we dont have any players in the current state
	if (!CountSurvivalPlayers(m_SurvivalGameState))
	{
		m_SurvivalGameState = SURVIVAL_OFFLINE;
		m_SurvivalBackgroundState = SURVIVAL_OFFLINE;
		return;
	}

	// decrease the tick at any time if it exists (its a timer)
	if (m_SurvivalTick)
		m_SurvivalTick--;

	int Remaining = m_SurvivalTick / Server()->TickSpeed();

	// main part
	char aBuf[128];

	if (m_SurvivalGameState > SURVIVAL_LOBBY && CountSurvivalPlayers(m_SurvivalGameState) == 1)
	{
		// if there is only one survival player left, before the time is over, we have a winner
		m_SurvivalWinner = GetRandomSurvivalPlayer(m_SurvivalGameState);

		if (m_apPlayers[m_SurvivalWinner])
		{
			str_format(aBuf, sizeof(aBuf), "The winner is '%s'", Server()->ClientName(m_SurvivalWinner));
			SendSurvivalBroadcast(aBuf);

			// send message to winner
			SendChatTarget(m_SurvivalWinner, "You are the winner");

			// add a win to the winners' accounts
			m_Accounts[m_apPlayers[m_SurvivalWinner]->GetAccID()].m_SurvivalWins++;
			m_apPlayers[m_SurvivalWinner]->GiveXP(250, "win a survival round");
		}

		// sending back to lobby
		m_SurvivalGameState = SURVIVAL_LOBBY;
		m_SurvivalBackgroundState = SURVIVAL_OFFLINE;
		SetPlayerSurvivalState(SURVIVAL_LOBBY);
	}


	// checking for foreground states
	switch (m_SurvivalGameState)
	{
		case SURVIVAL_LOBBY:
		{
			// check whether we have something running in the background
			if (m_SurvivalBackgroundState != SURVIVAL_OFFLINE)
				break;

			// count the lobby players, if they are fewer than the minimum amount, set the waiting mode in the background
			if (CountSurvivalPlayers(SURVIVAL_LOBBY) < Config()->m_SvSurvivalMinPlayers)
			{
				m_SurvivalBackgroundState = BACKGROUND_LOBBY_WAITING;
			}
			// if we are more than the minimum players waiting, the countdown will start in the background (30 seconds until the game starts)
			else
			{
				m_SurvivalBackgroundState = BACKGROUND_LOBBY_COUNTDOWN;
				m_SurvivalTick = Server()->TickSpeed() * (Config()->m_SvSurvivalLobbyCountdown + 1);
			}
			break;
		}

		case SURVIVAL_PLAYING:
		{
			// the game is running
			break;
		}

		case SURVIVAL_DEATHMATCH:
		{
			if (!m_SurvivalTick)
			{
				// if the deathmatch is over, reset the survival game, sending players back to lobby
				if (CountSurvivalPlayers(SURVIVAL_DEATHMATCH) > 1)
					SendSurvivalBroadcast("There is no winner this round!");
				m_SurvivalGameState = SURVIVAL_OFFLINE;
				m_SurvivalBackgroundState = BACKGROUND_IDLE;
				SetPlayerSurvivalState(SURVIVAL_LOBBY);
			}
			else
			{
				// before its over, send some broadcasts until its finally over
				if (Server()->Tick() % 50 == 0)
				{
					if (Remaining % 30 == 0 || Remaining <= 10)
					{
						str_format(aBuf, sizeof(aBuf), "Deathmatch will end in %d seconds", Remaining);
						SendSurvivalBroadcast(aBuf, true);
					}
				}
			}
			break;
		}
	}

	// checking for background states
	switch (m_SurvivalBackgroundState)
	{
		case BACKGROUND_LOBBY_WAITING:
		{
			// send the waiting for players broadcast to all survival players
			if (Server()->Tick() % 50 == 0)
			{
				str_format(aBuf, sizeof(aBuf), "[%d/%d] players to start a round", CountSurvivalPlayers(SURVIVAL_LOBBY), Config()->m_SvSurvivalMinPlayers);
				SendSurvivalBroadcast(aBuf, false, false);
			}
			break;
		}

		case BACKGROUND_LOBBY_COUNTDOWN:
		{
			if (!m_SurvivalTick)
			{
				// timer is over, the round starts
				str_format(aBuf, sizeof(aBuf), "Round started, you have %d minutes to kill each other", Config()->m_SvSurvivalRoundTime);
				SendSurvivalBroadcast(aBuf);

				// set a new tick, this time for the round to end after its up
				m_SurvivalTick = Server()->TickSpeed() * 60 * Config()->m_SvSurvivalRoundTime;
				// set the foreground state
				m_SurvivalGameState = SURVIVAL_PLAYING;
				// change background state
				m_SurvivalBackgroundState = BACKGROUND_DEATHMATCH_COUNTDOWN;
				// set the player's survival state
				SetPlayerSurvivalState(SURVIVAL_PLAYING);
			}
			else if (CountSurvivalPlayers(SURVIVAL_LOBBY) >= Config()->m_SvSurvivalMinPlayers)
			{
				// if we are more than the minimum players, the countdown will start
				if (Server()->Tick() % 50 == 0)
				{
					str_format(aBuf, sizeof(aBuf), "Round will start in %d seconds", Remaining);
					SendSurvivalBroadcast(aBuf, Remaining <= 10, false);
				}
			}
			// if someone left the lobby, the countdown stops and we return to the lobby state (waiting for players again)
			else
			{
				SendSurvivalBroadcast("Start failed, too few players");
				m_SurvivalGameState = SURVIVAL_LOBBY;
				m_SurvivalBackgroundState = SURVIVAL_OFFLINE;
			}
			break;
		}

		case BACKGROUND_DEATHMATCH_COUNTDOWN:
		{
			if (!m_SurvivalTick)
			{
				// deathmatch countdown is over, we will start the deathmatch now
				SendSurvivalBroadcast("Deathmatch started, you have 2 minutes to kill the last survivors");

				//sending to deathmatch arena
				m_SurvivalGameState = SURVIVAL_DEATHMATCH;
				SetPlayerSurvivalState(SURVIVAL_DEATHMATCH);
				m_SurvivalBackgroundState = BACKGROUND_IDLE;

				// deathmatch will be 2 minutes
				m_SurvivalTick = Server()->TickSpeed() * 60 * Config()->m_SvSurvivalDeathmatchTime;
			}
			else
			{
				// printing broadcast until deathmatch starts
				if (Server()->Tick() % 50 == 0)
				{
					if (Remaining % 60 == 0 || Remaining == 30 || Remaining <= 10)
					{
						str_format(aBuf, sizeof(aBuf), "Deathmatch will start in %d %s%s", Remaining > 30 ? Remaining / 60 : Remaining, (Remaining % 60 == 0 && Remaining != 0) ? "minute" : "second", (Remaining == 1 || Remaining == 60) ? "" : "s");
						SendSurvivalBroadcast(aBuf, true);
					}
				}
			}
			break;
		}
	}
}

int CGameContext::CountSurvivalPlayers(int State)
{
	int count = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (m_apPlayers[i] && m_apPlayers[i]->m_Minigame == MINIGAME_SURVIVAL && (m_apPlayers[i]->m_SurvivalState == State || State == -1))
			count++;
	return count;
}

void CGameContext::SetPlayerSurvivalState(int State)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (m_apPlayers[i] && m_apPlayers[i]->m_Minigame == MINIGAME_SURVIVAL)
		{
			// only send playing people to deathmatch
			if (State == SURVIVAL_DEATHMATCH && m_apPlayers[i]->m_SurvivalState != SURVIVAL_PLAYING)
				continue;

			// unset spectator mode and pause
			m_apPlayers[i]->SetPlaying();
			// kill the character
			m_apPlayers[i]->KillCharacter(WEAPON_GAME);
			// set its new survival state
			m_apPlayers[i]->m_SurvivalState = State;
			// hide name in every state except lobby
			m_apPlayers[i]->m_ShowName = State == SURVIVAL_LOBBY;
		}
}

int CGameContext::GetRandomSurvivalPlayer(int State, int NotThis)
{
	std::vector<int> SurvivalPlayers;
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (i != NotThis && m_apPlayers[i] && m_apPlayers[i]->m_Minigame == MINIGAME_SURVIVAL && (m_apPlayers[i]->m_SurvivalState == State || State == -1))
			SurvivalPlayers.push_back(i);
	if (SurvivalPlayers.size())
	{
		int Rand = rand() % SurvivalPlayers.size();
		return SurvivalPlayers[Rand];
	}
	return -1;
}

void CGameContext::SendSurvivalBroadcast(const char *pMsg, bool Sound, bool IsImportant)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i] && m_apPlayers[i]->m_Minigame == MINIGAME_SURVIVAL)
		{
			if (Sound)
				CreateSound(SOUND_HOOK_NOATTACH, i);

			// show money broadcast instead of the wanted one if we are on a money tile
			if (m_apPlayers[i]->GetCharacter() && m_apPlayers[i]->GetCharacter()->m_MoneyTile)
				continue;
			SendBroadcast(pMsg, i, IsImportant);
		}
	}
}

void CGameContext::InstagibTick(int Type)
{
	Type = Type == 0 ? MINIGAME_INSTAGIB_BOOMFNG : MINIGAME_INSTAGIB_FNG;

	// if there are no spawn tiles, we cant play the game
	if (!m_aMinigameDisabled[Type] && Collision()->GetRandomTile(Type == MINIGAME_INSTAGIB_BOOMFNG ? ENTITY_SPAWN_RED : ENTITY_SPAWN_BLUE) == vec2(-1, -1))
	{
		m_aMinigameDisabled[Type] = true;
		return;
	}

	//m_apPlayers[Winner]->GiveXP(250, "win an instagib round");

	// add instagib here
}
