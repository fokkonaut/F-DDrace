/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <base/math.h>

#include <antibot/antibot_data.h>
#include <base/hash_ctxt.h>

#include <engine/shared/config.h>
#include <engine/shared/memheap.h>
#include <engine/shared/datafile.h>
#include <engine/shared/json.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <engine/map.h>

#include <generated/server_data.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/version.h>

#include "entities/character.h"
#include "entities/interactive/money.h"
#include "entities/map/draweditor/speedup.h"
#include "entities/map/draweditor/button.h"
#include "entities/map/draweditor/teleporter.h"
#include "entities/map/draweditor/drawtile.h"
#include "entities/map/playercounter.h"
#include "gamemodes/DDRace.h"
#include "teeinfo.h"
#include "gamecontext.h"
#include "player.h"
#include "houses/shop.h"
#include "houses/bank.h"
#include "houses/tavern.h"

#include "entities/interactive/flag.h"
#include <limits>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <cstdarg>
#include <cstdio>

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

	mem_zero(&m_aLastPlayerInput, sizeof(m_aLastPlayerInput));
	std::fill(std::begin(m_aPlayerHasInput), std::end(m_aPlayerHasInput), false);

	m_pController = 0;
	m_VoteCloseTime = 0;
	m_VoteCancelTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	m_LastMapVote = 0;
	m_LockTeams = 0;
	m_NonEmptySince = 0;

	if(Resetting==NO_RESET)
	{
		m_pVoteOptionHeap = new CHeap();
		m_pScore = 0;
		m_NumMutes = 0;
		m_NumVoteMutes = 0;
		for (int i = 0; i < NUM_HOUSES; i++)
			m_pHouses[i] = 0;
		for (int i = 0; i < NUM_MINIGAMES; i++)
			m_pMinigames[i] = 0;
		m_Accounts.m_NumAccountSystemBans = 0;
	}

	m_ChatResponseTargetID = -1;
	m_TeeHistorianActive = false;

	m_pRandomMapResult = nullptr;
	m_pMapVoteResult = nullptr;
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

	for (int i = 0; i < NUM_HOUSES; i++)
		if (m_pHouses[i])
			delete m_pHouses[i];

	for (int i = 0; i < NUM_MINIGAMES; i++)
		if (m_pMinigames[i])
			delete m_pMinigames[i];
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;
	CVotingMenu VotingMenu = m_VotingMenu;

	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new (this) CGameContext(RESET);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;
	m_VotingMenu = VotingMenu;
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

CTuningParams *CGameContext::TuningFromChrOrZone(int ClientID, int Zone)
{
	if(GetPlayerChar(ClientID))
		return GetPlayerChar(ClientID)->Tuning();
	if(Zone > 0)
		return &TuningList()[Zone];
	return &m_Tuning;
}

int CGameContext::SetLockedTune(LOCKED_TUNES *pLockedTunings, CLockedTune &Tune, bool AllowGlobalValues)
{
	float GlobalValue;
	if(!m_Tuning.Get(Tune.m_aParam, &GlobalValue))
		return 0;

	bool IsGlobalValue = Tune.m_Value.Get() == (int)(GlobalValue * 100.f);
	for(unsigned int i = 0; i < pLockedTunings->size(); i++)
	{
		if(str_comp_nocase(pLockedTunings->at(i).m_aParam, Tune.m_aParam) == 0)
		{
			if(IsGlobalValue)
			{
				pLockedTunings->erase(pLockedTunings->begin() + i);
				return 3;
			}
			pLockedTunings->at(i).m_Value = Tune.m_Value;
			return 2;
		}
	}

	if (IsGlobalValue && !AllowGlobalValues)
		return 0;

	pLockedTunings->push_back(Tune);
	return 1;
}

void CGameContext::ApplyTuneLock(LOCKED_TUNES *pLockedTunings, int TuneLock)
{
	if(TuneLock < 0 || TuneLock >= TuneZone::NUM)
	{
		pLockedTunings->clear();
		return;
	}

	for(unsigned int i = 0; i < LockedTuning()[TuneLock].size(); i++)
		SetLockedTune(pLockedTunings, LockedTuning()[TuneLock][i]);
}

CTuningParams *CGameContext::ApplyLockedTunings(CTuningParams *pTuning, LOCKED_TUNES &LockedTunings)
{
	static CTuningParams Tuning;
	Tuning = *pTuning;
	for(unsigned int i = 0; i < LockedTunings.size(); i++)
		Tuning.Set(LockedTunings[i].m_aParam, (float)LockedTunings[i].m_Value);
	return &Tuning;
}

void CGameContext::SetBotDetected(int ClientID)
{
	if (m_apPlayers[ClientID])
		m_apPlayers[ClientID]->m_BotDetected = true;
}

void CGameContext::OnCountryCodeLookup(int ClientID)
{
	// If player got a save drop or shutdown save and got logged in the acc language got loaded already, or dummy connect aswell
	if (m_apPlayers[ClientID]->GetAccID() >= ACC_START || !Server()->IsMain(ClientID))
		return;
	m_apPlayers[ClientID]->StartVoteQuestion(CPlayer::VOTE_QUESTION_LANGUAGE_SUGGESTION);
}

void CGameContext::FillAntibot(CAntibotRoundData *pData)
{
	if(!pData->m_Map.m_pTiles)
	{
		Collision()->FillAntibot(&pData->m_Map);
	}
	pData->m_Tick = Server()->Tick();
	mem_zero(pData->m_aCharacters, sizeof(pData->m_aCharacters));
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CAntibotCharacterData *pChar = &pData->m_aCharacters[i];
		for(auto &LatestInput : pChar->m_aLatestInputs)
		{
			LatestInput.m_TargetX = -1;
			LatestInput.m_TargetY = -1;
		}
		pChar->m_Alive = false;
		pChar->m_Pause = false;
		pChar->m_Team = -1;

		pChar->m_Pos = vec2(-1, -1);
		pChar->m_Vel = vec2(0, 0);
		pChar->m_Angle = -1;
		pChar->m_HookedPlayer = -1;
		pChar->m_SpawnTick = -1;
		pChar->m_WeaponChangeTick = -1;

		if(m_apPlayers[i] && !m_apPlayers[i]->m_IsDummy)
		{
			str_copy(pChar->m_aName, Server()->ClientName(i), sizeof(pChar->m_aName));
			CCharacter *pGameChar = m_apPlayers[i]->GetCharacter();
			pChar->m_Alive = (bool)pGameChar;
			pChar->m_Pause = m_apPlayers[i]->IsPaused();
			pChar->m_Team = m_apPlayers[i]->GetTeam();
			if(pGameChar)
			{
				pGameChar->FillAntibot(pChar);
			}
		}
	}
}

void CGameContext::CreateDamage(vec2 Pos, int Id, vec2 Source, int HealthAmount, int ArmorAmount, bool Self, Mask128 Mask, int SevendownAmount)
{
	float a = 3 * pi / 2 + -std::atan2(Source.x, Source.y);
	float s = a-pi/3;
	float e = a+pi/3;

	if (SevendownAmount == 0)
		SevendownAmount = HealthAmount+ArmorAmount;
	for(int i = 0; i < SevendownAmount; i++)
	{
		float f = mix(s, e, (i+1)/(float)(SevendownAmount+1));
		int *pEvent = (int*)m_Events.Create(20 + NUM_NETOBJTYPES, 3*4, Mask);
		if(pEvent)
		{
			((int*)pEvent)[0] = (int)Pos.x;
			((int*)pEvent)[1] = (int)Pos.y;
			((int*)pEvent)[2] = (int)(f*256.0f);
		}
	}

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

void CGameContext::CreateHammerHit(vec2 Pos, Mask128 Mask)
{
	// create the event
	CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}


void CGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int ActivatedTeam, Mask128 Mask)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	// deal damage
	CEntity *apEnts[MAX_CLIENTS];
	float Radius = g_pData->m_Explosion.m_Radius;
	float InnerRadius = 48.0f;

	int64 Types = (1ULL<<CGameWorld::ENTTYPE_CHARACTER);
	if (Config()->m_SvInteractiveDrops)
		Types |= (1ULL<<CGameWorld::ENTTYPE_FLAG) | (1ULL<<CGameWorld::ENTTYPE_PICKUP_DROP) | (1ULL<<CGameWorld::ENTTYPE_MONEY) | (1ULL<<CGameWorld::ENTTYPE_GROG) | (1ULL<<CGameWorld::ENTTYPE_HELICOPTER) | (1ULL<<CGameWorld::ENTTYPE_SPIDER);
	int Num = m_World.FindEntitiesTypes(Pos, Radius, (CEntity * *)apEnts, MAX_CLIENTS, Types);
	Mask128 TeamMask = Mask128();
	for (int i = 0; i < Num; i++)
	{
		vec2 Diff = apEnts[i]->GetPos() - Pos;
		float l = length(Diff);

		CCharacter *pChr = 0;
		CAdvancedEntity *pEnt = 0;
		bool IsCharacter = apEnts[i]->GetObjType() == CGameWorld::ENTTYPE_CHARACTER;
		if (IsCharacter)
		{
			pChr = (CCharacter *)apEnts[i];
		}
		else
		{
			pEnt = (CAdvancedEntity *)apEnts[i];
			if (pEnt->GetObjType() == CGameWorld::ENTTYPE_FLAG)
			{
				if (((CFlag *)pEnt)->GetCarrier())
					continue;
			}
			else if (pEnt->GetObjType() == CGameWorld::ENTTYPE_HELICOPTER || pEnt->GetObjType() == CGameWorld::ENTTYPE_SPIDER)
			{
				if (((IVehicle *)pEnt)->IsBuilding())
					continue;

				l -= pEnt->GetProximityRadius();
			}

			pChr = pEnt->GetOwner();
		}

		vec2 ForceDir(0, 1);
		if (l)
			ForceDir = normalize(Diff);
		l = 1 - clamp((l - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);

		int TuneZone = (Owner == -1 || !m_apPlayers[Owner]) ? 0 : m_apPlayers[Owner]->m_TuneZone;
		float Strength = TuningFromChrOrZone(Owner, TuneZone)->m_ExplosionStrength;

		float Dmg = Strength * l;
		if (!(int)Dmg) continue;

		if ((GetPlayerChar(Owner) ? !(GetPlayerChar(Owner)->m_Hit & CCharacter::DISABLE_HIT_GRENADE) : Config()->m_SvHit || NoDamage) || (pChr && Owner == pChr->GetPlayer()->GetCID()))
		{
			if (Owner != -1 && pChr && pChr->IsAlive() && !pChr->CanCollide(Owner)) continue;
			if (Owner == -1 && ActivatedTeam != -1 && pChr && pChr->IsAlive() && pChr->Team() != ActivatedTeam) continue;

			// Explode at most once per team
			int PlayerTeam = pChr ? ((CGameControllerDDRace*)m_pController)->m_Teams.m_Core.Team(pChr->GetPlayer()->GetCID()) : 0;
			if (GetPlayerChar(Owner) ? GetPlayerChar(Owner)->m_Hit & CCharacter::DISABLE_HIT_GRENADE : !Config()->m_SvHit || NoDamage)
			{
				if (!CmaskIsSet(TeamMask, PlayerTeam)) continue;
				TeamMask = CmaskUnset(TeamMask, PlayerTeam);
			}

			vec2 Force = ForceDir * Dmg * 2;
			if (IsCharacter)
			{
				pChr->TakeDamage(Force, ForceDir*-1, (int)Dmg, Owner, Weapon);
			}
			else
			{
				if (pEnt->GetObjType() == CGameWorld::ENTTYPE_FLAG)
					((CFlag *)pEnt)->SetAtStand(false);
				else if (pEnt->GetObjType() == CGameWorld::ENTTYPE_HELICOPTER || pEnt->GetObjType() == CGameWorld::ENTTYPE_SPIDER)
					((IVehicle *)pEnt)->ExplosionDamage(Strength, Pos, Owner);

				vec2 Temp = pEnt->GetVel() + Force;
				pEnt->SetVel(ClampVel(pEnt->GetMoveRestrictions(), Temp));
			}
		}
	}
}

void CGameContext::CreatePlayerSpawn(vec2 Pos, Mask128 Mask)
{
	// create the event
	CNetEvent_Spawn *pEvent = (CNetEvent_Spawn *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDeath(vec2 Pos, int ClientID, Mask128 Mask)
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

void CGameContext::CreateFinishConfetti(vec2 Pos, Mask128 Mask)
{
	// create the event
	CNetEvent_Finish *pEvent = (CNetEvent_Finish *)m_Events.Create(NETEVENTTYPE_FINISH, sizeof(CNetEvent_Finish), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, Mask128 Mask)
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

void CGameContext::SendChatMsg(CNetMsg_Sv_Chat *pMsg, int Flags, int To)
{
	if (Server()->IsSevendown(To) || str_length(pMsg->m_pMessage) < 128)
	{
		Server()->SendPackMsg(pMsg, Flags, To);
		return;
	}

	const char *pText = pMsg->m_pMessage;

	for (int i = 0; i < 2; i++)
	{
		char aTemp[128];
		for (int pos = 0; pos < 128-1; pos++)
		{
			char c = pMsg->m_pMessage[pos+(i*128)-i];
			aTemp[pos] = c;
			if (c == 0)
				break;
		}
		aTemp[128-1] = 0;
		pMsg->m_pMessage = aTemp;
		Server()->SendPackMsg(pMsg, Flags, To);
		pMsg->m_pMessage = pText;
	}
}

void CGameContext::SendChatTarget(int To, const char *pText, int Flags)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = CHAT_ALL;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = pText;
	Msg.m_TargetID = -1;

	if (To == -1)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if ((Server()->IsSevendown(i) && !(Flags&CHAT_SEVENDOWN))
				|| (!Server()->IsSevendown(i) && !(Flags&CHAT_SEVEN)))
				continue;

			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
		}
	}
	else
	{
		if ((Server()->IsSevendown(To) && !(Flags&CHAT_SEVENDOWN))
			|| (!Server()->IsSevendown(To) && !(Flags&CHAT_SEVEN)))
			return;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
	}
}

void CGameContext::SendChatTeam(int Team, const char *pText, CFormatArg *pArgs, int NumArgs)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] && ((CGameControllerDDRace*)m_pController)->m_Teams.m_Core.Team(i) == Team)
		{
			char aBuf[256];
			str_format_args(aBuf, sizeof(aBuf), m_apPlayers[i]->Localize(pText), pArgs, NumArgs);
			SendChatTarget(i, aBuf);
		}
	}
}

void CGameContext::SendModLogMessage(int ClientID, const char *pMsg, bool IsAuth)
{
	if (ClientID < 0 && ClientID != MODLOG_ID_SERVER)
		return;

	char aName[128];
	char aAvatarURL[256];
	if (ClientID >= 0 && ClientID < MAX_CLIENTS)
	{
		str_format(aName, sizeof(aName), "%s (%s)", Server()->ClientName(ClientID), Server()->GetAuthIdent(ClientID));
		str_copy(aAvatarURL, GetAvatarURL(ClientID), sizeof(aAvatarURL));
	}
	else if (ClientID == MODLOG_ID_SERVER)
	{
		str_copy(aName, "[Server]", sizeof(aName));
		str_copy(aAvatarURL, Config()->m_SvWebhookChatAvatarURL, sizeof(aAvatarURL));
	}

	char aMsg[512];
	str_format(aMsg, sizeof(aMsg), "%s on port %d (%s)", pMsg, Config()->m_SvPort, Config()->m_SvMap);
	Server()->SendWebhookMessage(IsAuth ? Config()->m_SvWebhookAuthLogURL : Config()->m_SvWebhookModLogURL, aMsg, aName, FormatURL(aAvatarURL));
}

const char *CGameContext::GetAvatarURL(int ClientID)
{
	static char aAvatarURL[256];
	char aParameters[256];

	if (Config()->m_SvWebhookChatSkinRenderer == 0) // skins.tw
	{
		const char *pSkinName = m_apPlayers[ClientID]->m_TeeInfos.m_Sevendown.m_SkinName[0] ? m_apPlayers[ClientID]->m_TeeInfos.m_Sevendown.m_SkinName : "default";
		str_format(aAvatarURL, sizeof(aAvatarURL), "https://teedata.net/api/skin/render/name/%s", pSkinName);

		if (m_apPlayers[ClientID]->m_TeeInfos.m_Sevendown.m_UseCustomColor)
		{
			str_format(aParameters, sizeof(aParameters), "?bodyColor=%d&footColor=%d&colorFormat=code", m_apPlayers[ClientID]->m_TeeInfos.m_Sevendown.m_ColorBody, m_apPlayers[ClientID]->m_TeeInfos.m_Sevendown.m_ColorFeet);
			str_append(aAvatarURL, aParameters, sizeof(aParameters));
		}
	}
	else if (Config()->m_SvWebhookChatSkinRenderer == 1) // KoG
	{
		str_format(aAvatarURL, sizeof(aAvatarURL), "https://kog.tw/render_tee.php?skin=%s", m_apPlayers[ClientID]->m_TeeInfos.m_Sevendown.m_SkinName);

		if (m_apPlayers[ClientID]->m_TeeInfos.m_Sevendown.m_UseCustomColor)
		{
			str_format(aParameters, sizeof(aParameters), "&body_color=%d&feet_color=%d", m_apPlayers[ClientID]->m_TeeInfos.m_Sevendown.m_ColorBody, m_apPlayers[ClientID]->m_TeeInfos.m_Sevendown.m_ColorFeet);
			str_append(aAvatarURL, aParameters, sizeof(aParameters));
		}
	}

	return aAvatarURL;
}

void CGameContext::SendChatPolice(const char *pMessage)
{
	SendChat(-1, CHAT_POLICE_CHANNEL, -1, pMessage);
}

bool CGameContext::SendChat(int ChatterClientID, int Mode, int To, const char *pText, int SpamProtectionClientID, int Flags, CFormatArg *pArgs, int NumArgs)
{
	if (SpamProtectionClientID >= 0 && SpamProtectionClientID < MAX_CLIENTS)
		if (ProcessSpamProtection(SpamProtectionClientID))
			return false;

	// client id used to check against muted
	int MuteChecked = ChatterClientID;
	bool SlashMe = false;

	char aBuf[512], aText[256];
	if (Mode == CHAT_POLICE_CHANNEL)
	{
		str_format_args(aBuf, sizeof(aBuf), pText, pArgs, NumArgs);
		str_format(aText, sizeof(aText), "[POLICE-CHANNEL] %s", aBuf);
	}
	else
	{
		str_format_args(aText, sizeof(aText), pText, pArgs, NumArgs);
	}

	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
	{
		// Can happen when translating a player message and that player left the server before translator finished...
		if (!m_apPlayers[ChatterClientID])
			return false;

		// dont trigger updating of teams twice. this means people who translate chat will probably receive the color update before their chat msg appeared
		// CHAT_SINGLE and CHAT_SINGLE_TEAM are used for translating aswell, so they would cause that
		if (Mode == CHAT_ALL || Mode == CHAT_ATEVERYONE)
			m_RainbowName.OnChatMessage(ChatterClientID);

		// join local or public chat
		bool Local = Mode == CHAT_TEAM;
		if ((Mode == CHAT_ALL || Mode == CHAT_TEAM) && m_apPlayers[ChatterClientID]->JoinChat(Local))
			Mode = Local ? CHAT_LOCAL : CHAT_ALL;

		if (Mode == CHAT_WHISPER)
			str_format(aBuf, sizeof(aBuf), "%d:%d:%s -> %d:%s: %s", ChatterClientID, Mode, Server()->ClientName(ChatterClientID), To, Server()->ClientName(To), aText);
		else
			str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Mode, Server()->ClientName(ChatterClientID), aText);
	}
	else if (ChatterClientID == -2)
	{
		str_format(aBuf, sizeof(aBuf), "### %s", aText);
		str_copy(aText, aBuf, sizeof(aText));
		ChatterClientID = -1;
		// if '/me' is used, still dont send the message when sender is muted
		MuteChecked = SpamProtectionClientID;
		SlashMe = true;
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "*** %s", aText);
	}

	const char *pModeStr;
	if (Mode == CHAT_WHISPER)
		pModeStr = Config()->m_SvWhisperLog ? "whisper" : 0;
	else if(Mode == CHAT_SINGLE)
		pModeStr = 0;
	else if(Mode == CHAT_TEAM)
		pModeStr = "teamchat";
	else if(Mode == CHAT_POLICE_CHANNEL)
		pModeStr = "police";
	else
		pModeStr = "chat";

	if(pModeStr)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, pModeStr, aBuf);
	}

	if (!(Flags&CHAT_NO_WEBHOOK) && (Mode == CHAT_ALL || Mode == CHAT_ATEVERYONE || Mode == CHAT_POLICE_CHANNEL ||
		(Mode == CHAT_TEAM && ChatterClientID >= 0 && GetDDRaceTeam(ChatterClientID) == 0)))
	{
		char aWebhookName[32];
		char aAvatarURL[256];
		str_copy(aWebhookName, "[Server]", sizeof(aWebhookName));
		str_copy(aAvatarURL, Config()->m_SvWebhookChatAvatarURL, sizeof(aAvatarURL));

		if (ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		{
			str_format(aWebhookName, sizeof(aWebhookName), "%s [%d]", Server()->ClientName(ChatterClientID), m_Accounts.Get(m_apPlayers[ChatterClientID]->GetAccID()).m_Level);
			if (Config()->m_SvWebhookChatSkinAvatars)
				str_copy(aAvatarURL, GetAvatarURL(ChatterClientID), sizeof(aAvatarURL));
		}

		Server()->SendWebhookMessage(Config()->m_SvWebhookChatURL, aText, aWebhookName, FormatURL(aAvatarURL));
	}

	if (Mode == CHAT_ALL || Mode == CHAT_TEAM || Mode == CHAT_LOCAL)
		Server()->TranslateChat(ChatterClientID, aText, Mode);

	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = Mode;
	Msg.m_ClientID = ChatterClientID;
	Msg.m_pMessage = aText;
	Msg.m_TargetID = -1;

	int MsgFlags = 0;
	if (ChatterClientID >= 0 && (!m_apPlayers[ChatterClientID]->m_ShowName || Durak()->InDurakGame(ChatterClientID)))
		MsgFlags |= MSGFLAG_NONAME;

	if(Mode == CHAT_ALL)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (To == -1 || i == To)
				if (!IsMuted(MuteChecked, i) && CanReceiveMessage(ChatterClientID, i) && !str_comp(Server()->GetChatLanguage(i), "none"))
				{
					bool Send = (Server()->IsSevendown(i) && (Flags&CHAT_SEVENDOWN)) || (!Server()->IsSevendown(i) && (Flags&CHAT_SEVEN));
					if (Send)
					{
						if (ChatterClientID == -1 && !SlashMe)
						{
							str_format_args(aText, sizeof(aText), m_apPlayers[i]->Localize(pText), pArgs, NumArgs);
							Msg.m_pMessage = aText;
						}
						SendChatMsg(&Msg, MsgFlags|MSGFLAG_VITAL, i);
					}
				}
	}
	else if(Mode == CHAT_TEAM)
	{
		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MsgFlags|MSGFLAG_VITAL|MSGFLAG_NOSEND, -1);

		CTeamsCore* Teams = &((CGameControllerDDRace*)m_pController)->m_Teams.m_Core;

		// send to the clients
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] != 0)
			{
				if (IsMuted(MuteChecked, i) || !CanReceiveMessage(ChatterClientID, i) || str_comp(Server()->GetChatLanguage(i), "none"))
					continue;

				if(m_apPlayers[ChatterClientID]->GetTeam() == TEAM_SPECTATORS)
				{
					if(m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
					{
						SendChatMsg(&Msg, MsgFlags|MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
					}
				}
				else
				{
					if(Teams->Team(i) == GetDDRaceTeam(ChatterClientID) && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
					{
						SendChatMsg(&Msg, MsgFlags|MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
					}
				}
			}
		}
	}
	else if (Mode == CHAT_SINGLE || Mode == CHAT_SINGLE_TEAM)
	{
		// send to the clients
		Msg.m_Mode = Mode == CHAT_SINGLE_TEAM ? CHAT_TEAM : CHAT_ALL;
		if (!IsMuted(MuteChecked, To))
			SendChatMsg(&Msg, MsgFlags|MSGFLAG_VITAL, To);
	}
	else if (Mode == CHAT_ATEVERYONE)
	{
		// send to the clients
		Msg.m_Mode = CHAT_ALL;

		char aMsg[256];
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (m_apPlayers[i])
			{
				str_format(aMsg, sizeof(aMsg), "%s: %s", Server()->ClientName(i), aText);
				Msg.m_pMessage = aMsg;
				SendChatMsg(&Msg, MsgFlags|MSGFLAG_VITAL, i);
			}
		}
	}
	else if (Mode == CHAT_LOCAL)
	{
		// send to the clients
		Msg.m_Mode = CHAT_TEAM;

		for (int i = 0; i < MAX_CLIENTS; i++)
			if (!IsMuted(MuteChecked, i) && IsLocal(ChatterClientID, i) && !str_comp(Server()->GetChatLanguage(i), "none"))
				SendChatMsg(&Msg, MsgFlags|MSGFLAG_VITAL, i);
	}
	else if (Mode == CHAT_POLICE_CHANNEL)
	{
		Msg.m_Mode = CHAT_ALL;
		for (int i = 0; i < MAX_CLIENTS; i++)
			if (m_apPlayers[i] && m_Accounts.Get(m_apPlayers[i]->GetAccID()).m_PoliceLevel)
			{
				str_format_args(aBuf, sizeof(aBuf), m_apPlayers[i]->Localize(pText), pArgs, NumArgs);
				str_format(aText, sizeof(aText), "[POLICE-CHANNEL] %s", aBuf);
				Msg.m_pMessage = aText;
				SendChatMsg(&Msg, MsgFlags|MSGFLAG_VITAL, i);
			}
	}
	else // Mode == CHAT_WHISPER
	{
		if (To < 0 || To >= MAX_CLIENTS || !Server()->ClientIngame(To))
		{
			char aMsg[32];
			str_format(aMsg, sizeof(aMsg), "Invalid whisper");
			SendChatTarget(ChatterClientID, aMsg);
			return false;
		}

		m_apPlayers[ChatterClientID]->m_LastWhisperTo = To;

		// send to target
		if (!IsMuted(MuteChecked, To))
		{
			Msg.m_Mode = CHAT_WHISPER_RECV;
			Msg.m_TargetID = To;
			SendChatMsg(&Msg, MsgFlags|MSGFLAG_VITAL, To);
		}

		// send to sender
		{
			MsgFlags = 0;
			if (!m_apPlayers[To]->m_ShowName || Durak()->InDurakGame(To))
				MsgFlags |= MSGFLAG_NONAME;

			// reset ids bcs they got translated
			Msg.m_Mode = CHAT_WHISPER_SEND;
			Msg.m_TargetID = To;
			Msg.m_ClientID = ChatterClientID;
			SendChatMsg(&Msg, MsgFlags|MSGFLAG_VITAL, ChatterClientID);
		}
	}
	return true;
}

static void UnescapeNewlines(char *pBuf)
{
	int i, j;
	for(i = 0, j = 0; pBuf[i]; i++, j++)
	{
		if(pBuf[i] == '\\' && pBuf[i + 1] == 'n')
		{
			pBuf[j] = '\n';
			i++;
		}
		else if(i != j)
		{
			pBuf[j] = pBuf[i];
		}
	}
	pBuf[j] = '\0';
}

void CGameContext::SendBroadcast(const char* pText, int ClientID, bool IsImportant, CFormatArg *pArgs, int NumArgs)
{
	if (ClientID == -1)
	{
		dbg_assert(IsImportant, "broadcast messages to all players must be important");
		for (int i = 0; i < MAX_CLIENTS; i++)
			SendBroadcast(pText, i);
		return;
	}

	if (!m_apPlayers[ClientID])
		return;

	if (!IsImportant && m_apPlayers[ClientID]->m_LastBroadcastImportance && m_apPlayers[ClientID]->m_LastBroadcast > Server()->Tick() - Server()->TickSpeed() * 10)
		return;

	CNetMsg_Sv_Broadcast Msg;
	char aBuf[1024];
	str_format_args(aBuf, sizeof(aBuf), m_apPlayers[ClientID]->Localize(pText), pArgs, NumArgs);

	// This is done clientside in 0.7, but DDNet clients only parse aBuf[i] == '\n'
	if (Server()->IsSevendown(ClientID))
	{
		UnescapeNewlines(aBuf);
	}

	Msg.m_pMessage = aBuf;
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
		if (m_apPlayers[ClientID]->m_SpookyGhost)
			Msg.m_Emoticon = EMOTICON_GHOST;
		else if (GetPlayerChar(ClientID) && GetPlayerChar(ClientID)->GetActiveWeapon() == WEAPON_HEART_GUN)
			Msg.m_Emoticon = EMOTICON_HEARTS;
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendWeaponPickup(int ClientID, int Weapon)
{
	// dont send so the client doesnt auto switch the weapon and stops editing
	CCharacter *pChr = GetPlayerChar(ClientID);
	if (!pChr || pChr->m_DrawEditor.Active() || (pChr->GetActiveWeapon() == WEAPON_NINJA && !pChr->m_ScrollNinja))
		return;

	// include ninja, client doesnt auto switch to ninja on pickup, or when we have no weapon at all
	if (Weapon >= NUM_VANILLA_WEAPONS-1 || pChr->GetActiveWeaponUnclamped() == -1)
	{
		pChr->SetQueuedWeapon(Weapon);
	}
	else
	{
		CNetMsg_Sv_WeaponPickup Msg;
		Msg.m_Weapon = Weapon;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
}

void CGameContext::SendServerAlert(const char *pMessage)
{
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(!m_apPlayers[ClientId])
		{
			continue;
		}

		if(GetClientDDNetVersion(ClientId) >= VERSION_DDNET_IMPORTANT_ALERT)
		{
			CNetMsg_Sv_ServerAlert Msg;
			Msg.m_pMessage = pMessage;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientId);
		}
		else
		{
			char aBroadcastText[1024 + 32];
			str_copy(aBroadcastText, "SERVER ALERT\n\n");
			str_append(aBroadcastText, pMessage, sizeof(aBroadcastText));
			SendBroadcast(aBroadcastText, ClientId, true);
		}
	}

	// Record server alert to demos exactly once
	// TODO: Workaround https://github.com/ddnet/ddnet/issues/11144 by using client ID 0,
	//       otherwise the message is recorded multiple times.
	CNetMsg_Sv_ServerAlert Msg;
	Msg.m_pMessage = pMessage;
	Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, 0);
}

void CGameContext::SendModeratorAlert(const char *pMessage, int ToClientId)
{
	dbg_assert(ToClientId >= 0 && ToClientId < MAX_CLIENTS, "SendImportantAlert ToClientId invalid");
	dbg_assert(m_apPlayers[ToClientId] != nullptr, "Client not online");

	if(GetClientDDNetVersion(ToClientId) >= VERSION_DDNET_IMPORTANT_ALERT)
	{
		CNetMsg_Sv_ModeratorAlert Msg;
		Msg.m_pMessage = pMessage;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ToClientId);
	}
	else
	{
		char aBroadcastText[1024 + 32];
		str_copy(aBroadcastText, "MODERATOR ALERT\n\n");
		str_append(aBroadcastText, pMessage, sizeof(aBroadcastText));
		SendBroadcast(aBroadcastText, ToClientId, true);
		char aLogMsg[128];
		str_format(aLogMsg, sizeof(aLogMsg), "Notice: player uses an old client version and may not see moderator alerts: %s (ID %d)", Server()->ClientName(ToClientId), ToClientId);
		Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "moderator_alert", aLogMsg);
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
	Msg.m_PlayerSlots = clamp(Config()->m_SvPlayerSlots, 0, (int)VANILLA_MAX_CLIENTS);
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendSkinChange(CTeeInfo TeeInfos, int ClientID, int TargetID)
{
	CNetMsg_Sv_SkinChange Msg;
	Msg.m_ClientID = ClientID;
	for(int p = 0; p < NUM_SKINPARTS; p++)
	{
		Msg.m_apSkinPartNames[p] = TeeInfos.m_aaSkinPartNames[p];
		Msg.m_aUseCustomColors[p] = TeeInfos.m_aUseCustomColors[p];
		Msg.m_aSkinPartColors[p] = TeeInfos.m_aSkinPartColors[p];
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, TargetID);

	// F-DDrace
	m_apPlayers[ClientID]->m_CurrentInfo.m_TeeInfos = TeeInfos;
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
	if (ClientID == -1)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
			SendChatCommand(pCommand, i);
		return;
	}

	if (Server()->IsSevendown(ClientID))
	{
		CNetMsg_Sv_CommandInfoEx Msg;
		Msg.m_pName = pCommand->m_aName;
		Msg.m_pArgsFormat = pCommand->m_aArgsFormat;
		Msg.m_pHelpText = pCommand->m_aHelpText;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
	else
	{
		CNetMsg_Sv_CommandInfo Msg;
		Msg.m_Name = pCommand->m_aName;
		Msg.m_HelpText = pCommand->m_aHelpText;
		Msg.m_ArgsFormat = pCommand->m_aArgsFormat;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
}

void CGameContext::SendChatCommands(int ClientID)
{
	// F-DDrace
	// Remove the clientside commands for 0.7 (expect w and whisper)
	if (!Server()->IsSevendown(ClientID))
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
void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSevendownDesc)
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
	str_copy(m_aSevendownVoteDescription, pSevendownDesc, sizeof(m_aSevendownVoteDescription));
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

	CMsgPacker MsgSevendown(NETMSGTYPE_SV_VOTESET);
	MsgSevendown.AddInt(0);
	MsgSevendown.AddString(pDescription, -1);
	MsgSevendown.AddString(pReason, -1);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if(Server()->IsSevendown(i))
			Server()->SendMsg(&MsgSevendown, MSGFLAG_VITAL, i);
		else
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}
}

void CGameContext::SendVoteSet(int Type, int ClientID)
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

	CMsgPacker MsgSevendown(NETMSGTYPE_SV_VOTESET);
	if(m_VoteCloseTime)
	{
		MsgSevendown.AddInt((m_VoteCloseTime-time_get())/time_freq());
		MsgSevendown.AddString(m_aSevendownVoteDescription, -1);
		MsgSevendown.AddString(m_aVoteReason, -1);
	}
	else
	{
		MsgSevendown.AddInt(0);
		MsgSevendown.AddString("", -1);
		MsgSevendown.AddString("", -1);
	}

	if (ClientID == -1)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (!m_apPlayers[i])
				continue;

			if (!Server()->IsSevendown(i))
			{
				m_PlayerMapping.ForceInsertPlayer(m_VoteCreator, i); // 0.7 clients need the client id in order to show the vote and the name of the caller, so its important that we get him in
				int id = m_VoteCreator;
				Server()->Translate(id, i);
				Msg.m_ClientID = id;
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
				Msg.m_ClientID = m_VoteCreator;
			}
			else
			{
				Server()->SendMsg(&MsgSevendown, MSGFLAG_VITAL, i);
			}
		}
	}
	else
	{
		if (!Server()->IsSevendown(ClientID))
		{
			m_PlayerMapping.ForceInsertPlayer(m_VoteCreator, ClientID);
			int id = m_VoteCreator;
			Server()->Translate(id, ClientID);
			Msg.m_ClientID = id;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
			Msg.m_ClientID = m_VoteCreator;
		}
		else
		{
			Server()->SendMsg(&MsgSevendown, MSGFLAG_VITAL, ClientID);
		}
	}
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

	// F-DDrace
	static CTuningParams Tunings;
	CTuningParams *pTunings = Zone > 0 ? &TuningList()[Zone] : Tuning();
	CCharacter *pChr = GetPlayerChar(ClientID);
	if (pChr)
		Tunings = *ApplyLockedTunings(pTunings, pChr->m_LockedTunings);
	else
		Tunings = *pTunings;

	// set projectile tunings to normal ones, if they are different in zones for example its handled in CProjectile
	Tunings.m_GrenadeCurvature = m_Tuning.m_GrenadeCurvature;
	Tunings.m_GrenadeSpeed = m_Tuning.m_GrenadeSpeed;
	Tunings.m_ShotgunCurvature = m_Tuning.m_ShotgunCurvature;
	Tunings.m_ShotgunSpeed = m_Tuning.m_ShotgunSpeed;
	Tunings.m_GunCurvature = m_Tuning.m_GunCurvature;
	Tunings.m_GunSpeed = m_Tuning.m_GunSpeed;

	if (pChr)
	{
		if (pChr->m_FakeTuneCollision || pChr->m_InSnake || (pChr->m_Passive && !pChr->m_Super))
			Tunings.m_PlayerCollision = 0.f;
		if ((pChr->m_Passive && !pChr->m_Super) || pChr->m_Snake.Active())
			Tunings.m_PlayerHooking = 0.f;

		bool IsActivelyPlayingDurak = Durak()->ActivelyPlaying(ClientID);
		if (pChr->m_DrawEditor.Active() || pChr->m_pVehicle || pChr->m_Snake.Active())
			Tunings.m_HookFireSpeed = 0.f;
		if (pChr->m_pVehicle || pChr->m_Snake.Active() || IsActivelyPlayingDurak)
			Tunings.m_HookDragAccel = 0.f;
		if (pChr->m_pVehicle || pChr->m_InSnake || IsActivelyPlayingDurak)
			Tunings.m_HookDragSpeed = 0.f;

		if (pChr->m_MoveRestrictions&CANTMOVE_DOWN_SOLID_DRAWTILE)
		{
			Tunings.m_Gravity = 0.f;
			Tunings.m_AirControlAccel = Tunings.m_GroundControlAccel;
			Tunings.m_AirControlSpeed = Tunings.m_GroundControlSpeed;
			Tunings.m_AirFriction = Tunings.m_GroundFriction;
			Tunings.m_AirJumpImpulse = Tunings.m_GroundJumpImpulse;
		}

		if (pChr->m_DrawEditor.Active() || pChr->m_pVehicle || pChr->m_InSnake || IsActivelyPlayingDurak
			|| (!Server()->IsSevendown(ClientID) && ((pChr->m_FreezeTime && Config()->m_SvFreezePrediction) || pChr->GetPlayer()->m_TeeControllerID != -1)))
		{
			Tunings.m_GroundControlSpeed = 0.f;
			Tunings.m_GroundJumpImpulse = 0.f;
			Tunings.m_GroundControlAccel = 0.f;
			Tunings.m_AirControlSpeed = 0.f;
			Tunings.m_AirJumpImpulse = 0.f;
			Tunings.m_AirControlAccel = 0.f;
		}

		if (pChr->m_MoveRestrictions&CANTMOVE_DOWN_LASERDOOR || pChr->m_pVehicle || pChr->m_InSnake)
			Tunings.m_Gravity = 0.f;

		if (pChr->m_pVehicle)
			Tunings.m_ExplosionStrength = 0.f;

		// AntiPing
		if (pChr->GetPlayer()->AntiPing())
		{
			const int PreventReloadTimer = 1000000;
			int ActiveWeapon = pChr->GetActiveWeapon();
			int WeaponType = GetWeaponType(ActiveWeapon);
			if (ActiveWeapon != WEAPON_LASER && WeaponType == WEAPON_LASER)
			{
				Tunings.m_LaserReach = 0.f;

				if (ActiveWeapon == WEAPON_TASER)
					Tunings.m_LaserFireDelay = Tunings.m_TaserFireDelay;
				else if (ActiveWeapon == WEAPON_PORTAL_RIFLE)
					Tunings.m_LaserFireDelay = Tunings.m_PortalRifleFireDelay;
				else if (ActiveWeapon == WEAPON_PLASMA_RIFLE)
					Tunings.m_LaserFireDelay = Tunings.m_PlasmaRifleFireDelay;
				else if (ActiveWeapon == WEAPON_PROJECTILE_RIFLE)
					Tunings.m_LaserFireDelay = Tunings.m_ProjectileRifleFireDelay;
				else if (ActiveWeapon == WEAPON_TELE_RIFLE)
					Tunings.m_LaserFireDelay = Tunings.m_TeleRifleFireDelay;
				else if (ActiveWeapon == WEAPON_LIGHTNING_LASER)
					Tunings.m_LaserFireDelay = Tunings.m_LightningLaserFireDelay;
			}
			else if (ActiveWeapon == WEAPON_SHOTGUN && pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA)
				Tunings.m_LaserReach = 0;
			else if (ActiveWeapon == WEAPON_STRAIGHT_GRENADE)
				Tunings.m_GrenadeFireDelay = Tunings.m_StraightGrenadeFireDelay;
			else if (ActiveWeapon == WEAPON_BALL_GRENADE)
				Tunings.m_GrenadeFireDelay = Tunings.m_BallGrenadeFireDelay;
			else if (ActiveWeapon == WEAPON_HEART_GUN)
				Tunings.m_GunFireDelay = Tunings.m_HeartGunFireDelay;
			else if (ActiveWeapon == WEAPON_LIGHTSABER)
				Tunings.m_GunFireDelay = PreventReloadTimer;
			else if (ActiveWeapon == WEAPON_TELEKINESIS || ActiveWeapon == WEAPON_DRAW_EDITOR)
			{
				//Tunings.m_NinjaFireDelay = PreventReloadTimer; // Avoided by not sending CHARACTERFLAG_WEAPON_NINJA at all
				// The way everything works the client might mispredict a grenade or gun proj or a laser even though we are on ninja. prevent that
				//Tunings.m_GunFireDelay = Tunings.m_ShotgunFireDelay = Tunings.m_GrenadeFireDelay = Tunings.m_LaserFireDelay = PreventReloadTimer;
				Tunings.m_LaserReach = 0.f;
			}
		}
	}

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int* pParams = (int*)&Tunings;

	unsigned int last = sizeof(m_Tuning) / sizeof(int);
	for (unsigned i = 0; i < last; i++)
	{
		if (i >= NUM_DDNET_TUNES)
			break;

		if(i == 30 && Server()->IsSevendown(ClientID)) // laser damage
			Msg.AddInt(500); // 5 is default value
		Msg.AddInt(pParams[i]);
	}
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
	m_PlayerMapping.Tick();

	//if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	for (int i = 0; i < NUM_MINIGAMES; i++)
		m_pMinigames[i]->Tick();

	for (int i = 0; i < 2; i++)
		if (!m_aMinigameDisabled[i == 0 ? MINIGAME_INSTAGIB_BOOMFNG : MINIGAME_INSTAGIB_FNG])
			InstagibTick(i);

	m_RainbowName.Tick();
	// has to happen before playerticks, as the wanted players get added there and are resetted after CVotingMenu::Tick
	m_VotingMenu.Tick();
	m_Plots.Tick();
	m_Accounts.Tick();
	m_SavedTees.Tick();

	if (m_LastPlayerCountUpdate + Server()->TickSpeed() * 60 < Server()->Tick())
	{
		SendPlayerCountUpdate();
	}

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
		CPlayer *pPlayer = m_apPlayers[i];
		if(pPlayer)
		{
			// Do it safely here so we dont get any crashes
			if (pPlayer->m_BotDetected)
			{
				const int Action = Config()->m_SvAntibotAutoAction;
				if (Action == 1)
				{
					int Seconds = Config()->m_SvAntibotAutoActionTime;
					if (JailPlayer(i, Seconds, MODLOG_ID_SERVER))
					{
						char aBuf[256];
						SendChatPoliceFormat(Localizable("'%s' has been arrested for using a suspicious client (%d seconds arrest)"), Server()->ClientName(i), Seconds);
						str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You were arrested for %d seconds for using a suspicious client. Try using official DDNet client or disable dummy hammerfly."), Seconds);
						SendChatTarget(i, aBuf);
					}

					// Reset, so we dont loop
					pPlayer->m_BotDetected = false;
				}
				else if (Action == 2)
				{
					char aBuf[64];
					str_format(aBuf, sizeof(aBuf), "Bot detected (%s)", Server()->ClientName(i));
					int Seconds = 60 * Config()->m_SvAntibotAutoActionTime;
					Server()->Ban(i, Seconds, aBuf);
				}

				continue;
			}

			if (Config()->m_SvDnsblJail && Server()->DnsblBlack(i) && !pPlayer->m_ProcessedDnsblJail)
			{
				int Seconds = 60 * Config()->m_SvDnsblJailTime;
				if (JailPlayer(i, Seconds, MODLOG_ID_SERVER))
				{
					char aBuf[256];
					SendChatPoliceFormat(Localizable("'%s' has been arrested for using a VPN (%d seconds arrest)"), Server()->ClientName(i), Seconds);
					str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You were arrested for %d seconds"), Seconds);
					if (Config()->m_SvDnsblBanReason[0])
					{
						char aReason[132];
						str_format(aReason, sizeof(aReason), " (%s)", Config()->m_SvDnsblBanReason);
						str_append(aBuf, aReason, sizeof(aBuf));
					}
					SendChatTarget(i, aBuf);
					pPlayer->m_ProcessedDnsblJail = true;
				}
			}

			// send vote options
			ProgressVoteOptions(i);

			pPlayer->Tick();
			pPlayer->PostTick();

			// F-DDrace
			for (int j = 0; j < NUM_HOUSES; j++)
				m_pHouses[j]->Tick(i);
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
		{
			SendChat(-1, CHAT_ALL, -1, Localizable("Vote aborted"), -1, CHAT_SEVENDOWN);
			EndVote(VOTE_END_ABORT, false);
		}
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

					// don't count votes by blacklisted clients
					if(Config()->m_SvDnsblVote && !m_pServer->DnsblWhite(i))
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

				SendChat(-1, CHAT_ALL, -1, Localizable("Vote passed"), -1, CHAT_SEVENDOWN);
				EndVote(VOTE_END_PASS, m_VoteEnforce==VOTE_ENFORCE_YES);
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || (m_VoteUpdate && No >= (Total+1)/2) || time_get() > m_VoteCloseTime)
			{
				SendChat(-1, CHAT_ALL, -1, Localizable("Vote failed"), -1, CHAT_SEVENDOWN);
				EndVote(VOTE_END_FAIL, m_VoteEnforce==VOTE_ENFORCE_NO);
			}
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
			SendChat(-1, CHAT_ALL, -1, Line, -1, CHATFLAG_ALL|CHAT_NO_WEBHOOK);
	}

	if (Collision()->GetNumAllSwitchers() > 0)
		for (int i = 0; i < Collision()->GetNumAllSwitchers() + 1; ++i)
		{
			for (int j = 0; j < VANILLA_MAX_CLIENTS; ++j)
			{
				// F-DDrace
				// set current switcher client id to -1 if the player doesnt exist OR it is a non-timed switch and the player is not on the switch anymore
				if ((Collision()->m_pSwitchers[i].m_ClientID[j] != -1 && !m_apPlayers[Collision()->m_pSwitchers[i].m_ClientID[j]])
					|| (Collision()->m_pSwitchers[i].m_StartTick[j] < Server()->Tick() && Collision()->m_pSwitchers[i].m_EndTick[j] == 0))
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

		CFormatArg aArgs[3] = { Server()->ClientName(m_pMapVoteResult->m_ClientID), m_pMapVoteResult->m_aMap, "/map" };
		CallVote(m_pMapVoteResult->m_ClientID, m_pMapVoteResult->m_aMap, aCmd, "/map", Localizable("'%s' called vote to change server option '%s' (%s)"), 0, aArgs, 3);

		m_pMapVoteResult = nullptr;
	}


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

void CGameContext::PreInputClients(int ClientId, bool *pClients)
{
	if(!pClients || !m_apPlayers[ClientId])
		return;

	CCharacter *pInputChr = m_apPlayers[ClientId]->GetCharacter();
	if(!pInputChr || m_apPlayers[ClientId]->GetTeam() == TEAM_SPECTATORS || m_apPlayers[ClientId]->m_Afk)
		return;

	// Prevent pre inputs when player cant even move. Avoid annoying mispredictions for most common cases
	// It would be possible to only reset m_Fire for telekinesis for example, but i think it doesnt matter
	if (pInputChr->m_DrawEditor.Active() || pInputChr->m_pVehicle || pInputChr->m_InSnake || pInputChr->GetActiveWeapon() == WEAPON_TELEKINESIS
		|| Arenas()->IsConfiguring(ClientId) || Durak()->ActivelyPlaying(ClientId) || m_apPlayers[ClientId]->m_pControlledTee)
		return;

	for(int Id = 0; Id < MAX_CLIENTS; Id++)
	{
		if(ClientId == Id)
			continue;

		CPlayer *pPlayer = m_apPlayers[Id];
		if(!pPlayer)
			continue;

		if(GetClientDDNetVersion(Id) < VERSION_DDNET_PREINPUT)
			continue;

		if(pPlayer->GetTeam() == TEAM_SPECTATORS || GetDDRaceTeam(ClientId) != GetDDRaceTeam(Id) || pPlayer->m_Afk)
			continue;

		if(!pInputChr->CanSnapCharacter(Id) || pInputChr->NetworkClipped(Id, true))
			continue;

		pClients[Id] = true;
	}
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
	CNetObj_PlayerInput *pApplyInput = static_cast<CNetObj_PlayerInput *>(pInput);

	if(pApplyInput == nullptr)
	{
		// early return if no input at all has been sent by a player
		if(!m_aPlayerHasInput[ClientID])
		{
			return;
		}
		// set to last sent input when no new input has been sent
		pApplyInput = &m_aLastPlayerInput[ClientID];
	}

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
		{
			m_apPlayers[ClientID]->OnPredictedInput(pApplyInput);
		}
	}
}

void CGameContext::OnClientPredictedEarlyInput(int ClientID, void *pInput)
{
	CNetObj_PlayerInput *pApplyInput = static_cast<CNetObj_PlayerInput *>(pInput);

	if(pApplyInput == nullptr)
	{
		// early return if no input at all has been sent by a player
		if(!m_aPlayerHasInput[ClientID])
		{
			return;
		}
		// set to last sent input when no new input has been sent
		pApplyInput = &m_aLastPlayerInput[ClientID];
	}
	else
	{
		// Store input in this function and not in `OnClientPredictedInput`,
		// because this function is called on all inputs, while
		// `OnClientPredictedInput` is only called on the first input of each
		// tick.
		mem_copy(&m_aLastPlayerInput[ClientID], pApplyInput, sizeof(m_aLastPlayerInput[ClientID]));
		m_aPlayerHasInput[ClientID] = true;
	}

	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnPredictedEarlyInput(pApplyInput);
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

void CGameContext::StartResendingVotes(int ClientID, bool ResendVotesPage)
{
	m_VotingMenu.SendPageVotes(ClientID, ResendVotesPage);
}

void CGameContext::ProgressVoteOptions(int ClientID)
{
	CPlayer *pPl = m_apPlayers[ClientID];

	if (pPl->m_SendVoteIndex == -1)
		return;

	if(pPl->m_SendVoteIndex > m_NumVoteOptions)
		return; // shouldn't happen / fail silently

	int VotesLeft = m_NumVoteOptions - pPl->m_SendVoteIndex;
	int NumVotesToSend = minimum(Config()->m_SvVotesPerTick, VotesLeft);

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

	m_VotingMenu.OnProgressVoteOptions(ClientID, &Msg, &CurIndex, &pCurrent);

	while(pCurrent && CurIndex < NumVotesToSend)
	{
		Msg.AddString(pCurrent->m_aDescription, VOTE_DESC_LENGTH);
		pCurrent = pCurrent->m_pNext;
		CurIndex++;
	}

	if (Server()->IsSevendown(ClientID))
	{
		while (CurIndex < CVotingMenu::MAX_VOTES_PER_PACKET)
		{
			Msg.AddString("", VOTE_DESC_LENGTH);
			CurIndex++;
		}
	}

	if(pPl->m_SendVoteIndex == 0)
	{
		CNetMsg_Sv_VoteOptionGroupStart StartMsg;
		Server()->SendPackMsg(&StartMsg, MSGFLAG_VITAL, ClientID);
	}

	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);

	pPl->m_SendVoteIndex += NumVotesToSend;
	if(pPl->m_SendVoteIndex == m_NumVoteOptions)
	{
		CNetMsg_Sv_VoteOptionGroupEnd EndMsg;
		Server()->SendPackMsg(&EndMsg, MSGFLAG_VITAL, ClientID);
	}
}

void CGameContext::OnClientEnter(int ClientID)
{
	// F-DDrace
	str_copy(m_apPlayers[ClientID]->m_CurrentInfo.m_aName, Server()->ClientName(ClientID), sizeof(m_apPlayers[ClientID]->m_CurrentInfo.m_aName));
	str_copy(m_apPlayers[ClientID]->m_CurrentInfo.m_aClan, Server()->ClientClan(ClientID), sizeof(m_apPlayers[ClientID]->m_CurrentInfo.m_aClan));
	m_apPlayers[ClientID]->m_CurrentInfo.m_TeeInfos = m_apPlayers[ClientID]->m_TeeInfos;
	m_apPlayers[ClientID]->Respawn();

	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), m_apPlayers[ClientID]->GetTeam());
		Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
	}

	// load score
	{
		Score()->PlayerData(ClientID)->Reset();
		Score()->LoadScore(ClientID);
		Score()->PlayerData(ClientID)->m_CurrentTime = Score()->PlayerData(ClientID)->m_BestTime;
		m_apPlayers[ClientID]->m_Score = !Score()->PlayerData(ClientID)->m_BestTime ? -1 : Score()->PlayerData(ClientID)->m_BestTime;
	}

	m_VoteUpdate = true;

	if(Server()->DemoRecorder_IsRecording())
	{
		CNetMsg_De_ClientEnter Msg;
		Msg.m_pName = Server()->ClientName(ClientID);
		Msg.m_ClientID = ClientID;
		Msg.m_Team = m_apPlayers[ClientID]->GetTeam();
		Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
	}

	// F-DDrace
	Server()->ExpireServerInfo();

	UpdateHidePlayers();

	if (!Config()->m_SvSilentSpectatorMode || m_apPlayers[ClientID]->GetTeam() != TEAM_SPECTATORS)
	{
		char aBuf[128];
		if (m_apPlayers[ClientID]->GetTeam() == TEAM_RED)
			str_copy(aBuf, Localizable("'%s' entered and joined the game"), sizeof(aBuf));
		else
			str_copy(aBuf, Localizable("'%s' entered and joined the spectators"), sizeof(aBuf));
		int Flags = CHATFLAG_ALL;
		if (m_apPlayers[ClientID]->m_IsDummy)
			Flags |= CHAT_NO_WEBHOOK;

		if (Config()->m_SvJoinMsgDelay && !m_apPlayers[ClientID]->m_IsDummy)
		{
			str_copy(m_apPlayers[ClientID]->m_aDelayedJoinMsg, aBuf, sizeof(m_apPlayers[ClientID]->m_aDelayedJoinMsg));
		}
		else
		{
			SendChatFormat(-1, CHAT_ALL, -1, Flags, aBuf, Server()->ClientName(ClientID));
		}
	}

	m_PlayerMapping.InitPlayerMap(ClientID);

	if (m_apPlayers[ClientID]->m_IsDummy) // dummies dont need these information
		return;

	m_WhoIs.AddEntry(ClientID);

	IServer::CClientInfo Info;
	Server()->GetClientInfo(ClientID, &Info);
	if(Info.m_GotDDNetVersion)
	{
		if (OnClientDDNetVersionKnown(ClientID))
			return; // kicked
	}

	mem_zero(&m_aLastPlayerInput[ClientID], sizeof(m_aLastPlayerInput[ClientID]));
	m_aPlayerHasInput[ClientID] = false;

	SendChatTarget(ClientID, "F-DDrace Mod. Version: " GAME_VERSION ", by fokkonaut");
	SendChatTarget(ClientID, "for more information, please say '/info'");
	if (Config()->m_SvWelcome[0] != 0)
		SendChatTarget(ClientID, Config()->m_SvWelcome);

	m_apPlayers[ClientID]->CheckClanProtection();

	SendStartMessages(ClientID);
	SendPlayerCountUpdate();

	// initial chat delay
	int Seconds = maximum(Config()->m_SvChatInitialDelay, Config()->m_SvJoinMsgDelay);
	if(Seconds != 0 && m_apPlayers[ClientID]->m_JoinTick > m_NonEmptySince + 10 * Server()->TickSpeed() && Server()->GetDummy(ClientID) == -1)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), m_apPlayers[ClientID]->Localize("This server has an initial chat delay, you will need to wait %d seconds before talking."), Seconds);
		SendChatTarget(ClientID, aBuf);
		NETADDR Addr;
		Server()->GetClientAddr(ClientID, &Addr);
		Mute(&Addr, Seconds, Server()->ClientName(ClientID), "Initial chat delay", -1, true);
	}

	int DummyID = Server()->GetDummy(ClientID);
	if (DummyID != -1 && m_apPlayers[DummyID])
	{
		// Always keep track of dummy language
		m_apPlayers[ClientID]->SetLanguage(m_apPlayers[DummyID]->m_Language, true);
	}
}

void CGameContext::SendStartMessages(int ClientID)
{
	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(m_VoteType, ClientID);

	// send settings
	SendSettings(ClientID);
	SendChatCommands(ClientID);

	if (!Server()->IsSevendown(ClientID))
	{
		m_pController->UpdateGameInfo(ClientID);
	}
}

void CGameContext::OnClientRejoin(int ClientID)
{
	SendChatFormat(-1, CHAT_ALL, -1, CHATFLAG_ALL, Localizable("'%s' rejoined current session"), Server()->ClientName(ClientID));

	if (!m_apPlayers[ClientID])
		return;

	// send clear vote options
	CNetMsg_Sv_VoteClearOptions ClearMsg;
	Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);
	// begin sending vote options
	StartResendingVotes(ClientID);

	SendStartMessages(ClientID);
	m_PlayerMapping.InitPlayerMap(ClientID, true);

	int Zone = GetPlayerChar(ClientID) ? GetPlayerChar(ClientID)->m_TuneZone : 0;
	SendTuningParams(ClientID, Zone);
}

void CGameContext::MapDesignChangeDone(int ClientID)
{
	if (!m_apPlayers[ClientID])
		return;

	// send clear vote options
	CNetMsg_Sv_VoteClearOptions ClearMsg;
	Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);
	// begin sending vote options
	StartResendingVotes(ClientID);

	SendStartMessages(ClientID);
	m_PlayerMapping.InitPlayerMap(ClientID, true);

	int Zone = GetPlayerChar(ClientID) ? GetPlayerChar(ClientID)->m_TuneZone : 0;
	SendTuningParams(ClientID, Zone);

	if (Server()->GetDummy(ClientID) != -1)
		SendChatTarget(ClientID, m_apPlayers[ClientID]->Localize("[WARNING] You need to reconnect your dummy after the design change is done, so it can get back it's old state."));
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

	dbg_assert(!m_apPlayers[ClientID], "non-free player slot");

	m_apPlayers[ClientID] = new(ClientID) CPlayer(this, ClientID, Dummy, AsSpec);

	Server()->ExpireServerInfo();

	if (Dummy)
		return;

	// send motd
	SendMotd(FormatMotd(Config()->m_SvMotd), ClientID);
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
	if(Server()->ClientIngame(ClientID) || IsClientBot(ClientID))
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
		Msg.m_Silent = 1;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, -1);

		if (!Config()->m_SvSilentSpectatorMode || m_apPlayers[ClientID]->GetTeam() != TEAM_SPECTATORS)
		{
			bool HasReason = pReason && *pReason;
			if (HasReason || m_apPlayers[ClientID]->m_aDelayedJoinMsg[0] == '\0')
			{
				int Flags = CHATFLAG_ALL;
				if (m_apPlayers[ClientID]->m_IsDummy)
					Flags |= CHAT_NO_WEBHOOK;
				if (HasReason)
					SendChatFormat(-1, CHAT_ALL, -1, CHATFLAG_ALL, Localizable("'%s' has left the game (%s)"), Server()->ClientName(ClientID), pReason);
				else
					SendChatFormat(-1, CHAT_ALL, -1, CHATFLAG_ALL, Localizable("'%s' has left the game"), Server()->ClientName(ClientID));
			}
		}
	}

	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	m_VoteUpdate = true;

	Server()->ExpireServerInfo();

	if (((CServer*)Server())->m_RunServer != CServer::STOPPING)
	{
		SendPlayerCountUpdate();
	}
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

	if (Level == AUTHED_NO)
		return;

	// Unmute on rcon auth, to skip initial chat delay
	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);
	for (int i = 0; i < m_NumMutes; i++)
	{
		if (net_addr_comp(&m_aMutes[i].m_Addr, &Addr, 0) == 0)
		{
			char aBuf[128];
			char aIpBuf[NETADDR_MAXSTRSIZE];
			net_addr_str(&Addr, aIpBuf, sizeof(aIpBuf), false);
			str_format(aBuf, sizeof(aBuf), "Unmuted \"<{%s}>\" (%s) by rcon auth", aIpBuf, Server()->ClientName(ClientID));
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "mutes", aBuf);
			// Unmute
			m_NumMutes--;
			m_aMutes[i] = m_aMutes[m_NumMutes];
			break;
		}
	}
}

const char *CGameContext::GetWhisper(char *pStr, int *pTarget)
{
	char *pName;
	int Error = 0;

	pStr = str_skip_whitespaces(pStr);

	int Victim = -1;

	// add token
	if(*pStr == '"')
	{
		pStr++;

		pName = pStr; // we might have to process escape data
		while(1)
		{
			if(pStr[0] == '"')
				break;
			else if(pStr[0] == '\\')
			{
				if(pStr[1] == '\\')
					pStr++; // skip due to escape
				else if(pStr[1] == '"')
					pStr++; // skip due to escape
			}
			else if(pStr[0] == 0)
				Error = 1;

			pStr++;
		}

		// write null termination
		*pStr = 0;
		pStr++;

		for(Victim = 0; Victim < MAX_CLIENTS; Victim++)
			if (str_comp(pName, Server()->ClientName(Victim)) == 0)
				break;

	}
	else
	{
		pName = pStr;
		while(1)
		{
			bool WasSpace = pStr[0] == ' ';
			if(WasSpace || pStr[0] == 0)
			{
				pStr[0] = 0;
				for(Victim = 0; Victim < MAX_CLIENTS; Victim++)
					if (str_comp(pName, Server()->ClientName(Victim)) == 0)
						break;

				if (WasSpace)
					pStr[0] = ' ';
				else
					break;

				if (Victim < MAX_CLIENTS)
					break;
			}
			pStr++;
		}
	}

	if (Victim >= MAX_CLIENTS)
		Victim = -1;

	*pStr = 0;
	pStr++;

	if (Error)
		Victim = -1;
	*pTarget = Victim;
	return pStr;
}

bool CGameContext::OnClientDDNetVersionKnown(int ClientID)
{
	IServer::CClientInfo Info;
	Server()->GetClientInfo(ClientID, &Info);
	int ClientVersion = Info.m_DDNetVersion;
	if (Server()->IsSevendown(ClientID))
		dbg_msg("ddnet", "cid=%d version=%d", ClientID, ClientVersion);
	else
		dbg_msg("ddnet", "cid=%d ddnet_ver=%d version=%x", ClientID, ClientVersion, Server()->GetClientVersion(ClientID));

	//autoban known bot versions
	if(Config()->m_SvBannedVersions[0] != '\0' && IsVersionBanned(ClientVersion))
	{
		Server()->Kick(ClientID, "unsupported client");
		return true;
	}

	m_PlayerMapping.UpdateTeamsState(ClientID);
	if (ClientVersion >= VERSION_DDNET_PLAYERFLAG_SPEC_CAM)
	{
		m_apPlayers[ClientID]->m_ZoomCursor = true;
	}
	return false;
}

void *CGameContext::PreProcessMsg(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	if (Server()->IsSevendown(ClientID))
	{
		CPlayer *pPlayer = m_apPlayers[ClientID];
		static char s_aRawMsg[1024];
		bool ProcessedMsg = true;

		if (MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			if(pPlayer->m_IsReadyToEnter)
				return 0;

			CNetMsg_Cl_StartInfo *pMsg = (CNetMsg_Cl_StartInfo *)s_aRawMsg;

			for (int p = 0; p < NUM_SKINPARTS; p++)
			{
				pMsg->m_apSkinPartNames[p] = "";
				pMsg->m_aUseCustomColors[p] = 0;
				pMsg->m_aSkinPartColors[p] = 0;
			}

			pMsg->m_pName = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			pMsg->m_pClan = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			pMsg->m_Country = pUnpacker->GetInt();

			char aSkinName[24];
			str_copy(aSkinName, pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES), sizeof(aSkinName));
			int UseCustomColor = pUnpacker->GetInt() ? 1 : 0;
			int ColorBody = pUnpacker->GetInt();
			int ColorFeet = pUnpacker->GetInt();

			CTeeInfo Info(aSkinName, UseCustomColor, ColorBody, ColorFeet);
			pPlayer->m_TeeInfos = Info;
		}
		else if (MsgID == NETMSGTYPE_CL_SAY)
		{
			CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)s_aRawMsg;

			pMsg->m_Mode = pUnpacker->GetInt() ? CHAT_TEAM : CHAT_ALL;
			pMsg->m_pMessage = pUnpacker->GetString(CUnpacker::SANITIZE_CC);
			pMsg->m_Target = -1;

			if (pMsg->m_pMessage[0] == '/')
			{
				// manually catch these so we can give the correct output, not "No such command", because these commands are only "hacked in" for 0.6 clients
				if (str_comp_nocase(pMsg->m_pMessage + 1, "w") == 0 || str_comp_nocase(pMsg->m_pMessage + 1, "whisper") == 0)
				{
					SendChatTarget(ClientID, "Invalid arguments... Usage: w s[player name] r[message]");
					return 0;
				}
				else if (str_comp_nocase(pMsg->m_pMessage + 1, "c") == 0 || str_comp_nocase(pMsg->m_pMessage + 1, "converse") == 0)
				{
					SendChatTarget(ClientID, "Invalid arguments... Usage: c r[message]");
					return 0;
				}

				int WhisperOffset = -1;
				int ConverseOffset = -1;

				if (str_comp_nocase_num(pMsg->m_pMessage + 1, "w ", 2) == 0) WhisperOffset = 3;
				if (str_comp_nocase_num(pMsg->m_pMessage + 1, "whisper ", 8) == 0) WhisperOffset = 9;
				if (str_comp_nocase_num(pMsg->m_pMessage + 1, "c ", 2) == 0) ConverseOffset = 3;
				if (str_comp_nocase_num(pMsg->m_pMessage + 1, "converse ", 9) == 0) ConverseOffset = 10;

				if (WhisperOffset != -1)
				{
					static char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + WhisperOffset, 256);
					pMsg->m_Mode = CHAT_WHISPER;
					pMsg->m_pMessage = GetWhisper(aWhisperMsg, &pMsg->m_Target);
					if (pMsg->m_Target == -1)
					{
						SendChatTarget(ClientID, "Invalid whisper");
						return 0;
					}
				}
				else if (ConverseOffset != -1)
				{
					if (pPlayer->m_LastWhisperTo >= 0)
					{
						static char aWhisperMsg[256];
						str_copy(aWhisperMsg, pMsg->m_pMessage + ConverseOffset, 256);
						pMsg->m_pMessage = aWhisperMsg;
						pMsg->m_Target = pPlayer->m_LastWhisperTo;
						pMsg->m_Mode = CHAT_WHISPER;
					}
					else
					{
						SendChatTarget(ClientID, pPlayer->Localize("You do not have an ongoing conversation. Whisper to someone to start one"));
						return 0; // dont process any further
					}
				}
			}
		}
		else if (MsgID == NETMSGTYPE_CL_SETSPECTATORMODE)
		{
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)s_aRawMsg;

			pMsg->m_SpectatorID = clamp(pUnpacker->GetInt(), -1, MAX_CLIENTS - 1);
			pMsg->m_SpecMode = pMsg->m_SpectatorID == -1 ? SPEC_FREEVIEW : SPEC_PLAYER;

			if (m_World.FlagsUsed() && (pMsg->m_SpectatorID == m_PlayerMapping.GetSpecSelectFlag(ClientID, SPEC_FLAGRED) || pMsg->m_SpectatorID == m_PlayerMapping.GetSpecSelectFlag(ClientID, SPEC_FLAGBLUE)))
			{
				pMsg->m_SpecMode = Server()->GetMaxClients(ClientID) - pMsg->m_SpectatorID;
				pMsg->m_SpectatorID = -1;
			}
		}
		else if (MsgID == NETMSGTYPE_CL_SKINCHANGE)
		{
			if(pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo+Server()->TickSpeed()*Config()->m_SvInfoChangeDelay > Server()->Tick())
				return 0;

			CNetMsg_Cl_SkinChange *pMsg = (CNetMsg_Cl_SkinChange *)s_aRawMsg;

			for (int p = 0; p < NUM_SKINPARTS; p++)
			{
				pMsg->m_apSkinPartNames[p] = "";
				pMsg->m_aUseCustomColors[p] = 0;
				pMsg->m_aSkinPartColors[p] = 0;
			}

			const char *pName = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			const char *pClan = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			int Country = pUnpacker->GetInt();

			char aSkinName[24];
			str_copy(aSkinName, pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES), sizeof(aSkinName));
			int UseCustomColor = pUnpacker->GetInt() ? 1 : 0;
			int ColorBody = pUnpacker->GetInt();
			int ColorFeet = pUnpacker->GetInt();

			CTeeInfo Info(aSkinName, UseCustomColor, ColorBody, ColorFeet);
			pPlayer->m_TeeInfos = Info;

			bool UpdateInfo = false;

			// set infos
			if (str_comp(m_apPlayers[ClientID]->m_CurrentInfo.m_aName, Server()->ClientName(ClientID)) == 0) // check that we dont have a name on right now set by an admin
			{
				if(Server()->WouldClientNameChange(ClientID, pName) && !ProcessSpamProtection(ClientID))
				{
					char aOldName[MAX_NAME_LENGTH];
					str_copy(aOldName, Server()->ClientName(ClientID), sizeof(aOldName));
					Server()->SetClientName(ClientID, pName);

					SendChatFormat(-1, CHAT_ALL, -1, CHATFLAG_ALL, Localizable("'%s' changed name to '%s'"), aOldName, Server()->ClientName(ClientID));
					pPlayer->SetName(Server()->ClientName(ClientID));

					// reload scores
					{
						Score()->PlayerData(ClientID)->Reset();
						Score()->LoadScore(ClientID);
						Score()->PlayerData(ClientID)->m_CurrentTime = Score()->PlayerData(ClientID)->m_BestTime;
						m_apPlayers[ClientID]->m_Score = !Score()->PlayerData(ClientID)->m_BestTime ? -1 : Score()->PlayerData(ClientID)->m_BestTime;
					}

					UpdateInfo = true;

					// update whois on namechange
					m_WhoIs.AddEntry(ClientID);
				}
			}

			if(str_comp(Server()->ClientClan(ClientID), pClan)
				&& str_comp(m_apPlayers[ClientID]->m_CurrentInfo.m_aClan, Server()->ClientClan(ClientID)) == 0) // check that we dont have a clan on right now set by an admin)
			{
				Server()->SetClientClan(ClientID, str_find_nocase(pClan, "Zombie") ? "Human" : pClan);
				pPlayer->SetClan(Server()->ClientClan(ClientID));
				UpdateInfo = true;
			}

			if(Server()->ClientCountry(ClientID) != Country)
			{
				Server()->SetClientCountry(ClientID, Country);
				UpdateInfo = true;
			}

			if (UpdateInfo)
				pPlayer->UpdateInformation();
		}
		else if (MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)s_aRawMsg;
			pMsg->m_Type = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			pMsg->m_Value = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			pMsg->m_Reason = pUnpacker->GetString(CUnpacker::SANITIZE_CC|CUnpacker::SKIP_START_WHITESPACES);
			pMsg->m_Force = 0;
		}
		else
		{
			ProcessedMsg = false;
		}

		if (ProcessedMsg)
		{
			if (pUnpacker->Error())
				return 0;
			return s_aRawMsg;
		}
	}

	return m_NetObjHandler.SecureUnpackMsg(MsgID, pUnpacker);
}

void CGameContext::OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID)
{
	void *pRawMsg = PreProcessMsg(MsgID, pUnpacker, ClientID);
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(m_TeeHistorianActive)
	{
		if(m_NetObjHandler.TeeHistorianRecordMsg(MsgID))
		{
			m_TeeHistorian.RecordPlayerMessage(ClientID, pUnpacker->CompleteData(), pUnpacker->CompleteSize());
		}
	}

	if (!pRawMsg)
		return;

	if(Server()->ClientIngame(ClientID))
	{
		if(MsgID == NETMSGTYPE_CL_SAY)
		{
			CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;

			// trim right and set maximum length to 256 utf8-characters
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

				if(++Length >= 255)
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

			bool Command = pMsg->m_pMessage[0] == '/';
			if (!Command)
			{
				if (Durak()->TryEnterBetStake(ClientID, pMsg->m_pMessage) ||
					(pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_DrawEditor.OnChatMessage(pMsg->m_pMessage)))
				return;
			}

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

			if (Mode == CHAT_WHISPER)
			{
				if (!Server()->IsSevendown(ClientID))
					if (!Server()->ReverseTranslate(pMsg->m_Target, ClientID))
						return;
			}
			else if (Mode == CHAT_TEAM)
			{
				if (pPlayer->m_LocalChat && GetPlayerChar(ClientID))
					Mode = CHAT_LOCAL;
			}

			if (Mode != CHAT_WHISPER)
			{
				if (Config()->m_SvLolFilter && str_comp_nocase(pMsg->m_pMessage, "lol") == 0)
				{
					pMsg->m_pMessage = "I like turtles.";
				}
				else if (Server()->GetAuthedState(ClientID) >= Config()->m_SvAtEveryoneLevel && str_find_nocase(pMsg->m_pMessage, "@everyone"))
				{
					Mode = CHAT_ATEVERYONE;
				}
				else if (Server()->GetAuthedState(ClientID) < Config()->m_SvChatAdminPingLevel && !Command)
				{
					for (int i = 0; i < MAX_CLIENTS; i++)
					{
						const char *pName = Server()->ClientName(i);
						if (!m_apPlayers[i] || !Server()->GetAuthedState(i) || !LineShouldHighlight(pMsg->m_pMessage, pName))
							continue;

						while(1)
						{
							const char *pHaystack = str_find_nocase(pMsg->m_pMessage, pName);
							if (!pHaystack)
								break;

							for (int j = 0; j < str_length(pName); j++)
							{
								*(const_cast<char *>(pHaystack)) = '*';
								pHaystack++;
							}
						}
					}
				}
			}

			if (Command && Mode != CHAT_WHISPER)
			{
				CPlayer *pPlayer = m_apPlayers[ClientID];
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
				int Len = -1;
				if (str_comp_nocase_num(pMsg->m_pMessage + 1, "login ", 6) == 0) Len = 6;
				if (str_comp_nocase_num(pMsg->m_pMessage + 1, "register ", 9) == 0) Len = 9;
				if (str_comp_nocase_num(pMsg->m_pMessage + 1, "changepassword ", 15) == 0) Len = 15;
				if (Len != -1)
				{
					char aCmd[32];
					str_copy(aCmd, pMsg->m_pMessage, Len + 1);
					str_format(aBuf, sizeof(aBuf), "%d used %s", ClientID, aCmd);
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "%d used %s", ClientID, pMsg->m_pMessage);
				}
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "chat-command", aBuf);

				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				Console()->SetFlagMask(CFGFLAG_SERVER);
				m_ChatResponseTargetID = -1;
				Server()->RestrictRconOutput(-1);
			}
			else if(Mode != CHAT_NONE)
			{
				pPlayer->UpdatePlaytime();

				if (SendChat(ClientID, Mode, pMsg->m_Target, pMsg->m_pMessage, ClientID) && Mode != CHAT_WHISPER)
				{
					char aLocalNames[256] = "";
					for (int i = 0; i < MAX_CLIENTS; i++)
					{
						if (m_apPlayers[i] && LineShouldHighlight(pMsg->m_pMessage, Server()->ClientName(i)) && (!CanReceiveMessage(ClientID, i) || !CanReceiveMessage(i, ClientID)))
						{
							char aBuf[128];
							str_format(aBuf, sizeof(aBuf), "%s%s", aLocalNames[0] ? ", " : "", Server()->ClientName(i));
							str_append(aLocalNames, aBuf, sizeof(aLocalNames));
						}
					}

					if (aLocalNames[0])
					{
						SendChatTarget(ClientID, pPlayer->Localize("Following players can't receive your message because either you or they are in local chat mode:"));
						SendChatTarget(ClientID, aLocalNames);
					}
				}
			}
		}
		else if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;
			if(str_comp_nocase(pMsg->m_Type, "option") != 0 && m_PlayerMapping.DoSeeOthers(ClientID, str_toint(pMsg->m_Value), true))
				return;
			if (m_VotingMenu.OnMessage(ClientID, pMsg))
				return;

			if(pMsg->m_Force)
			{
				int Authed = Server()->GetAuthedState(ClientID);
				char aCmd[1024];
				str_format(aCmd, sizeof(aCmd), "force_vote \"%s\" \"%s\" \"%s\"", pMsg->m_Type, pMsg->m_Value, pMsg->m_Reason);
				Console()->SetAccessLevel(Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : Authed == AUTHED_MOD ? IConsole::ACCESS_LEVEL_MOD : IConsole::ACCESS_LEVEL_HELPER);
				Console()->ExecuteLine(aCmd, ClientID, false);
				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				return;
			}

			if(RateLimitPlayerVote(ClientID) || m_VoteCloseTime)
				return;

			m_apPlayers[ClientID]->UpdatePlaytime();

			m_VoteType = -1;
			std::pair<const char*, std::pair<CFormatArg[4], int> > ChatMsg;
			ChatMsg.first = 0;
			ChatMsg.second.first[0] = 0;
			ChatMsg.second.first[1] = 0;
			ChatMsg.second.first[2] = 0;
			ChatMsg.second.first[3] = 0;
			ChatMsg.second.second = 0;
			char aDesc[VOTE_DESC_LENGTH] = {0};
			char aSevendownDesc[VOTE_DESC_LENGTH] = {0};
			char aCmd[VOTE_CMD_LENGTH] = {0};
			char aReason[VOTE_REASON_LENGTH];
			str_copy(aReason, Localizable("No reason given"), sizeof(aReason));
			if(!str_utf8_check(pMsg->m_Type) || !str_utf8_check(pMsg->m_Reason) || !str_utf8_check(pMsg->m_Value))
			{
				return;
			}
			if(pMsg->m_Reason[0])
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
						if(!Console()->LineIsValid(pOption->m_aCommand))
						{
							SendChatTarget(ClientID, "Invalid option");
							return;
						}
						if((str_find(pOption->m_aCommand, "sv_map ") != 0 || str_find(pOption->m_aCommand, "change_map ") != 0 || str_find(pOption->m_aCommand, "random_map") != 0 || str_find(pOption->m_aCommand, "random_unfinished_map") != 0) && RateLimitPlayerMapVote(ClientID))
						{
							return;
						}

						ChatMsg.first = Localizable("'%s' called vote to change server option '%s' (%s)");
						ChatMsg.second.first[0] = Server()->ClientName(ClientID);
						ChatMsg.second.first[1] = pOption->m_aDescription;
						ChatMsg.second.first[2] = aReason;
						ChatMsg.second.second = 3;

						str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);

						if((str_endswith(pOption->m_aCommand, "random_map") || str_endswith(pOption->m_aCommand, "random_unfinished_map")) && str_length(aReason) == 1 && aReason[0] >= '0' && aReason[0] <= '5')
						{
							int Stars = aReason[0] - '0';
							str_format(aCmd, sizeof(aCmd), "%s %d", pOption->m_aCommand, Stars);
						}
						else
						{
							str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
						}

						m_LastMapVote = time_get();
						break;
					}

					pOption = pOption->m_pNext;
				}

				if(!pOption)
				{
					if(Authed != AUTHED_ADMIN) // allow admins to call any vote they want
					{
						char aChatmsg[256];
						str_format(aChatmsg, sizeof(aChatmsg), pPlayer->Localize("'%s' isn't an option on this server"), pMsg->m_Value);
						SendChatTarget(ClientID, aChatmsg);
						return;
					}
					else
					{
						ChatMsg.first = Localizable("'%s' called vote to change server option '%s'");
						ChatMsg.second.first[0] = Server()->ClientName(ClientID);
						ChatMsg.second.first[1] = pMsg->m_Value;
						ChatMsg.second.first[2] = 0;
						ChatMsg.second.second = 2;
						str_format(aDesc, sizeof(aDesc), "%s", pMsg->m_Value);
						str_format(aCmd, sizeof(aCmd), "%s", pMsg->m_Value);
					}
				}

				m_VoteType = VOTE_START_OP;
			}
			else if(str_comp_nocase(pMsg->m_Type, "kick") == 0)
			{
				int Authed = Server()->GetAuthedState(ClientID);
				if(!Authed && time_get() < m_apPlayers[ClientID]->m_Last_KickVote + (time_freq() * 5))
					return;
				else if(!Authed && time_get() < m_apPlayers[ClientID]->m_Last_KickVote + (time_freq() * Config()->m_SvVoteKickDelay))
				{
					char aChatmsg[256];
					str_format(aChatmsg, sizeof(aChatmsg), pPlayer->Localize("There's a %d second wait time between kick votes for each player please wait %d second(s)"),
						Config()->m_SvVoteKickDelay,
						(int)(((m_apPlayers[ClientID]->m_Last_KickVote + (m_apPlayers[ClientID]->m_Last_KickVote * time_freq())) / time_freq()) - (time_get() / time_freq())));
					SendChatTarget(ClientID, aChatmsg);
					m_apPlayers[ClientID]->m_Last_KickVote = time_get();
					return;
				}
				else if(!Config()->m_SvVoteKick && !Authed) // allow admins to call kick votes even if they are forbidden
				{
					SendChatTarget(ClientID, pPlayer->Localize("Server does not allow voting to kick players"));
					m_apPlayers[ClientID]->m_Last_KickVote = time_get();
					return;
				}

				if(Config()->m_SvVoteKickMin && !GetDDRaceTeam(ClientID))
				{
					char aaAddresses[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
					for(int i = 0; i < MAX_CLIENTS; i++)
					{
						if(m_apPlayers[i])
						{
							Server()->GetClientAddr(i, aaAddresses[i], NETADDR_MAXSTRSIZE);
						}
					}
					int NumPlayers = 0;
					for(int i = 0; i < MAX_CLIENTS; ++i)
					{
						if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS && !GetDDRaceTeam(i))
						{
							NumPlayers++;
							for(int j = 0; j < i; j++)
							{
								if(m_apPlayers[j] && m_apPlayers[j]->GetTeam() != TEAM_SPECTATORS && !GetDDRaceTeam(j))
								{
									if(str_comp(aaAddresses[i], aaAddresses[j]) == 0)
									{
										NumPlayers--;
										break;
									}
								}
							}
						}
					}

					if(NumPlayers < Config()->m_SvVoteKickMin)
					{
						char aChatmsg[128];
						str_format(aChatmsg, sizeof(aChatmsg), pPlayer->Localize("Kick voting requires %d players"), Config()->m_SvVoteKickMin);
						SendChatTarget(ClientID, aChatmsg);
						return;
					}
				}

				int KickID = str_toint(pMsg->m_Value);

				if(!Server()->ReverseTranslate(KickID, ClientID))
				{
					return;
				}
				if(KickID < 0 || KickID >= MAX_CLIENTS || !m_apPlayers[KickID])
				{
					SendChatTarget(ClientID, "Invalid client id to kick");
					return;
				}
				if(KickID == ClientID)
				{
					SendChatTarget(ClientID, pPlayer->Localize("You can't kick yourself"));
					return;
				}
				if (m_apPlayers[KickID]->m_IsDummy)
				{
					SendChatTarget(ClientID, pPlayer->Localize("You can't kick dummies"));
					return;
				}
				int KickedAuthed = Server()->GetAuthedState(KickID);
				if(KickedAuthed > Authed)
				{
					SendChatTarget(ClientID, pPlayer->Localize("You can't kick authorized players"));
					m_apPlayers[ClientID]->m_Last_KickVote = time_get();
					char aBufKick[128];
					str_format(aBufKick, sizeof(aBufKick), m_apPlayers[KickID]->Localize("'%s' called for vote to kick you"), Server()->ClientName(ClientID));
					SendChatTarget(KickID, aBufKick);
					return;
				}

				// Don't allow kicking if a player has no character
				if(!GetPlayerChar(ClientID) || !GetPlayerChar(KickID) || GetDDRaceTeam(ClientID) != GetDDRaceTeam(KickID))
				{
					SendChatTarget(ClientID, pPlayer->Localize("You can kick only your team member"));
					m_apPlayers[ClientID]->m_Last_KickVote = time_get();
					return;
				}

				ChatMsg.first = Localizable("'%s' called for vote to kick '%s' (%s)");
				ChatMsg.second.first[0] = Server()->ClientName(ClientID);
				ChatMsg.second.first[1] = Server()->ClientName(KickID);
				ChatMsg.second.first[2] = aReason;
				ChatMsg.second.second = 3;
				str_format(aDesc, sizeof(aDesc), "%2d: %s", KickID, Server()->ClientName(KickID));
				if(!GetDDRaceTeam(ClientID))
				{
					if(!Config()->m_SvVoteKickBantime)
					{
						str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickID);
						str_format(aSevendownDesc, sizeof(aSevendownDesc), "Kick '%s'", Server()->ClientName(KickID));
					}
					else
					{
						char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
						Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
						str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, Config()->m_SvVoteKickBantime);
						str_format(aSevendownDesc, sizeof(aSevendownDesc), "Ban '%s'", Server()->ClientName(KickID));
					}
				}
				else
				{
					str_format(aCmd, sizeof(aCmd), "uninvite %d %d; set_team_ddr %d 0", KickID, GetDDRaceTeam(KickID), KickID);
					str_format(aSevendownDesc, sizeof(aSevendownDesc), "Move '%s' to team 0", Server()->ClientName(KickID));
				}
				m_apPlayers[ClientID]->m_Last_KickVote = time_get();
				m_VoteType = VOTE_START_KICK;
				m_VoteClientID = KickID;
			}
			else if(str_comp_nocase(pMsg->m_Type, "spectate") == 0)
			{
				if(!Config()->m_SvVoteSpectate)
				{
					SendChatTarget(ClientID, pPlayer->Localize("Server does not allow voting to move players to spectators"));
					return;
				}

				int SpectateID = str_toint(pMsg->m_Value);

				if(!Server()->ReverseTranslate(SpectateID, ClientID))
				{
					return;
				}
				if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] || m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
				{
					SendChatTarget(ClientID, "Invalid client id to move");
					return;
				}
				if(SpectateID == ClientID)
				{
					SendChatTarget(ClientID, pPlayer->Localize("You can't move yourself"));
					return;
				}
				if (m_apPlayers[SpectateID]->m_IsDummy)
				{
					SendChatTarget(ClientID, pPlayer->Localize("You can't set dummies to spectator"));
					return;
				}

				if(!GetPlayerChar(ClientID) || !GetPlayerChar(SpectateID) || GetDDRaceTeam(ClientID) != GetDDRaceTeam(SpectateID))
				{
					SendChatTarget(ClientID, pPlayer->Localize("You can only move your team member to spectators"));
					return;
				}

				ChatMsg.first = Localizable("'%s' called for vote to kick '%s' (%s)");
				ChatMsg.second.first[0] = Server()->ClientName(ClientID);
				ChatMsg.second.first[1] = Server()->ClientName(SpectateID);
				ChatMsg.second.first[2] = aReason;
				ChatMsg.second.second = 3;
				str_format(aDesc, sizeof(aDesc), "%2d: %s", SpectateID, Server()->ClientName(SpectateID));
				if(Config()->m_SvPauseable && Config()->m_SvVotePause)
				{
					ChatMsg.first = Localizable("'%s' called for vote to pause '%s' for %d seconds (%s)");
					ChatMsg.second.first[0] = Server()->ClientName(ClientID);
					ChatMsg.second.first[1] = Server()->ClientName(SpectateID);
					ChatMsg.second.first[2] = Config()->m_SvVotePauseTime;
					ChatMsg.second.first[3] = aReason;
					ChatMsg.second.second = 4;
					str_format(aSevendownDesc, sizeof(aSevendownDesc), "Pause '%s' (%ds)", Server()->ClientName(SpectateID), Config()->m_SvVotePauseTime);
					str_format(aCmd, sizeof(aCmd), "uninvite %d %d; force_pause %d %d", SpectateID, GetDDRaceTeam(SpectateID), SpectateID, Config()->m_SvVotePauseTime);
				}
				else
				{
					ChatMsg.first = Localizable("'%s' called for vote to move '%s' to spectators (%s)");
					ChatMsg.second.first[0] = Server()->ClientName(ClientID);
					ChatMsg.second.first[1] = Server()->ClientName(SpectateID);
					ChatMsg.second.first[2] = aReason;
					ChatMsg.second.second = 3;
					str_format(aSevendownDesc, sizeof(aSevendownDesc), "Move '%s' to spectators", Server()->ClientName(SpectateID));
					str_format(aCmd, sizeof(aCmd), "uninvite %d %d; set_team %d -1 %d", SpectateID, GetDDRaceTeam(SpectateID), SpectateID, Config()->m_SvVoteSpectateRejoindelay);
				}
				m_VoteType = VOTE_START_SPEC;
				m_VoteClientID = SpectateID;
			}

			if(aCmd[0] && str_comp(aCmd, "info") != 0)
				CallVote(ClientID, aDesc, aCmd, aReason, ChatMsg.first, aSevendownDesc[0] ? aSevendownDesc : 0, ChatMsg.second.first, ChatMsg.second.second);
		}
		else if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			if(Config()->m_SvSpamprotection && pPlayer->m_LastVote && pPlayer->m_LastVote+Server()->TickSpeed()/8 > Server()->Tick())
				return;

			pPlayer->m_LastVote = Server()->Tick();
			CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;

			if (pPlayer->m_VoteQuestionRunning)
			{
				pPlayer->OnEndVoteQuestion(pMsg->m_Vote);
				return;
			}

			if (m_VoteCloseTime)
			{
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

				// Dont process further if a vote is running
				return;
			}

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
					bool InHouse = false;
					for (int i = 0; i < NUM_HOUSES; i++)
					{
						if (m_pHouses[i]->IsInside(ClientID))
						{
							m_pHouses[i]->OnKeyPress(ClientID, pMsg->m_Vote);
							InHouse = true;
						}
					}
					
					if (!InHouse)
					{
						IVehicle* pVehicle = pChr->m_pVehicle;
						if (pVehicle)
						{
							if (pChr->CanSwitchSeats())
							{
								int switchSeat = pVehicle->GetNextAvailableSeat(pChr->m_VehicleSeat);
								if (switchSeat != -1)
								{
									pVehicle->Dismount(ClientID, false);
									pVehicle->Mount(ClientID, switchSeat);
								}
							}
						}
						else
						{
							pChr->DropFlag();
						}
					}
				}
			}
			else if (pMsg->m_Vote == -1) //vote no (f4)
			{
				if (pPlayer->m_TeeControlMode)
				{
					if (pControlledTee)
						pControlledTee->DropWeapon(pControlledTee->GetActiveWeaponUnclamped(), false);
				}
				else if (pChr)
				{
					bool InHouse = false;
					for (int i = 0; i < NUM_HOUSES; i++)
					{
						if (m_pHouses[i]->IsInside(ClientID))
						{
							m_pHouses[i]->OnKeyPress(ClientID, pMsg->m_Vote);
							InHouse = true;
						}
					}

					if (!InHouse)
					{
						if (pChr->m_pVehicle)
						{
							pChr->m_pVehicle->Dismount(ClientID);
						}
						else if (!pChr->DropGrog() && !pChr->TryMountVehicle())
						{
							pChr->DropWeapon(pChr->GetActiveWeaponUnclamped(), false);
						}
					}
				}
			}
		}
		else if(MsgID == NETMSGTYPE_CL_SETTEAM)
		{
			CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

			if (pPlayer->m_HasTeeControl && !pPlayer->IsPaused())
			{
				bool SetTeeControl = pMsg->m_Team == TEAM_SPECTATORS;
				if (!SetTeeControl || (pPlayer->m_TeeControlMode && SetTeeControl))
					pPlayer->UnsetTeeControl();
				if (pPlayer->m_TeeControlMode != SetTeeControl)
				{
					pPlayer->m_TeeControlMode = SetTeeControl;
					SendChatTarget(ClientID, pPlayer->m_TeeControlMode ? pPlayer->Localize("You are now using the tee controller") : pPlayer->Localize("You are no longer using the tee controller"));
					SendTeamChange(ClientID, SetTeeControl ? TEAM_SPECTATORS : pPlayer->GetTeam(), true, Server()->Tick(), ClientID);
					if (pPlayer->GetCharacter() && !Server()->IsSevendown(ClientID))
						SendTuningParams(ClientID, pPlayer->GetCharacter()->m_TuneZone);
					if (pPlayer->m_TeeControlForcedID != -1 && SetTeeControl)
						pPlayer->SetTeeControl(m_apPlayers[pPlayer->m_TeeControlForcedID]);
				}
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
					SendChatTarget(ClientID, pPlayer->Localize("Kill Protection enabled. If you really want to join the spectators, first type /kill"));
					return;
				}
				if (Config()->m_SvWalletKillProtection != 0 && pPlayer->GetWalletMoney() >= Config()->m_SvWalletKillProtection && pChr->m_FreezeTime)
				{
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), pPlayer->Localize("Warning you have %lld money in your wallet! (see /money)"), pPlayer->GetWalletMoney());
					SendChatTarget(ClientID, aBuf);
					SendChatTarget(ClientID, pPlayer->Localize("Wallet kill Protection enabled. If you really want to join the spectators, first type /kill"));
					return;
				}
			}

			pPlayer->m_LastSetTeam = Server()->Tick();

			if (pPlayer->m_EscapeTime)
			{
				SendChatTarget(ClientID, pPlayer->Localize("You can't join the spectators while being searched by the police"));
				return;
			}

			// Switch team on given client and kill/respawn him
			if(m_pController->CanJoinTeam(pMsg->m_Team, ClientID))
			{
				if (pPlayer->IsPaused())
					SendChatTarget(ClientID, pPlayer->Localize("Use /pause first then you can kill"));
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
				SendBroadcastFormat(ClientID, true, Localizable("Only %d active players are allowed"), Config()->m_SvPlayerSlots);
			}
		}
		else if (MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			if(Config()->m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode+Server()->TickSpeed()/4 > Server()->Tick())
				return;

			pPlayer->m_LastSetSpectatorMode = Server()->Tick();
			pPlayer->UpdatePlaytime();

			if(pMsg->m_SpecMode == SPEC_PLAYER)
			{
				if (m_PlayerMapping.DoSeeOthers(ClientID, pMsg->m_SpectatorID))
					return;
				if (Durak()->OnSetSpectator(ClientID, pMsg->m_SpectatorID))
					return;
			}

			if (pMsg->m_SpecMode == SPEC_PLAYER && pMsg->m_SpectatorID >= 0)
				if (!Server()->ReverseTranslate(pMsg->m_SpectatorID, ClientID))
					return;

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

			// On 0.6 local client can be changed, allow emoticons from controller tee
			CPlayer *pProcessed = pPlayer->m_pControlledTee ? pPlayer->m_pControlledTee : pPlayer;

			pProcessed->UpdatePlaytime();
			pProcessed->m_LastEmote = Server()->Tick();

			SendEmoticon(pProcessed->GetCID(), pMsg->m_Emoticon);
			CCharacter *pChr = pProcessed->GetCharacter();
			if(pChr && Config()->m_SvEmotionalTees && pProcessed->m_EyeEmote)
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
				if (pProcessed->m_SpookyGhost)
					pChr->SetEmoteType(EMOTE_SURPRISE);
				else if (pChr->GetActiveWeapon() == WEAPON_HEART_GUN)
					pChr->SetEmoteType(EMOTE_HAPPY);
				pChr->SetEmoteStop(Server()->Tick() + 2 * Server()->TickSpeed());
			}
		}
		else if (MsgID == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
		{
			if (pPlayer->m_LastKill && pPlayer->m_LastKill + Server()->TickSpeed() * Config()->m_SvKillDelay > Server()->Tick())
				return;
			if (pPlayer->IsPaused())
				return;

			CCharacter* pChr = pPlayer->GetCharacter();
			if (!pChr)
				return;

			if (pChr->m_DrawEditor.Active())
			{
				pChr->m_DrawEditor.OnPlayerKill();
				return;
			}

			if (Durak()->ActivelyPlaying(ClientID))
			{
				return;
			}

			pPlayer->m_LastKill = Server()->Tick();

			int Fight = Arenas()->GetClientFight(ClientID);
			if (!Arenas()->FightStarted(ClientID))
			{	
				if (Fight >= 0)
				{
					Arenas()->EndFight(Fight);
					return;
				}
			}
			else if (!Arenas()->CanSelfkill(ClientID))
				return;

			if (!m_apPlayers[ClientID]->IsMinigame() && m_apPlayers[ClientID]->m_EscapeTime)
			{
				if (pPlayer->m_DieTick > Server()->Tick() - Server()->TickSpeed() * 10)
				{
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You need to wait %d seconds before killing again due to escaping"), 10 - (Server()->Tick() - pPlayer->m_DieTick) / Server()->TickSpeed());
					SendChatTarget(ClientID, aBuf);
					return;
				}
			}

			//Kill Protection
			int CurrTime = (Server()->Tick() - pChr->m_StartTime) / Server()->TickSpeed();
			if (Config()->m_SvKillProtection != 0 && CurrTime >= (60 * Config()->m_SvKillProtection) && pChr->m_DDRaceState == DDRACE_STARTED)
			{
				SendChatTarget(ClientID, pPlayer->Localize("Kill Protection enabled. If you really want to kill, type /kill"));
				return;
			}
			if (Config()->m_SvWalletKillProtection != 0 && pPlayer->GetWalletMoney() >= Config()->m_SvWalletKillProtection && pChr->m_FreezeTime)
			{
				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), pPlayer->Localize("Warning you have %lld money in your wallet! (see /money)"), pPlayer->GetWalletMoney());
				SendChatTarget(ClientID, aBuf);
				SendChatTarget(ClientID, pPlayer->Localize("Wallet kill Protection enabled. If you really want to kill, type /kill"));
				return;
			}

			CPlayer *pControlledTee = pPlayer->m_pControlledTee;
			if (pControlledTee)
			{
				if (pControlledTee->m_IsDummy)
				{
					pControlledTee->KillCharacter(WEAPON_SELF, false);
					pControlledTee->Respawn();
				}
			}
			else
			{
				if (pChr->m_LastWantedLogout && pChr->m_LastWantedLogout + Server()->TickSpeed() * Config()->m_SvKillLogout > Server()->Tick())
					m_Accounts.Logout(pPlayer->GetAccID());

				pPlayer->m_ToggleSpawn = pPlayer->m_PlayerFlags&PLAYERFLAG_SCOREBOARD;
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
			if(pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo+Server()->TickSpeed()*Config()->m_SvInfoChangeDelay > Server()->Tick())
				return;

			if(Config()->m_SvSpamprotection)
			{
				CNetMsg_Sv_ChangeInfoCooldown ChangeInfoCooldownMsg;
				ChangeInfoCooldownMsg.m_WaitUntil = Server()->Tick() + Server()->TickSpeed() * Config()->m_SvInfoChangeDelay;
				Server()->SendPackMsg(&ChangeInfoCooldownMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
			}

			pPlayer->UpdatePlaytime();
			pPlayer->m_LastChangeInfo = Server()->Tick();
			CNetMsg_Cl_SkinChange *pMsg = (CNetMsg_Cl_SkinChange *)pRawMsg;

			for(int p = 0; p < NUM_SKINPARTS; p++)
			{
				str_utf8_copy_num(pPlayer->m_TeeInfos.m_aaSkinPartNames[p], pMsg->m_apSkinPartNames[p], sizeof(pPlayer->m_TeeInfos.m_aaSkinPartNames[p]), MAX_SKIN_LENGTH);
				pPlayer->m_TeeInfos.m_aUseCustomColors[p] = pMsg->m_aUseCustomColors[p];
				pPlayer->m_TeeInfos.m_aSkinPartColors[p] = pMsg->m_aSkinPartColors[p];
			}

			pPlayer->m_TeeInfos.Translate(Server()->IsSevendown(ClientID));

			// F-DDrace
			pPlayer->CheckClanProtection();
			m_SavedTees.CheckLoadPlayer(ClientID);

			Server()->ExpireServerInfo();

			if (pPlayer->m_SpookyGhost || pPlayer->m_ForcedSkin != SKIN_NONE || (pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_IsZombie))
				return;

			// update currently visable teeinfos
			// it also gets set in SendSkinChange(), but that wont get executed if no up-to-date 0.7 client is connected, so definitely do it here aswell
			m_apPlayers[ClientID]->m_CurrentInfo.m_TeeInfos = pPlayer->m_TeeInfos;

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
		else if (MsgID == NETMSGTYPE_CL_SHOWDISTANCE)
		{
			CNetMsg_Cl_ShowDistance *pMsg = (CNetMsg_Cl_ShowDistance *)pRawMsg;
			pPlayer->m_ShowDistance = vec2(pMsg->m_X, pMsg->m_Y);
			pPlayer->m_SentShowDistance = true;

			float Aspect = pPlayer->m_ShowDistance.x / pPlayer->m_ShowDistance.y;
			CalcScreenParams(Aspect, 1.f, &pPlayer->m_StandardShowDistance.x, &pPlayer->m_StandardShowDistance.y);
		}
		else if (MsgID == NETMSGTYPE_CL_CAMERAINFO)
		{
			CNetMsg_Cl_CameraInfo *pMsg = (CNetMsg_Cl_CameraInfo *)pRawMsg;
			pPlayer->m_CameraInfo.Write(pMsg);
		}
		else if (MsgID == NETMSGTYPE_CL_ENABLESPECTATORCOUNT)
		{
			CNetMsg_Cl_EnableSpectatorCount *pMsg = (CNetMsg_Cl_EnableSpectatorCount *)pRawMsg;
			pPlayer->m_EnableSpectatorCount = pMsg->m_Enable;
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
			Server()->SetClientClan(ClientID, str_find_nocase(pMsg->m_pClan, "Zombie") ? "Human" : pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);

			for(int p = 0; p < NUM_SKINPARTS; p++)
			{
				str_utf8_copy_num(pPlayer->m_TeeInfos.m_aaSkinPartNames[p], pMsg->m_apSkinPartNames[p], sizeof(pPlayer->m_TeeInfos.m_aaSkinPartNames[p]), MAX_SKIN_LENGTH);
				pPlayer->m_TeeInfos.m_aUseCustomColors[p] = pMsg->m_aUseCustomColors[p];
				pPlayer->m_TeeInfos.m_aSkinPartColors[p] = pMsg->m_aSkinPartColors[p];
			}

			pPlayer->m_TeeInfos.Translate(Server()->IsSevendown(ClientID));

			// send clear vote options
			CNetMsg_Sv_VoteClearOptions ClearMsg;
			Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

			// Init before sending votes
			m_VotingMenu.InitPlayer(ClientID);

			// begin sending vote options
			StartResendingVotes(ClientID);

			// send tuning parameters to client
			SendTuningParams(ClientID);

			// client is ready to enter
			pPlayer->m_IsReadyToEnter = true;
			CNetMsg_Sv_ReadyToEnter m;
			Server()->SendPackMsg(&m, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);

			Server()->ExpireServerInfo();
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

	float NewValue = absolute(OldValue - pResult->GetFloat(1)) < 0.0001f
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
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "tuning", aBuf);
	}
}

void CGameContext::ConTuneZone(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int List = pResult->GetInteger(0);
	const char* pParamName = pResult->GetString(1);
	float NewValue = pResult->NumArguments() == 3 ? pResult->GetFloat(2) : -1;
	char aBuf[256];
	float Value;

	if (List >= 0 && List < TuneZone::NUM)
	{
		if (NewValue != -1 && pSelf->TuningList()[List].Set(pParamName, NewValue))
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
	if (List >= 0 && List < TuneZone::NUM)
	{
		for (int i = 0; i < pSelf->TuningList()[List].Num(); i++)
		{
			float v;
			pSelf->TuningList()[List].Get(i, &v);
			str_format(aBuf, sizeof(aBuf), "zone %d: %s %.2f", List, pSelf->TuningList()[List].ms_apNames[i], v);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "tuning", aBuf);
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
		if (List >= 0 && List < TuneZone::NUM)
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
		for (int i = 0; i < TuneZone::NUM; i++)
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
		if (List >= 0 && List < TuneZone::NUM)
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
		if (List >= 0 && List < TuneZone::NUM)
		{
			str_copy(pSelf->m_aaZoneLeaveMsg[List], pResult->GetString(1), sizeof(pSelf->m_aaZoneLeaveMsg[List]));
		}
	}
}

void CGameContext::ConTuneLock(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int List = pResult->GetInteger(0);
	const char *pParamName = pResult->GetString(1);
	float NewValue = pResult->GetFloat(2);

	if(List >= 0 && List < TuneZone::NUM)
	{
		CLockedTune LockedTune(pParamName, NewValue);
		char aBuf[256];
		int Result = pSelf->SetLockedTune(&pSelf->LockedTuning()[List], LockedTune, true);
		if(Result == 3)
		{
			str_format(aBuf, sizeof(aBuf), "Reset '%s' for lock %d", pParamName, List);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		}
		else if(Result)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' for lock %d changed to %.2f", pParamName, List, NewValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		}
		else
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
	}
}

void CGameContext::ConTuneLockReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int List = pResult->GetInteger(0);

	LOCKED_TUNES *pLockedTunings = &pSelf->LockedTuning()[List];

	char aBuf[256];
	if (pResult->NumArguments() == 1)
	{
		pLockedTunings->clear();
		pSelf->m_aaTuneLockMsg[List][0] = 0;
		if (List == 0)
			str_copy(aBuf, "Reset enter message for lock reset", sizeof(aBuf));
		else
			str_format(aBuf, sizeof(aBuf), "Reset all locked tunings and enter message for lock %d", List);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		return;
	}

	const char *pParam = pResult->GetString(1);

	float GlobalValue;
	if (!pSelf->m_Tuning.Get(pParam, &GlobalValue))
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
		return;
	}

	if (!pSelf->IsTuneInList(pLockedTunings, pParam))
	{
		str_format(aBuf, sizeof(aBuf), "'%s' is not in lock list %d", pParam, List);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		return;
	}

	CLockedTune Tune(pParam, GlobalValue);
	if (pSelf->SetLockedTune(pLockedTunings, Tune) == 3)
	{
		str_format(aBuf, sizeof(aBuf), "Reset '%s' for lock %d", pParam, List);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConTuneLockDump(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int List = pResult->GetInteger(0);
	char aBuf[256];
	if(List >= 0 && List < TuneZone::NUM)
	{
		for(unsigned int i = 0; i < pSelf->LockedTuning()[List].size(); i++)
		{
			str_format(aBuf, sizeof(aBuf), "lock %d: %s %.2f", List, pSelf->LockedTuning()[List][i].m_aParam, (float)pSelf->LockedTuning()[List][i].m_Value);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "tuning", aBuf);
		}
	}
}

void CGameContext::ConTuneLockSetMsgEnter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments())
	{
		int List = pResult->GetInteger(0);
		if(List >= 0 && List < TuneZone::NUM)
		{
			str_copy(pSelf->m_aaTuneLockMsg[List], pResult->GetString(1), sizeof(pSelf->m_aaTuneLockMsg[List]));
		}
	}
}

void CGameContext::ConSwitchOpen(IConsole::IResult* pResult, void* pUserData)
{
	CGameContext* pSelf = (CGameContext*)pUserData;
	int Switch = pResult->GetInteger(0);

	if (pSelf->Collision()->m_HighestSwitchNumber > 0 && Switch >= 0 && Switch < pSelf->Collision()->m_HighestSwitchNumber + 1)
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

void CGameContext::ConServerAlert(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	char aBuf[1024];
	str_copy(aBuf, pResult->GetString(0), sizeof(aBuf));
	UnescapeNewlines(aBuf);

	pSelf->SendServerAlert(aBuf);
}

void CGameContext::ConModAlert(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	const int Victim = pResult->GetVictim();
	if(Victim < 0 || Victim >= MAX_CLIENTS || !pSelf->m_apPlayers[Victim])
	{
		char aLogMsg[128];
		str_format(aLogMsg, sizeof(aLogMsg), "Client ID not found: %d", Victim);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "moderator_alert", aLogMsg);
		return;
	}

	char aBuf[1024];
	str_copy(aBuf, pResult->GetString(1), sizeof(aBuf));
	UnescapeNewlines(aBuf);

	pSelf->SendModeratorAlert(aBuf, Victim);
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
	if (Team == TEAM_RED)
		pSelf->SendChat(-1, CHAT_ALL, -1, Localizable("All players were moved to the game"));
	else
		pSelf->SendChat(-1, CHAT_ALL, -1, Localizable("All players were moved to the spectators"));

	for (int i = 0; i < MAX_CLIENTS; ++i)
		if (pSelf->m_apPlayers[i])
			pSelf->m_apPlayers[i]->SetTeam(Team, false);
}

void CGameContext::ConForceVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pType = pResult->GetString(0);
	const char *pValue = pResult->GetString(1);
	const char *pReason = pResult->NumArguments() > 2 && pResult->GetString(2)[0] ? pResult->GetString(2) : "No reason given";
	char aBuf[128] = {0};

	if(str_comp_nocase(pType, "option") == 0)
	{
		CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
		while(pOption)
		{
			if(str_comp_nocase(pValue, pOption->m_aDescription) == 0)
			{
				pSelf->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("Authorized player forced server option '%s' (%s)"), pValue, pReason);
				pSelf->Console()->ExecuteLine(pOption->m_aCommand);
				break;
			}

			pOption = pOption->m_pNext;
		}

		if(!pOption)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' isn't an option on this server", pValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "server", aBuf);
			return;
		}
	}
	else if(str_comp_nocase(pType, "kick") == 0)
	{
		int KickID = str_toint(pValue);
		if(!pSelf->Server()->ReverseTranslate(KickID, pResult->m_ClientID))
			return;
		if(KickID < 0 || KickID >= MAX_CLIENTS || !pSelf->m_apPlayers[KickID])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "server", "Invalid client id to kick");
			return;
		}

		if(!pSelf->Config()->m_SvVoteKickBantime)
		{
			str_format(aBuf, sizeof(aBuf), "kick %d %s", KickID, pReason);
			pSelf->Console()->ExecuteLine(aBuf, IConsole::CLIENT_ID_UNSPECIFIED, false);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			pSelf->Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
			str_format(aBuf, sizeof(aBuf), "ban %s %d %s", aAddrStr, pSelf->Config()->m_SvVoteKickBantime, pReason);
			pSelf->Console()->ExecuteLine(aBuf, IConsole::CLIENT_ID_UNSPECIFIED, false);
		}
	}
	else if(str_comp_nocase(pType, "spectate") == 0)
	{
		int SpectateID = str_toint(pValue);
		if (!pSelf->Server()->ReverseTranslate(SpectateID, pResult->m_ClientID))
			return;
		if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !pSelf->m_apPlayers[SpectateID] || pSelf->m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "server", "Invalid client id to move");
			return;
		}

		pSelf->SendChatFormat(-1, CHAT_ALL, -1, CGameContext::CHATFLAG_ALL, Localizable("'%s' was moved to spectator (%s)"), pSelf->Server()->ClientName(SpectateID), pReason);
		str_format(aBuf, sizeof(aBuf), "set_team %d -1 %d", SpectateID, pSelf->Config()->m_SvVoteSpectateRejoindelay);
		pSelf->Console()->ExecuteLine(aBuf, IConsole::CLIENT_ID_UNSPECIFIED, false);
	}
}


void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);
	pSelf->AddVote(pDescription, pCommand);
}

void CGameContext::AddVote(const char *pDescription, const char *pCommand)
{
	if(m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of vote options reached");
		return;
	}

	// Force, "info" is used as placeholder cmd
	if (str_comp(pCommand, "info") != 0)
	{
		// check for valid option
		if(!Console()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}

		pDescription = str_skip_whitespaces_const(pDescription);
		if(str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}

		// check for duplicate entry
		for(CVoteOptionServer *pOption = m_pVoteOptionFirst; pOption; pOption = pOption->m_pNext)
		{
			if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				return;
			}
		}
	}

	// add the option
	++m_NumVoteOptions;
	int Len = str_length(pCommand);

	CVoteOptionServer *pOption = (CVoteOptionServer *)m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len, alignof(CVoteOptionServer));
	pOption->m_pNext = 0;
	pOption->m_pPrev = m_pVoteOptionLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	m_pVoteOptionLast = pOption;
	if(!m_pVoteOptionFirst)
		m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription, sizeof(pOption->m_aDescription));
	mem_copy(pOption->m_aCommand, pCommand, Len+1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
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
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "server", aBuf);
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
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len, alignof(CVoteOptionServer));
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

	// Reset so the votes get added again
	pSelf->m_VotingMenu.AddPlaceholderVotes();

	// reset sending of vote options
	for(int i = 0; i < MAX_CLIENTS; i++)
		pSelf->m_VotingMenu.SendPageVotes(i);
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

void CGameContext::ConchainUpdateLocalChat(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if (pResult->NumArguments())
	{
		CGameContext* pSelf = (CGameContext*)pUserData;
		if (!pSelf->Config()->m_SvLocalChat)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->m_LocalChat)
				{
					pSelf->m_apPlayers[i]->m_LocalChat = false;
					pSelf->SendChatTarget(i, pSelf->m_apPlayers[i]->Localize("Local chat mode has been disabled, automatically entered public chat"));
				}
			}
		}
	}
}

void CGameContext::ConchainUpdateBankMode(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if (pResult->NumArguments())
	{
		CGameContext* pSelf = (CGameContext*)pUserData;
		if (!pSelf->Config()->m_SvMoneyBankMode)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (pSelf->m_apPlayers[i] && pSelf->m_apPlayers[i]->GetWalletMoney() && pSelf->m_apPlayers[i]->GetAccID() >= ACC_START)
				{
					pSelf->m_apPlayers[i]->BankTransaction(pSelf->m_apPlayers[i]->GetWalletMoney(), "automatic wallet to bank due to bank disabling");
					// Manually set wallet money instead of using WalletTransaction, because SvMoneyBankMode 0 redirects walelttransactions to bank, which doesn't make any sense here
					pSelf->m_apPlayers[i]->SetWalletMoney(0);
					pSelf->SendChatTarget(i, pSelf->m_apPlayers[i]->Localize("Automatic wallet to bank due to bank disabling"));
				}
			}
		}
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
	Console()->Register("tune_lock", "i[number] s[tuning] i[value]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneLock, this, "Tune for lock a variable to value", AUTHED_ADMIN);
	Console()->Register("tune_lock_reset", "i[number] ?s[tuning]", CFGFLAG_SERVER, ConTuneLockReset, this, "Reset a specific locked tuning variable to default for lock i or all and enter message", AUTHED_ADMIN);
	Console()->Register("tune_lock_dump", "i[number]", CFGFLAG_SERVER, ConTuneLockDump, this, "Dump lock tuning for number x", AUTHED_HELPER);
	Console()->Register("tune_lock_enter", "i[number] r[message]", CFGFLAG_SERVER | CFGFLAG_GAME, ConTuneLockSetMsgEnter, this, "which message to display on tune lock enter; use 0 for lock reset", AUTHED_ADMIN);
	Console()->Register("switch_open", "i[switch]", CFGFLAG_SERVER|CFGFLAG_GAME, ConSwitchOpen, this, "Whether a switch is deactivated by default (otherwise activated)", AUTHED_ADMIN);

	Console()->Register("pausegame", "?i[on/off]", CFGFLAG_SERVER|CFGFLAG_STORE, ConPause, this, "Pause/unpause game", AUTHED_ADMIN);
	Console()->Register("change_map", "?r[map]", CFGFLAG_SERVER|CFGFLAG_STORE, ConChangeMap, this, "Change map", AUTHED_ADMIN);
	Console()->Register("restart", "?i[seconds]", CFGFLAG_SERVER|CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)", AUTHED_ADMIN);
	Console()->Register("server_alert", "r[message]", CFGFLAG_SERVER, ConServerAlert, this, "Send a server alert message to all players", AUTHED_ADMIN);
	Console()->Register("mod_alert", "v[id] r[message]", CFGFLAG_SERVER, ConModAlert, this, "Send a moderator alert message to player", AUTHED_MOD);
	Console()->Register("say", "r[message]", CFGFLAG_SERVER, ConSay, this, "Say in chat", AUTHED_MOD);
	Console()->Register("broadcast", "r[message]", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message", AUTHED_MOD);
	Console()->Register("set_team", "i[id] i[team-id] ?i[delay in minutes]", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team", AUTHED_ADMIN);
	Console()->Register("set_team_all", "i[team-id]", CFGFLAG_SERVER, ConSetTeamAll, this, "Set team of all players to team", AUTHED_ADMIN);

	Console()->Register("force_vote", "s[type] s[option] ?r[reason]", CFGFLAG_SERVER, ConForceVote, this, "Force a voting option", AUTHED_ADMIN);
	Console()->Register("add_vote", "s[name] r[command]", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option", AUTHED_ADMIN);
	Console()->Register("remove_vote", "s[name]", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option", AUTHED_ADMIN);
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options", AUTHED_ADMIN);
	Console()->Register("vote", "r['yes'|'no']", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no", AUTHED_ADMIN);
	Console()->Register("dump_antibot", "?i[id]", CFGFLAG_SERVER, ConDumpAntibot, this, "Dumps the antibot status", AUTHED_ADMIN);

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
	Console()->Chain("sv_local_chat", ConchainUpdateLocalChat, this);
	Console()->Chain("sv_money_bank_mode", ConchainUpdateBankMode, this);

	#define CONSOLE_COMMAND(name, params, flags, callback, userdata, help, accesslevel) m_pConsole->Register(name, params, flags, callback, userdata, help, accesslevel);
	#include <game/ddracecommands.h>
	#undef CONSOLE_COMMAND

	// Keep this for backwards compatibility
	#define CHAT_COMMAND(name, params, flags, callback, userdata, help, accesslevel) m_pConsole->Register(name, params, flags, callback, userdata, help, accesslevel);
	#include "ddracechat.h"
	#undef CHAT_COMMAND

	m_VotingMenu.Init(this);
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
	m_pAntibot = Kernel()->RequestInterface<IAntibot>();
	m_pAntibot->RoundStart(this);
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);
	m_CommandManager.Init(m_pConsole, this, NewCommandHook, RemoveCommandHook);

	m_GameUuid = RandomUuid();
	Console()->SetTeeHistorianCommandCallback(CommandCallback, this);
	Console()->SetIsDummyCallback(ConsoleIsDummyCallback, this);
	Console()->SetIsInViewCallback(ConsoleIsInViewCallback, this);

	// HACK: only set static size for items, which were available in the first 0.7 release
	// so new items don't break the snapshot delta
	static const int OLD_NUM_NETOBJTYPES = 23;
	for(int i = 0; i < OLD_NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers, m_pConfig);

	// reset tune locks
	for(int i = 0; i < TuneZone::NUM; i++)
		LockedTuning()[i].clear();

	// Reset Tunezones
	CTuningParams TuningParams;
	for (int i = 0; i < TuneZone::NUM; i++)
	{
		TuningList()[i] = TuningParams;
		TuningList()[i].Set("gun_curvature", 0);
		TuningList()[i].Set("gun_speed", 1400);
		TuningList()[i].Set("shotgun_curvature", 0);
		TuningList()[i].Set("shotgun_speed", 500);
	}

	for (int i = 0; i < TuneZone::NUM; i++)
	{
		// Send no text by default when changing tune zones.
		m_aaZoneEnterMsg[i][0] = 0;
		m_aaZoneLeaveMsg[i][0] = 0;
		m_aaTuneLockMsg[i][0] = 0;
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
		//Config()->m_SvTeam = 1;
		Config()->m_SvShowOthersDefault = 0;

		if (Collision()->m_HighestSwitchNumber > 0)
			for (int i = 0; i < Collision()->m_HighestSwitchNumber + 1; ++i)
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

		for (int i = 0; i < TuneZone::NUM; i++)
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

	// F-DDrace
	FDDraceInitPreMapInit();

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);

	CTile* pFront = 0;
	CSwitchTile* pSwitch = 0;
	if (m_Layers.FrontLayer())
		pFront = (CTile*)Kernel()->RequestInterface<IMap>()->GetData(m_Layers.FrontLayer()->m_Front);
	if (m_Layers.SwitchLayer())
		pSwitch = (CSwitchTile*)Kernel()->RequestInterface<IMap>()->GetData(m_Layers.SwitchLayer()->m_Switch);

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

			if (Index > ENTITY_OFFSET)
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

				if (Index > ENTITY_OFFSET)
				{
					m_pController->OnEntity(Index, Pos, LAYER_FRONT, pFront[y * pTileMap->m_Width + x].m_Flags);
				}
			}
			if (pSwitch)
			{
				Index = pSwitch[y * pTileMap->m_Width + x].m_Type;
				// TODO: Add off by default door here
				// if (Index == TILE_DOOR_OFF)
				if (Index > ENTITY_OFFSET || Index == TILE_DURAK_TABLE || Index == TILE_DURAK_SEAT)
				{
					m_pController->OnEntity(Index, Pos, LAYER_SWITCH, pSwitch[y * pTileMap->m_Width + x].m_Flags, pSwitch[y * pTileMap->m_Width + x].m_Number);
				}
			}
		}
	}

	// clamp sv_player_slots to 0..MaxClients
	if(Config()->m_SvMaxClients < Config()->m_SvPlayerSlots)
		Config()->m_SvPlayerSlots = Config()->m_SvMaxClients;

	FDDraceInit();

#ifdef CONF_DEBUG
	// clamp dbg_dummies to 0..MAX_CLIENTS-1
	if(MAX_CLIENTS <= Config()->m_DbgDummies)
		Config()->m_DbgDummies = MAX_CLIENTS;
	if(Config()->m_DbgDummies)
	{
		for(int i = 0; i < Config()->m_DbgDummies ; i++)
		{
			OnClientConnected(MAX_CLIENTS-i-1, true, false);
			OnClientEnter(MAX_CLIENTS-i-1);
		}
	}
#endif
}

void CGameContext::FDDraceInitPreMapInit()
{
	Collision()->m_vTiles.clear();
	Collision()->m_vTiles.resize(NUM_INDICES);

	Collision()->m_vRedirectTiles.clear();

	// reset plots here but load them after the map init
	m_Plots.Init(this);

	// Durak has to be initialized before the map initialization
	for (int i = 0; i < NUM_MINIGAMES; i++)
		if (m_pMinigames[i])
			delete m_pMinigames[i];
	m_pMinigames[MINIGAME_BLOCK] = new CMinigame(this, MINIGAME_BLOCK);
	m_pMinigames[MINIGAME_SURVIVAL] = new CSurvival(this);
	m_pMinigames[MINIGAME_1VS1] = new CArenas(this);
	m_pMinigames[MINIGAME_DURAK] = new CDurak(this);
	m_pMinigames[MINIGAME_INSTAGIB_BOOMFNG] = new CMinigame(this, MINIGAME_INSTAGIB_BOOMFNG);
	m_pMinigames[MINIGAME_INSTAGIB_FNG] = new CMinigame(this, MINIGAME_INSTAGIB_FNG);

	for (int i = 0; i < NUM_MINIGAMES; i++)
		m_aMinigameDisabled[i] = false;
	// Validate when adding table tiles
	m_aMinigameDisabled[MINIGAME_DURAK] = true;
}

void CGameContext::FDDraceInit()
{
	// Save memory. Only save those few tiles we really use when calling CCollision::GetRandomTile or CGameworld::CanSpawn (due to calling getrandomtile)
	bool aRequiredRandomTilePositions[NUM_INDICES] = { 0 };
	#define REQUIRED_TILE(index) aRequiredRandomTilePositions[(index)] = true
	REQUIRED_TILE(ENTITY_SPAWN);
	REQUIRED_TILE(ENTITY_SPAWN_RED);
	REQUIRED_TILE(ENTITY_SPAWN_BLUE);
	// for dummy spawns
	REQUIRED_TILE(ENTITY_SHOP_DUMMY_SPAWN);
	REQUIRED_TILE(ENTITY_PLOT_SHOP_DUMMY_SPAWN);
	REQUIRED_TILE(ENTITY_BANK_DUMMY_SPAWN);
	REQUIRED_TILE(ENTITY_TAVERN_DUMMY_SPAWN);
	REQUIRED_TILE(TILE_SHOP);
	REQUIRED_TILE(TILE_PLOT_SHOP);
	REQUIRED_TILE(TILE_BANK);
	REQUIRED_TILE(TILE_TAVERN);
	// minigames
	REQUIRED_TILE(TILE_MINIGAME_BLOCK);
	REQUIRED_TILE(TILE_SURVIVAL_LOBBY);
	REQUIRED_TILE(TILE_SURVIVAL_SPAWN);
	REQUIRED_TILE(TILE_SURVIVAL_DEATHMATCH);
	REQUIRED_TILE(TILE_1VS1_LOBBY);
	REQUIRED_TILE(TILE_DURAK_LOBBY);
	// jail
	REQUIRED_TILE(TILE_JAIL);
	REQUIRED_TILE(TILE_JAIL_RELEASE);
	#undef REQUIRED_TILE
	for (int i = 0; i < NUM_INDICES; i++)
	{
		Collision()->m_aTileUsed[i] = Collision()->GetRandomTile(i) != vec2(-1, -1);
		if (!aRequiredRandomTilePositions[i])
			Collision()->m_vTiles[i].clear();
	}

	// check if there are minigame spawns available (survival and instagib are checked in their own ticks)
	m_aMinigameDisabled[MINIGAME_BLOCK] = !Collision()->TileUsed(TILE_MINIGAME_BLOCK);
	m_aMinigameDisabled[MINIGAME_1VS1] = !Collision()->TileUsed(TILE_1VS1_LOBBY);

	CreateFolders();

	m_Accounts.Init(this);

	// load plot data AFTER map init
	m_Plots.LoadData();

	m_PlayerMapping.Init(this);
	m_SavedTees.Init(this);

	if (Config()->m_SvBansFile[0])
		Console()->ExecuteFile(Config()->m_SvBansFile);
	if (Config()->m_SvWhitelistFile[0])
		Console()->ExecuteFile(Config()->m_SvWhitelistFile);

	{
		time_t rawtime;
		struct tm* timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);

		int Seconds = 60 - timeinfo->tm_sec;
		int Minutes = (60 - timeinfo->tm_min) - 1;

		m_FullHourOffsetTicks = (Seconds * Server()->TickSpeed()) + (Minutes * 60 * Server()->TickSpeed());
	}

	for (int i = 0; i < NUM_HOUSES; i++)
		if (m_pHouses[i])
			delete m_pHouses[i];
	m_pHouses[HOUSE_SHOP] = new CShop(this, HOUSE_SHOP);
	m_pHouses[HOUSE_PLOT_SHOP] = new CShop(this, HOUSE_PLOT_SHOP);
	m_pHouses[HOUSE_BANK] = new CBank(this);
	m_pHouses[HOUSE_TAVERN] = new CTavern(this);

	m_RainbowName.Init(this);
	m_WhoIs.Init(this);

	SetMapSpecificOptions();
	if (Config()->m_SvDefaultDummies)
	{
		ConnectDefaultDummies();
	}
	else
	{
		for (int i = 0; i < NUM_HOUSES; i++)
			ConnectHouseDummy(i, true);
	}

	m_LastPlayerCountUpdate = 0;
	SendPlayerCountUpdate();
}

void CGameContext::OnPreShutdown()
{
	bool ServerIsStopping = ((CServer *)Server())->m_RunServer == CServer::STOPPING;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = m_apPlayers[i];
		if (!pPlayer)
			continue;

		Durak()->OnPlayerLeave(i, false, true);
		Arenas()->OnPlayerLeave(i, false, true);
		Survival()->OnPlayerLeave(i, false, true);

		// Move all money from wallet to bank
		if (pPlayer->GetAccID() >= ACC_START)
		{
			pPlayer->BankTransaction(pPlayer->GetWalletMoney(), "automatic wallet to bank due to shutdown");
			pPlayer->WalletTransaction(-pPlayer->GetWalletMoney());
		}

		if (pPlayer->GetCharacter())
		{
			// Either save the character and it's money or simply drop the money so it can get loaded on next server start
			if (Config()->m_SvShutdownSaveTees)
			{
				m_SavedTees.SaveCharacter(i, SAVE_WALLET|SAVE_FLAG|SAVE_SHUTDOWN, Config()->m_SvShutdownSaveTeeExpire);
			}
			else
			{
				pPlayer->GetCharacter()->DropMoney(pPlayer->GetWalletMoney());
			}
		}

		// properly disconnect dummies on reload/shutdown
		if (pPlayer->m_IsDummy)
		{
			Server()->DummyLeave(i);
		}
		else if (Config()->m_SvShutdownAutoReconnect == 1 && Server()->IsSevendown(i) && ServerIsStopping)
		{
			// 0.7 is not supported, they would just time out, cl_reconnect_timeout is a ddnet feature.
			const char* pMsg = ((CServer*)Server())->m_NetServer.m_ShutdownMessage;
			CMsgPacker Msg(NETMSG_MAP_CHANGE, true);
			Msg.AddString(pMsg[0] != '\0' ? pMsg : "Server is restarting...", 0);
			Msg.AddInt(0);
			Msg.AddInt(1);
			/*if (!Server()->IsSevendown(i))
			{
				Msg.AddInt(0);
				Msg.AddInt(0);
				Msg.AddRaw(&SHA256_ZEROED, sizeof(SHA256_ZEROED));
			}*/
			Server()->SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, i);
		}
	}

	m_Accounts.WriteData();
	m_Plots.WriteData();

	if (Config()->m_SvBansFile[0])
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "bans_save \"%s\"", Config()->m_SvBansFile);
		Console()->ExecuteLine(aBuf);
	}
	Server()->SaveWhitelist(Config()->m_SvWhitelistFile);
	SendPlayerCountUpdate(true);

	if (ServerIsStopping && Config()->m_SvShutdownAutoReconnect == 2 && Config()->m_SvShutdownSaveTees)
	{
		char *pMsg = ((CServer *)Server())->m_NetServer.m_ShutdownMessage;
		str_copy(pMsg, "Restarting... Your stats will be fully reloaded shortly", sizeof(((CServer*)Server())->m_NetServer.m_ShutdownMessage));
	}
}

void CGameContext::OnShutdown(bool FullShutdown)
{
	Antibot()->RoundEnd();

	if (FullShutdown)
		Score()->OnShutdown();

	if(m_TeeHistorianActive)
	{
		m_TeeHistorian.Finish();
		io_close(m_TeeHistorianFile);
	}

	Console()->ResetServerGameSettings();
	Collision()->Dest();
	delete m_pController;
	m_pController = 0;
	for (int i = 0; i < NUM_HOUSES; i++)
	{
		delete m_pHouses[i];
		m_pHouses[i] = 0;
	}
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

	if (Config()->m_SvLoadMapConfigFile)
	{
		// Execute as id game, (map settings) anyways, removed OnMapChange settings import logic. Use cfg file for overrides
		Console()->ExecuteFile(aBuf, IConsole::CLIENT_ID_GAME);
	}
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

	// snap in this order to make sure we always send important and required gameinfo aswell as players, then the gameworld with all entities
	m_pController->Snap(ClientID);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
			m_apPlayers[i]->Snap(ClientID);
	}
	if (ClientID > -1)
		m_apPlayers[ClientID]->FakeSnap();

	for (int i = 0; i < NUM_MINIGAMES; i++)
		m_pMinigames[i]->Snap(ClientID);

	m_Events.Snap(ClientID);
	m_World.Snap(ClientID);
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	// Call m_Events.Clear() before PostSnap to switch the buffer
	m_Events.Clear();
	m_World.PostSnap();
	Durak()->PostSnap();
}

bool CGameContext::IsClientBot(int ClientID) const
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->IsDummy();
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
const char* CGameContext::VersionSevendown() const { return GAME_VERSION_SEVENDOWN; }
const char *CGameContext::NetVersion() const { return GAME_NETVERSION; }
const char *CGameContext::NetVersionSevendown() const { return GAME_NETVERSION_SEVENDOWN; }

IGameServer *CreateGameServer() { return new CGameContext; }

void CGameContext::SendChatResponseAll(const char* pLine, void* pUser)
{
	CGameContext* pSelf = (CGameContext*)pUser;

	static int ReentryGuard = 0;
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

	static int ReentryGuard = 0;

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
	else if(Config()->m_SvDnsblChat && Server()->DnsblBlack(ClientID))
	{
		SendChatTarget(ClientID, m_apPlayers[ClientID]->Localize("Players are not allowed to chat from VPNs at this time"));
		return 1;
	}
	else
		m_apPlayers[ClientID]->m_LastChat = Server()->Tick();
	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);
	int Muted = 0;

	for(int i = 0; i < m_NumMutes && !Muted; i++)
	{
		if(!net_addr_comp(&Addr, &m_aMutes[i].m_Addr, false))
			Muted = (m_aMutes[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
	}

	if (Muted > 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof aBuf, m_apPlayers[ClientID]->Localize("You are not permitted to talk for the next %d seconds."), Muted);
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

bool CGameContext::IsVersionBanned(int Version)
{
	char aVersion[16];
	str_format(aVersion, sizeof(aVersion), "%d", Version);

	return str_in_list(Config()->m_SvBannedVersions, ",", aVersion);
}

void CGameContext::List(int ClientID, const char* pFilter)
{
	#define SEND(str) \
		do \
		{ \
			if (ClientID == -1) \
				Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "console", str); \
			else \
				SendChatTarget(ClientID, m_apPlayers[ClientID]->Localize(str)); \
		} while(0)

	int Total = 0;
	int Dummies = 0;
	char aBuf[128];
	int Bufcnt = 0;
	if (pFilter[0])
		str_format(aBuf, sizeof(aBuf), Localizable("Listing players with \"%s\" in name:"), pFilter);
	else
		str_format(aBuf, sizeof(aBuf), Localizable("Listing all players:"));
	SEND(aBuf);
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (m_apPlayers[i])
		{
			Total++;
			if (Server()->GetDummy(i) != -1)
				Dummies++;

			const char* pName = Server()->ClientName(i);
			if (str_find_nocase(pName, pFilter) == NULL)
				continue;
			if (Bufcnt + str_length(pName) + 4 > 128)
			{
				SEND(aBuf);
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
		SEND(aBuf);
	str_format(aBuf, sizeof(aBuf), "%d players online, including %d client dummies", Total, Dummies/2);
	SEND(aBuf);
	#undef SEND
}

int CGameContext::GetClientDDNetVersion(int ClientID)
{
	if (ClientID < 0)
		return 0;

	IServer::CClientInfo Info = {0};
	Server()->GetClientInfo(ClientID, &Info);
	return Info.m_DDNetVersion;
}

Mask128 CGameContext::ClientsMaskExcludeClientVersionAndHigher(int Version)
{
	Mask128 Mask = CmaskNone();
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GetClientDDNetVersion(i) >= Version)
			continue;
		Mask |= CmaskOne(i);
	}
	return Mask;
}

bool CGameContext::RateLimitPlayerVote(int ClientID)
{
	int64 Now = Server()->Tick();
	int64 TickSpeed = Server()->TickSpeed();
	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(Config()->m_SvRconVote && !Server()->GetAuthedState(ClientID))
	{
		SendChatTarget(ClientID, pPlayer->Localize("You can only vote after logging in."));
		return true;
	}

	if(Config()->m_SvDnsblVote)
	{
		if(m_pServer->DnsblPending(ClientID))
		{
			SendChatTarget(ClientID, m_apPlayers[ClientID]->Localize("You are not allowed to vote because we're currently checking for VPNs. Try again in ~30 seconds."));
			return true;
		}
		else if(m_pServer->DnsblBlack(ClientID))
		{
			SendChatTarget(ClientID, m_apPlayers[ClientID]->Localize("You are not allowed to vote because you appear to be using a VPN. Try connecting without a VPN or contacting an admin if you think this is a mistake."));
			return true;
		}
	}

	if(m_VoteCloseTime)
	{
		SendChatTarget(ClientID, pPlayer->Localize("Wait for current vote to end before calling a new one."));
		return true;
	}

	if(Now < pPlayer->m_FirstVoteTick)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), pPlayer->Localize("You must wait %d seconds before making your first vote."), (int)((pPlayer->m_FirstVoteTick - Now) / TickSpeed) + 1);
		SendChatTarget(ClientID, aBuf);
		return true;
	}

	int TimeLeft = pPlayer->m_LastVoteCall + TickSpeed * Config()->m_SvVoteDelay - Now;
	if(pPlayer->m_LastVoteCall && TimeLeft > 0)
	{
		char aChatmsg[64];
		str_format(aChatmsg, sizeof(aChatmsg), pPlayer->Localize("You must wait %d seconds before making another vote."), (int)(TimeLeft / TickSpeed) + 1);
		SendChatTarget(ClientID, aChatmsg);
		return true;
	}

	NETADDR Addr;
	Server()->GetClientAddr(ClientID, &Addr);
	int VoteMuted = 0;
	for(int i = 0; i < m_NumVoteMutes && !VoteMuted; i++)
		if(!net_addr_comp(&Addr, &m_aVoteMutes[i].m_Addr, false))
			VoteMuted = (m_aVoteMutes[i].m_Expire - Server()->Tick()) / Server()->TickSpeed();
	if(VoteMuted > 0)
	{
		char aChatmsg[64];
		str_format(aChatmsg, sizeof(aChatmsg), pPlayer->Localize("You are not permitted to vote for the next %d seconds."), VoteMuted);
		SendChatTarget(ClientID, aChatmsg);
		return true;
	}
	return false;
}

bool CGameContext::RateLimitPlayerMapVote(int ClientID)
{
	if(!Server()->GetAuthedState(ClientID) && time_get() < m_LastMapVote + (time_freq() * Config()->m_SvVoteMapTimeDelay))
	{
		char aChatmsg[512] = {0};
		str_format(aChatmsg, sizeof(aChatmsg), m_apPlayers[ClientID]->Localize("There's a %d second delay between map-votes, please wait %d seconds."),
				Config()->m_SvVoteMapTimeDelay, (int)((m_LastMapVote + Config()->m_SvVoteMapTimeDelay * time_freq() - time_get())/time_freq()));
		SendChatTarget(ClientID, aChatmsg);
		return true;
	}
	return false;
}

void CGameContext::OnUpdatePlayerServerInfo(char *aBuf, int BufSize, int ID)
{
	if(!m_apPlayers[ID])
		return;

	char aCSkinName[64];

	CTeeInfo &TeeInfo = m_apPlayers[ID]->m_TeeInfos;

	char aJsonSkin[400];
	aJsonSkin[0] = '\0';

	// Only use 0.6 info, as only 0.6 clients handle this info right now, and it doesnt make sense to ship 0.7 skin info to 0.6 clients yet, if they wont display anything then
	//if(Server()->IsSevendown(ID))
	{
		// 0.6
		if(TeeInfo.m_Sevendown.m_UseCustomColor)
		{
			str_format(aJsonSkin, sizeof(aJsonSkin),
				"\"name\":\"%s\","
				"\"color_body\":%d,"
				"\"color_feet\":%d",
				EscapeJson(aCSkinName, sizeof(aCSkinName), TeeInfo.m_Sevendown.m_SkinName),
				TeeInfo.m_Sevendown.m_ColorBody,
				TeeInfo.m_Sevendown.m_ColorFeet);
		}
		else
		{
			str_format(aJsonSkin, sizeof(aJsonSkin),
				"\"name\":\"%s\"",
				EscapeJson(aCSkinName, sizeof(aCSkinName), TeeInfo.m_Sevendown.m_SkinName));
		}
	}
	/*else
	{
		const char *apPartNames[NUM_SKINPARTS] = {"body", "marking", "decoration", "hands", "feet", "eyes"};
		char aPartBuf[64];

		for(int i = 0; i < NUM_SKINPARTS; ++i)
		{
			str_format(aPartBuf, sizeof(aPartBuf),
				"%s\"%s\":{"
				"\"name\":\"%s\"",
				i == 0 ? "" : ",",
				apPartNames[i],
				EscapeJson(aCSkinName, sizeof(aCSkinName), TeeInfo.m_aaSkinPartNames[i]));

			str_append(aJsonSkin, aPartBuf, sizeof(aJsonSkin));

			if(TeeInfo.m_aUseCustomColors[i])
			{
				str_format(aPartBuf, sizeof(aPartBuf),
					",color:%d",
					TeeInfo.m_aSkinPartColors[i]);
				str_append(aJsonSkin, aPartBuf, sizeof(aJsonSkin));
			}
			str_append(aJsonSkin, "}", sizeof(aJsonSkin));
		}
	}*/

	str_format(aBuf, BufSize,
		",\"skin\":{"
		"%s"
		"},"
		"\"afk\":%s,"
		"\"team\":%d",
		aJsonSkin,
		JsonBool(m_apPlayers[ID]->m_Afk),
		m_apPlayers[ID]->GetTeam());
}

// DDRace

void CGameContext::CallVote(int ClientID, const char *pDesc, const char *pCmd, const char *pReason, const char *pChatmsg, const char *pSevendownDesc, CFormatArg *pArgs, int NumArgs)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	int64 Now = Server()->Tick();
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return;

	SendChat(-1, CHAT_ALL, -1, pChatmsg, -1, CHAT_SEVENDOWN, pArgs, NumArgs);
	if(!pSevendownDesc)
		pSevendownDesc = pDesc;

	m_VoteCreator = ClientID;
	StartVote(pDesc, pCmd, pReason, pSevendownDesc);
	pPlayer->m_Vote = 1;
	pPlayer->m_VotePos = m_VotePos = 1;
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

void CGameContext::SetExpireDateDays(time_t *pDate, float Days)
{
	SetExpireDate(pDate, Days*24.f, true);
}

void CGameContext::SetExpireDate(time_t *pDate, float Hours, bool SetMinutesZero)
{
	time_t Now;
	struct tm ExpireDate;
	time(&Now);
	ExpireDate = *localtime(&Now);

	// add another x days if we have the item already
	if (*pDate != 0)
	{
		struct tm AccDate;
		AccDate = *localtime(pDate);

		ExpireDate.tm_year = AccDate.tm_year;
		ExpireDate.tm_mon = AccDate.tm_mon;
		ExpireDate.tm_mday = AccDate.tm_mday;
		ExpireDate.tm_hour = AccDate.tm_hour;
	}

	const time_t ONE_HOUR = 60 * 60;
	time_t DateSeconds = mktime(&ExpireDate) + (Hours * ONE_HOUR);
	ExpireDate = *localtime(&DateSeconds);

	// we set minutes and seconds to 0 always :)
	if (SetMinutesZero)
		ExpireDate.tm_min = 0;
	ExpireDate.tm_sec = 0;
	
	*pDate = mktime(&ExpireDate);
}

bool CGameContext::IsExpired(time_t Date)
{
	if (!Date)
		return false;

	struct tm AccDate;
	AccDate = *localtime(&Date);

	time_t Now;
	struct tm ExpireDate;
	time(&Now);
	ExpireDate = *localtime(&Now);

	ExpireDate.tm_year = AccDate.tm_year;
	ExpireDate.tm_mon = AccDate.tm_mon;
	ExpireDate.tm_mday = AccDate.tm_mday;
	ExpireDate.tm_hour = AccDate.tm_hour;
	ExpireDate.tm_min = AccDate.tm_min;
	ExpireDate.tm_sec = AccDate.tm_sec;

	double Seconds = difftime(Now, mktime(&ExpireDate));
	int Minutes = Seconds / 60;
	return Minutes >= 0;
}

const char *CGameContext::GetDate(time_t Time, bool ShowTime)
{
	if (Time < 0)
		return "";

	time_t tmp = Time;
	struct tm Date = *localtime(&tmp);

	static char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%02d.%02d.%d", Date.tm_mday, Date.tm_mon+1, Date.tm_year+1900);

	if (ShowTime)
	{
		char aTime[64];
		str_format(aTime, sizeof(aTime), " (%02d:%02d)", Date.tm_hour, Date.tm_min);
		str_append(aBuf, aTime, sizeof(aBuf));
	}

	return aBuf;
}

void CGameContext::OnRedirectSaveTeeAdd(const char *pHash)
{
	m_SavedTees.OnRedirectSaveTeeAdd(pHash);
}

void CGameContext::OnRedirectSaveTeeRemove(const char *pHash)
{
	m_SavedTees.OnRedirectSaveTeeRemove(pHash);
}

void CGameContext::OnPlayerCountUpdate(int Port, int PlayerCount)
{
	CPlayerCounter *pEnt = (CPlayerCounter *)m_World.FindFirst(CGameWorld::ENTTYPE_PLAYER_COUNTER);
	for (; pEnt; pEnt = (CPlayerCounter *)pEnt->TypeNext())
	{
		pEnt->OnUpdate(Port, PlayerCount);
	}
}

void CGameContext::SendPlayerCountUpdate(bool Shutdown)
{
	m_LastPlayerCountUpdate = Server()->Tick();
	Server()->SendPlayerCountUpdate(Shutdown);
}

int CGameContext::GetRedirectListPort(int WantedSwitchNumber)
{
	const char *pList = Config()->m_SvRedirectServerTilePorts;
	char aBuf[16];
	while ((pList = str_next_token(pList, ",", aBuf, sizeof(aBuf))))
	{
		int Switch = 0;
		int Port = 0;
		if (sscanf(aBuf, "%d:%d", &Switch, &Port) == 2 && Switch == WantedSwitchNumber)
		{
			return Port;
		}
	}
	return 0;
}

int CGameContext::GetRedirectListSwitch(int WantedPort)
{
	const char *pList = Config()->m_SvRedirectServerTilePorts;
	char aBuf[16];
	while ((pList = str_next_token(pList, ",", aBuf, sizeof(aBuf))))
	{
		int Switch = 0;
		int Port = 0;
		if (sscanf(aBuf, "%d:%d", &Switch, &Port) == 2 && Port == WantedPort)
		{
			return Switch;
		}
	}
	return 0;
}

void CGameContext::CreateFolders()
{
	char aBuf[IO_MAX_PATH_LENGTH] = { 0 };
	fs_makedir(Storage()->GetBinaryPath(Config()->m_SvAccFilePath, aBuf, sizeof(aBuf)));
	fs_makedir(Storage()->GetBinaryPath(Config()->m_SvDonationFilePath, aBuf, sizeof(aBuf)));
	fs_makedir(Storage()->GetBinaryPath(Config()->m_SvTopAccountsFilePath, aBuf, sizeof(aBuf)));
	fs_makedir(Storage()->GetBinaryPath(Config()->m_SvLanguagesPath, aBuf, sizeof(aBuf)));
	fs_makedir(Storage()->GetBinaryPath(Config()->m_SvCountriesFilePath, aBuf, sizeof(aBuf)));

	char aPath[IO_MAX_PATH_LENGTH];

	// plots
	fs_makedir(Storage()->GetBinaryPath(Config()->m_SvPlotFilePath, aBuf, sizeof(aBuf)));
	str_format(aPath, sizeof(aPath), "%s/%s", Config()->m_SvPlotFilePath, Server()->GetMapName());
	fs_makedir(Storage()->GetBinaryPath(aPath, aBuf, sizeof(aBuf)));

	str_format(aPath, sizeof(aPath), "%s/presets", Config()->m_SvPlotFilePath);
	fs_makedir(Storage()->GetBinaryPath(aPath, aBuf, sizeof(aBuf)));

	// money drops
	fs_makedir(Storage()->GetBinaryPath(Config()->m_SvMoneyDropsFilePath, aBuf, sizeof(aBuf)));
	str_format(aPath, sizeof(aPath), "%s/%s", Config()->m_SvMoneyDropsFilePath, Server()->GetMapName());
	fs_makedir(Storage()->GetBinaryPath(aPath, aBuf, sizeof(aBuf)));

	// map designs
	fs_makedir(Storage()->GetBinaryPath(Config()->m_SvMapDesignPath, aBuf, sizeof(aBuf)));
	str_format(aPath, sizeof(aPath), "%s/%s", Config()->m_SvMapDesignPath, Server()->GetMapName());
	fs_makedir(Storage()->GetBinaryPath(aPath, aBuf, sizeof(aBuf)));

	// money history
	str_format(aPath, sizeof(aPath), "dumps/%s", Config()->m_SvMoneyHistoryFilePath);
	Storage()->CreateFolder(aPath, IStorage::TYPE_SAVE);

	// saved tee
	str_format(aPath, sizeof(aPath), "dumps/%s", Config()->m_SvSavedTeesFilePath);
	Storage()->CreateFolder(aPath, IStorage::TYPE_SAVE);
	str_format(aPath, sizeof(aPath), "dumps/%s/%s", Config()->m_SvSavedTeesFilePath, Server()->GetMapName());
	Storage()->CreateFolder(aPath, IStorage::TYPE_SAVE);
	str_format(aPath, sizeof(aPath), "dumps/%s/x_redirect_tile", Config()->m_SvSavedTeesFilePath);
	Storage()->CreateFolder(aPath, IStorage::TYPE_SAVE);
}

int CGameContext::GetNextClientID()
{
	for (int i = 0; i < Config()->m_SvMaxClients; i++)
		if (((CServer *)Server())->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
			return i;
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
		{
			if (Server()->IsSevendown(i))
			{
				CMsgPacker Msg(5 + NUM_NETMSGTYPES); // NETMSGTYPE_SV_SOUNDGLOBAL
				Msg.AddInt(Sound);
				Server()->SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
			}
			else
				CreateSoundPlayer(Sound, i);
		}
}

void CGameContext::CreateSoundPlayer(int Sound, int ClientID)
{
	CreateSound(m_apPlayers[ClientID]->m_ViewPos, Sound, CmaskOne(ClientID));
}

void CGameContext::CreateSoundPlayerAt(vec2 Pos, int Sound, int ClientID)
{
	CreateSound(Pos, Sound, CmaskOne(ClientID));
}

bool CGameContext::IsLocal(int ClientID1, int ClientID2)
{
	if (ClientID1 == ClientID2 || ClientID1 < 0 || ClientID2 < 0)
		return true;

	CCharacter *p1 = GetPlayerChar(ClientID1);
	CCharacter *p2 = GetPlayerChar(ClientID2);

	if (!p1 || !p2 || p1->Team() != p2->Team())
		return false;

	float dx = p1->GetPos().x-p2->GetPos().x;
	float dy = p1->GetPos().y-p2->GetPos().y;

	if(absolute(dx) > 1000.0f || absolute(dy) > 800.0f)
		return false;

	if(distance(p1->GetPos(), p2->GetPos()) > 4000.0f)
		return false;

	return true;
}

bool CGameContext::CanReceiveMessage(int Sender, int Receiver)
{
	return m_apPlayers[Receiver] && (!m_apPlayers[Receiver]->m_LocalChat || IsLocal(Sender, Receiver));
}

bool CGameContext::IsMuted(int Sender, int Receiver)
{
	return m_apPlayers[Receiver] && Sender >= 0 && m_apPlayers[Receiver]->m_aMuted[Sender];
}

bool CGameContext::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHL = str_utf8_find_nocase(pLine, pName);
	if (pHL)
	{
		int Length = str_length(pName);
		if(Length > 0 && (pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == ':'))
			return true;
	}
	return false;
}

bool CGameContext::JailPlayer(int ClientID, int Seconds, int ModLogID)
{
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if (!pPlayer || Seconds <= 0)
		return false;

	// make sure we are not saved as killer for someone else after we got arrested, so we cant take the flag to the jail
	m_World.UnsetKiller(ClientID);

	pPlayer->m_JailTime = Server()->TickSpeed() * Seconds;
	pPlayer->m_EscapeTime = 0;
	if(pPlayer->GetCharacter())
	{
		pPlayer->KillCharacter(WEAPON_GAME);
		pPlayer->Respawn();
	}

	// Force destroyendtick to be 1, so it can get resetted in the next tick and the owner gets the message aswell
	int PlotID = m_Plots.GetPlotID(pPlayer->GetAccID());
	m_Plots.SetPlotDestroyEndTick(PlotID, 1);

	if (ModLogID != -1)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' was arrested for %d seconds", Server()->ClientName(ClientID), Seconds);
		Console()->Print(IConsole::OUTPUT_LEVEL_RESPONSE, "console", aBuf);
		SendModLogMessage(ModLogID, aBuf);
	}
	return true;
}

bool CGameContext::ForceJailRelease(int ClientID)
{
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if (!pPlayer || !pPlayer->m_JailTime)
		return false;

	pPlayer->m_JailTime = 1;
	SendChatTarget(ClientID, pPlayer->Localize("You were released from jail"));
	pPlayer->KillCharacter(WEAPON_GAME);
	return true;
}

void CGameContext::ProcessSpawnBlockProtection(int ClientID)
{
	CCharacter *pChr = GetPlayerChar(ClientID);
	if (!pChr)
		return;

	int Killer = pChr->Core()->m_Killer.m_ClientID;
	if (Killer < 0 || Killer == ClientID)
		return;

	CPlayer *pKiller = m_apPlayers[Killer];
	if (!pKiller || !pKiller->GetCharacter() || pKiller->m_IsDummy)
		return;

	if (IsSpawnArea(pKiller->GetCharacter()->GetPos()) && !Arenas()->FightStarted(Killer)) // if killer is in spawn area
	{
		pKiller->m_SpawnBlockScore++;
		if (Config()->m_SvSpawnBlockProtection)
		{
			if (pKiller->m_SpawnBlockScore > 5)
			{
				SendChatPoliceFormat(Localizable("'%s' is spawnblocking. Catch him!"), Server()->ClientName(Killer));
				SendChatTarget(Killer, pKiller->Localize("Police is searching you because of spawnblocking"));
				pKiller->m_EscapeTime += Server()->TickSpeed() * 120; // + 2 minutes escape time
			}
			else
			{
				SendChatTarget(Killer, pKiller->Localize("[WARNING] Spawnblocking is illegal"));
			}
		}
	}
}

bool CGameContext::IsSpawnArea(vec2 Pos)
{
	return (Pos.x >= Config()->m_SvSpawnAreaLowX * 32
		&& Pos.x <= Config()->m_SvSpawnAreaHighX * 32
		&& Pos.y >= Config()->m_SvSpawnAreaLowY * 32
		&& Pos.y <= Config()->m_SvSpawnAreaHighY * 32);
}

const char *CGameContext::AppendMotdFooter(const char *pMsg, const char *pFooter)
{
	static char aRet[900] = "";
	if (pMsg[0])
		str_format(aRet, sizeof(aRet), "%s\n\n%s", pMsg, pFooter);
	return aRet;

	/*MOTD_MAX_LINES ist jetzt 24, war vorher aber 22. Weiß nicht wieso das dann überhaupt alles funktioniert... Muss auf jedenfall angepasst werden. Needs a Rewrite :D
	static char aRet[900] = "";
	if (!pFooter[0])
	{
		str_copy(aRet, pMsg, sizeof(aRet));
		return aRet;
	}

	int FooterLines = 0;
	int MaxLinesWithoutFooter = MOTD_MAX_LINES;
	for (int i = 0, s = 0; i < str_length(pFooter) + 1; i++)
	{
		s++;
		if ((pFooter[i] == '\\' && pFooter[i+1] == 'n') || pFooter[i] == '\n' || s >= 35)
		{
			FooterLines++;
			MaxLinesWithoutFooter--;
			s = 0;
		}
	}

	char aMotd[900];
	str_copy(aMotd, pMsg, sizeof(aMotd));
	if (!aMotd[0])
		return "";

	int Lines = 0;
	int MotdLen = str_length(aMotd) + 1;
	for (int i = 0, s = 0; i < MotdLen; i++)
	{
		s++;
		if ((aMotd[i] == '\\' && aMotd[i+1] == 'n') || aMotd[i] == '\n' || s >= 35)
		{
			Lines++;
			s = 0;
		}
	}

	for (int i = MotdLen; i > 0; i--)
	{
		if ((aMotd[i-1] == '\\' && aMotd[i] == 'n') || aMotd[i] == '\n' || Lines > MaxLinesWithoutFooter)
		{
			aMotd[i] = '\0';
			aMotd[i - 1] = '\0';
			Lines--;
		}
		else
			break;
	}

	Lines = clamp(Lines, 0, MaxLinesWithoutFooter);

	char aNewLines[64] = "";
	for (int i = 0; i < MOTD_MAX_LINES-Lines; i++)
		str_append(aNewLines, "\n", sizeof(aNewLines));

	str_format(aRet, sizeof(aRet), "%s%s%s", aMotd, aNewLines, pFooter);
	return aRet;*/
}

const char *CGameContext::FormatMotd(const char *pMsg)
{
	char aFooter[128];
	str_format(aFooter, sizeof(aFooter), "F-DDrace is a mod by fokkonaut\nF-DDrace Mod. Ver.: %s", GAME_VERSION);
	return AppendMotdFooter(pMsg, aFooter);
}

const char *CGameContext::FormatURL(const char *pURL)
{
	static char aURL[256];
	for (int i = 0, s = 0; i < str_length(pURL) + 1; i++)
	{
#ifdef CONF_FAMILY_WINDOWS
		if (pURL[i] == '&')
			aURL[s++] = '^';
#endif
		aURL[s++] = pURL[i];
	}

	return aURL;
}

const char *CGameContext::FormatExperienceBroadcast(const char *pMsg, int ClientID)
{
	if (Server()->IsSevendown(ClientID))
		return pMsg;

	char pTextColor[5] = { '^', Config()->m_SvExpMsgColorText[0], Config()->m_SvExpMsgColorText[1], Config()->m_SvExpMsgColorText[2] };
	char pSymbolColor[5] = { '^', Config()->m_SvExpMsgColorSymbol[0], Config()->m_SvExpMsgColorSymbol[1], Config()->m_SvExpMsgColorSymbol[2] };
	char pValueColor[5] = { '^', Config()->m_SvExpMsgColorValue[0], Config()->m_SvExpMsgColorValue[1], Config()->m_SvExpMsgColorValue[2] };

	const int ColorOffset = 4;
	int s = ColorOffset;

	static char aRet[512];
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

void CGameContext::CalcScreenParams(float Aspect, float Zoom, float *w, float *h)
{
	const float Amount = 1150 * 1000;
	const float WMax = 1500;
	const float HMax = 1050;

	float f = sqrtf(Amount) / sqrtf(Aspect);
	*w = f * Aspect;
	*h = f;

	// limit the view
	if(*w > WMax)
	{
		*w = WMax;
		*h = *w / Aspect;
	}

	if(*h > HMax)
	{
		*h = HMax;
		*w = *h * Aspect;
	}

	*w *= Zoom;
	*h *= Zoom;
}

void CGameContext::SnapSelectedArea(CSelectedArea *pSelectedArea, const CSnapContext &Context)
{
	vec2 TopLeft = pSelectedArea->TopLeft();
	vec2 BottomRight = pSelectedArea->BottomRight();
	vec2 aPoints[4] = { TopLeft, vec2(BottomRight.x, TopLeft.y), BottomRight, vec2(TopLeft.x, BottomRight.y) };

	for (int i = 0; i < 4; i++)
	{
		int To = i == 3 ? 0 : i+1;
		SnapLaserObject(Context, pSelectedArea->m_aID[i], aPoints[i], aPoints[To], Server()->Tick() - 2, Context.ClientId(), LASERTYPE_RIFLE, -1, -1, LASERFLAG_NO_PREDICT);
	}
}

bool CGameContext::SnapLaserObject(const CSnapContext &Context, int SnapId, const vec2 &To, const vec2 &From, int StartTick, int Owner, int LaserType, int Subtype, int SwitchNumber, int Flags) const
{
	if(Context.GetClientVersion() >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser *pObj = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, SnapId, sizeof(CNetObj_DDNetLaser)));
		if(!pObj)
			return false;

		int TranslatedOwner = Owner;
		if (!Server()->Translate(TranslatedOwner, Context.ClientId()))
			TranslatedOwner = -1;

		pObj->m_ToX = round_to_int(To.x);
		pObj->m_ToY = round_to_int(To.y);
		pObj->m_FromX = round_to_int(From.x);
		pObj->m_FromY = round_to_int(From.y);
		pObj->m_StartTick = StartTick;
		pObj->m_Owner = TranslatedOwner;
		pObj->m_Type = LaserType;
		pObj->m_Subtype = Subtype;
		pObj->m_SwitchNumber = SwitchNumber;
		pObj->m_Flags = Flags;
	}
	else
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, SnapId, sizeof(CNetObj_Laser)));
		if(!pObj)
			return false;

		pObj->m_X = round_to_int(To.x);
		pObj->m_Y = round_to_int(To.y);
		pObj->m_FromX = round_to_int(From.x);
		pObj->m_FromY = round_to_int(From.y);
		pObj->m_StartTick = StartTick;
	}

	return true;
}

bool CGameContext::SnapPickupObject(const CSnapContext &Context, int SnapId, const vec2 &Pos, int Type, int SubType, int SwitchNumber, int Flags) const
{
	if (!Context.IsSevendown() || Context.GetClientVersion() < VERSION_DDNET_ENTITY_NETOBJS)
	{
		int Size = Context.IsSevendown() ? 4*4 : sizeof(CNetObj_Pickup);
		CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, SnapId, Size));
		if (!pP)
			return false;

		pP->m_X = round_to_int(Pos.x);
		pP->m_Y = round_to_int(Pos.y);
		if (Context.IsSevendown())
		{
			int RealSubtype = GetWeaponType(SubType);
			pP->m_Type = RealSubtype == WEAPON_NINJA ? POWERUP_NINJA : Type;
			((int*)pP)[3] = RealSubtype;
		}
		else
			pP->m_Type = GetPickupType(Type, SubType);
	}
	else
	{
		CNetObj_DDNetPickup *pPickup = static_cast<CNetObj_DDNetPickup*>(Server()->SnapNewItem(NETOBJTYPE_DDNETPICKUP, SnapId, sizeof(CNetObj_DDNetPickup)));
		if(!pPickup)
			return false;

		pPickup->m_X = round_to_int(Pos.x);
		pPickup->m_Y = round_to_int(Pos.y);
		int RealSubtype = GetWeaponType(SubType);
		pPickup->m_Subtype = RealSubtype;
		pPickup->m_Type = RealSubtype == WEAPON_NINJA ? POWERUP_NINJA : Type;
		pPickup->m_SwitchNumber = SwitchNumber;
		pPickup->m_Flags = Flags;
	}

	return true;
}

bool CGameContext::SnapPickup(const CSnapContext &Context, int SnapId, const vec2 &Pos, int Type, int SubType, int SwitchNumber, int Flags, int Special, int *apExtraIds) const
{
	if (Type == POWERUP_BATTERY)
	{
		CNetObj_Projectile* pProj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, SnapId, sizeof(CNetObj_Projectile)));
		if (!pProj)
			return false;

		pProj->m_X = round_to_int(Pos.x);
		pProj->m_Y = round_to_int(Pos.y);

		pProj->m_VelX = 0;
		pProj->m_VelY = 0;
		pProj->m_StartTick = 0;
		pProj->m_Type = WEAPON_LASER;
	}
	else
	{
		SnapPickupObject(Context, SnapId, Pos, Type, SubType, SwitchNumber, Flags);
	}

	bool Gun = (SubType == WEAPON_GUN && (Special&SPECIAL_JETPACK || Special&SPECIAL_TELEWEAPON)) || SubType == WEAPON_PROJECTILE_RIFLE || (SubType == WEAPON_HAMMER && (Special&SPECIAL_DOORHAMMER || Special&SPECIAL_PPROJECTILEHAMMER));
	bool Plasma = SubType == WEAPON_PLASMA_RIFLE || SubType == WEAPON_LIGHTSABER || SubType == WEAPON_PORTAL_RIFLE || SubType == WEAPON_TELE_RIFLE
		|| SubType == WEAPON_LIGHTNING_LASER || (SubType == WEAPON_LASER && Special&SPECIAL_TELEWEAPON) || (SubType == WEAPON_TASER && Type == POWERUP_WEAPON);
	bool Heart = SubType == WEAPON_HEART_GUN;
	bool Grenade = SubType == WEAPON_STRAIGHT_GRENADE || SubType == WEAPON_BALL_GRENADE || (SubType == WEAPON_GRENADE && Special&SPECIAL_TELEWEAPON);

	int ExtraBulletOffset = 30;
	int SpreadOffset = -20;
	if (Special&SPECIAL_SPREADWEAPON && (Gun || Plasma || Heart || Grenade))
		ExtraBulletOffset = 50;

	if (Special&SPECIAL_SPREADWEAPON)
	{
		for (int i = 1; i < 4; i++)
		{
			CNetObj_Projectile* pSpreadIndicator = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, apExtraIds[i], sizeof(CNetObj_Projectile)));
			if (!pSpreadIndicator)
				return false;

			pSpreadIndicator->m_X = round_to_int(Pos.x + SpreadOffset);
			pSpreadIndicator->m_Y = round_to_int(Pos.y - 30);
			pSpreadIndicator->m_Type = WEAPON_SHOTGUN;
			pSpreadIndicator->m_StartTick = 0;

			SpreadOffset += 20;
		}
	}

	if (Gun)
	{
		CNetObj_Projectile* pShotgunBullet = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, apExtraIds[0], sizeof(CNetObj_Projectile)));
		if (!pShotgunBullet)
			return false;

		pShotgunBullet->m_X = round_to_int(Pos.x);
		pShotgunBullet->m_Y = round_to_int(Pos.y - ExtraBulletOffset);
		pShotgunBullet->m_Type = WEAPON_SHOTGUN;
		pShotgunBullet->m_StartTick = 0;
	}
	else if (Plasma)
	{
		vec2 LaserPos = vec2(Pos.x, Pos.y - ExtraBulletOffset);
		int LaserType = (SubType == WEAPON_TASER || SubType == WEAPON_LIGHTNING_LASER) ? LASERTYPE_FREEZE : LASERTYPE_RIFLE;
		SnapLaserObject(Context, apExtraIds[0], LaserPos, LaserPos, Server()->Tick(), -1, LaserType, -1, -1, LASERFLAG_NO_PREDICT);
	}
	else if (Heart)
	{
		vec2 HeartPos = vec2(Pos.x, Pos.y - ExtraBulletOffset);
		SnapPickupObject(Context, apExtraIds[0], HeartPos, POWERUP_HEALTH, 0, -1, PICKUPFLAG_NO_PREDICT);
	}
	else if (Grenade)
	{
		CNetObj_Projectile* pProj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, apExtraIds[0], sizeof(CNetObj_Projectile)));
		if (!pProj)
			return false;

		pProj->m_X = round_to_int(Pos.x);
		pProj->m_Y = round_to_int(Pos.y - ExtraBulletOffset);
		pProj->m_StartTick = Server()->Tick() - 2;
		pProj->m_Type = WEAPON_GRENADE;
	}

	return true;
}

void CGameContext::ConnectDummy(int DummyMode, vec2 Pos)
{
	int DummyID = GetNextClientID();
	if (DummyID < 0 || DummyID >= MAX_CLIENTS || m_apPlayers[DummyID])
		return;

	CPlayer *pDummy = m_apPlayers[DummyID] = new(DummyID) CPlayer(this, DummyID, false, false, true);
	Server()->DummyJoin(DummyID);
	pDummy->SetDummyMode(DummyMode);
	pDummy->m_ForceSpawnPos = Pos;
	pDummy->m_Afk = false; // players are marked as afk when they first enter. dummies dont send real inputs, thats why we need to make them non-afk again

	if (DummyMode == DUMMYMODE_V3_BLOCKER && Collision()->TileUsed(TILE_MINIGAME_BLOCK))
		pDummy->m_Minigame = MINIGAME_BLOCK;
	else if ((DummyMode == DUMMYMODE_SHOP_DUMMY && Collision()->TileUsed(ENTITY_SHOP_DUMMY_SPAWN))
		|| (DummyMode == DUMMYMODE_PLOT_SHOP_DUMMY && Collision()->TileUsed(ENTITY_PLOT_SHOP_DUMMY_SPAWN))
		|| (DummyMode == DUMMYMODE_BANK_DUMMY && Collision()->TileUsed(ENTITY_BANK_DUMMY_SPAWN))
		|| (DummyMode == DUMMYMODE_TAVERN_DUMMY && Collision()->TileUsed(ENTITY_TAVERN_DUMMY_SPAWN))
		)
		pDummy->m_Minigame = MINIGAME_NONE;

	if (DummyMode == DUMMYMODE_TAVERN_DUMMY)
	{
		pDummy->m_TeeInfos = CTeeInfo(SKIN_TWINBOP);
	}
	else
	{
		pDummy->m_TeeInfos = CTeeInfo(SKIN_DUMMY);
	}

	dbg_msg("dummy", "Dummy connected: %d, Dummymode: %d", DummyID, DummyMode);
	OnClientEnter(DummyID);
}

bool CGameContext::IsHouseDummy(int ClientID, int Type)
{
	if (Type == -1)
	{
		for (int i = 0; i < NUM_HOUSES; i++)
			if (IsHouseDummy(ClientID, i))
				return true;
		return false;
	}

	int Mode = 0;
	switch (Type)
	{
	case HOUSE_SHOP: Mode = DUMMYMODE_SHOP_DUMMY; break;
	case HOUSE_PLOT_SHOP: Mode = DUMMYMODE_PLOT_SHOP_DUMMY; break;
	case HOUSE_BANK: Mode = DUMMYMODE_BANK_DUMMY; break;
	case HOUSE_TAVERN: Mode = DUMMYMODE_TAVERN_DUMMY; break;
	}
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetDummyMode() == Mode;
}

int CGameContext::GetHouseDummy(int Type)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
		if (IsHouseDummy(i, Type))
			return i;
	return -1;
}

void CGameContext::ConnectHouseDummy(int Type, bool SpawnTileOnly)
{
	int Index, SpawnTile, Dummymode;
	switch (Type)
	{
	case HOUSE_SHOP: Index = TILE_SHOP; SpawnTile = ENTITY_SHOP_DUMMY_SPAWN; Dummymode = DUMMYMODE_SHOP_DUMMY; break;
	case HOUSE_PLOT_SHOP: Index = TILE_PLOT_SHOP; SpawnTile = ENTITY_PLOT_SHOP_DUMMY_SPAWN; Dummymode = DUMMYMODE_PLOT_SHOP_DUMMY; break;
	case HOUSE_BANK: Index = TILE_BANK; SpawnTile = ENTITY_BANK_DUMMY_SPAWN; Dummymode = DUMMYMODE_BANK_DUMMY; break;
	case HOUSE_TAVERN: Index = TILE_TAVERN; SpawnTile = ENTITY_TAVERN_DUMMY_SPAWN; Dummymode = DUMMYMODE_TAVERN_DUMMY; break;
	default: return;
	}

	if (GetHouseDummy(Type) == -1 && Collision()->TileUsed(Index))
	{
		vec2 Pos = vec2(-1, -1);
		if (Collision()->TileUsed(SpawnTile))
		{
			Pos = Collision()->GetRandomTile(SpawnTile);
		}
		else if (SpawnTileOnly)
		{
			// Don't automatically connect house bots if no spawn tile has been found, only with sv_default_dummies enabled
			return;
		}
		ConnectDummy(Dummymode, Pos);
	}
}

void CGameContext::ConnectDefaultDummies()
{
	if (!str_comp(Server()->GetMapName(), "ChillBlock5"))
	{
		ConnectDummy(DUMMYMODE_CHILLBLOCK5_POLICE, vec2(485*32.f, 235*32.f));
		ConnectDummy(DUMMYMODE_CHILLBLOCK5_BLOCKER);
		ConnectDummy(DUMMYMODE_CHILLBLOCK5_BLOCKER);
		ConnectDummy(DUMMYMODE_CHILLBLOCK5_RACER);
	}
	else if (!str_comp(Server()->GetMapName(), "BlmapChill"))
	{
		ConnectDummy(DUMMYMODE_BLMAPCHILL_POLICE);
	}
	else if (!str_comp(Server()->GetMapName(), "blmapV3RoyalX"))
	{
		ConnectDummy(DUMMYMODE_V3_BLOCKER);
	}

	if (Collision()->TileUsed(TILE_MINIGAME_BLOCK))
		ConnectDummy(DUMMYMODE_V3_BLOCKER);

	for (int i = 0; i < NUM_HOUSES; i++)
		ConnectHouseDummy(i);
}

void CGameContext::ConsoleIsDummyCallback(int ClientID, bool *pIsDummy, void *pUser)
{
	CGameContext* pSelf = (CGameContext*)pUser;
	*pIsDummy = pSelf->m_apPlayers[ClientID] && pSelf->m_apPlayers[ClientID]->m_IsDummy;
}

void CGameContext::ConsoleIsInViewCallback(int ClientID, int CallerID, bool *pIsInView, void *pUser)
{
	CGameContext* pSelf = (CGameContext*)pUser;
	*pIsInView = false;
	if (CallerID < 0 || !pSelf->m_apPlayers[CallerID] || !pSelf->GetPlayerChar(ClientID))
		return;

	vec2 CheckPos = pSelf->GetPlayerChar(ClientID)->GetPos();
	vec2 ShowDistance = pSelf->m_apPlayers[CallerID]->m_ShowDistance;
	float dx = pSelf->m_apPlayers[CallerID]->m_ViewPos.x-CheckPos.x;
	if (absolute(dx) > ShowDistance.x / 2.f)
	{
		return;
	}
	float dy = pSelf->m_apPlayers[CallerID]->m_ViewPos.y-CheckPos.y;
	if (absolute(dy) > ShowDistance.y / 2.f)
	{
		return;
	}
	*pIsInView = true;
}

void CGameContext::SetMapSpecificOptions()
{
	if (!str_comp(Server()->GetMapName(), "ChillBlock5"))
	{
		Config()->m_SvV3OffsetX = 374;
		Config()->m_SvV3OffsetY = 59;
	}
	else if (!str_comp(Server()->GetMapName(), "blmapV3RoyalX"))
	{
		Config()->m_SvV3OffsetX = 97;
		Config()->m_SvV3OffsetY = 19;
	}
	else if (!str_comp(Server()->GetMapName(), "BlmapChill"))
	{
		Config()->m_SvV3OffsetX = 696;
		Config()->m_SvV3OffsetY = 617;

		Config()->m_SvSpawnAreaLowX = 5;
		Config()->m_SvSpawnAreaLowY = 4;
		Config()->m_SvSpawnAreaHighX = 48;
		Config()->m_SvSpawnAreaHighY = 48;
	}
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

		int Team = m_apPlayers[UpdateID]->GetHidePlayerTeam(i);

		// only update the team when its not the same as before
		if (m_apPlayers[i]->m_HidePlayerTeam[UpdateID] == Team)
			continue;

		m_apPlayers[i]->m_HidePlayerTeam[UpdateID] = Team;

		SendTeamChange(UpdateID, Team, true, Server()->Tick(), i);
	}
}

void CGameContext::OnSetTimedOut(int ClientID, int OrigID)
{
	m_PlayerMapping.InitPlayerMap(ClientID, true, true);
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
	case WEAPON_PORTAL_RIFLE:
		return "Portal Rifle";
	case WEAPON_PROJECTILE_RIFLE:
		return "Projectile Rifle";
	case WEAPON_BALL_GRENADE:
		return "Ball Greande";
	case WEAPON_DRAW_EDITOR:
		return "Draw Editor";
	case WEAPON_TELE_RIFLE:
		return "Tele Rifle";
	case WEAPON_LIGHTNING_LASER:
		return "Lightning Laser";
	}
	return "Unknown";
}

int CGameContext::GetWeaponType(int Weapon) const
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
	case WEAPON_PORTAL_RIFLE:
		return WEAPON_LASER;
	case WEAPON_PROJECTILE_RIFLE:
		return WEAPON_LASER;
	case WEAPON_BALL_GRENADE:
		return WEAPON_GRENADE;
	case WEAPON_DRAW_EDITOR:
		return WEAPON_NINJA;
	case WEAPON_TELE_RIFLE:
		return WEAPON_LASER;
	case WEAPON_LIGHTNING_LASER:
		return WEAPON_LASER;
	}
	return Weapon;
}

int CGameContext::GetProjectileType(int Weapon) const
{
	switch (Weapon)
	{
	case WEAPON_STRAIGHT_GRENADE:
		return WEAPON_GRENADE;
	case WEAPON_PROJECTILE_RIFLE:
		return WEAPON_GUN;
	case WEAPON_BALL_GRENADE:
		return WEAPON_GRENADE;
	}
	return Weapon;
}

int CGameContext::GetPickupType(int Type, int Subtype) const
{
	if (Type == POWERUP_BATTERY)
		return PICKUP_LASER;
	if (Type == POWERUP_NINJA)
		return PICKUP_NINJA;
	if (Type != POWERUP_WEAPON)
		return Type;

	Subtype = GetWeaponType(Subtype);
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

bool CGameContext::IsValidSpreadWeapon(int Type)
{
	return !(Type == WEAPON_HAMMER || Type == WEAPON_NINJA || Type == WEAPON_TELEKINESIS || Type == WEAPON_LIGHTSABER || Type == WEAPON_PORTAL_RIFLE
		|| Type == WEAPON_DRAW_EDITOR || Type == WEAPON_TELE_RIFLE || Type == WEAPON_LIGHTNING_LASER);
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
		if (Extra == JETPACK || Extra == ATOM || Extra == TRAIL || Extra == METEOR || Extra == INF_METEOR || Extra == SCROLL_NINJA || Extra == HOOK_POWER|| Extra == SPREAD_WEAPON
			|| Extra == FREEZE_HAMMER || Extra == ITEM || Extra == TELE_WEAPON || Extra == DOOR_HAMMER || Extra == PROJECTILE_HAMMER || Extra == ROTATING_BALL || Extra == EPIC_CIRCLE || Extra == STAFF_IND)
			str_format(aMsg, sizeof(aMsg), "You %s %s", Set ? "have a" : "lost your", aItem);
		else if (Extra == VANILLA_MODE || Extra == DDRACE_MODE)
			str_format(aMsg, sizeof(aMsg), "You are now in %s", aItem);
		else if (Extra == PASSIVE || Extra == SNAKE)
			str_format(aMsg, sizeof(aMsg), "You are %s in %s", Set ? "now" : "no longer", aItem);
		else if (Extra == ENDLESS_HOOK || Extra == INFINITE_JUMPS)
			str_format(aMsg, sizeof(aMsg), "%s %s been %s", aItem, Extra == INFINITE_JUMPS ? "have" : "has", Set ? "activated" : "deactivated");
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
		return "Infinite Jumps";
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
	case PROJECTILE_HAMMER:
		return "Projectile Hammer";
	case TEE_CONTROL:
		return "Tee Control";
	case SNAKE:
		return "Snake Mode";
	case LOVELY:
		return "Lovely";
	case ROTATING_BALL:
		return "Rotating Ball";
	case EPIC_CIRCLE:
		return "Epic Circle";
	case STAFF_IND:
		return "Staff Indicator";
	case RAINBOW_NAME:
		return "Rainbow Name";
	case CONFETTI:
		return "Confetti";
	case SPARKLE:
		return "Sparkle";
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

const char *CGameContext::GetScoreModeName(int ScoreMode)
{
	switch (ScoreMode)
	{
	case SCORE_TIME:
		return "Time";
	case SCORE_LEVEL:
		return "Level";
	case SCORE_BLOCK_POINTS:
		return "Block Points";
	case SCORE_BONUS:
		return "No-Bonus Score";
	}
	return "Unknown";
}

const char *CGameContext::GetScoreModeCommand(int ScoreMode)
{
	switch (ScoreMode)
	{
	case SCORE_TIME:
		return "time";
	case SCORE_LEVEL:
		return "level";
	case SCORE_BLOCK_POINTS:
		return "points";
	case SCORE_BONUS:
		return "bonus";
	}
	return "Unknown";
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
	case MINIGAME_1VS1:
		return "1vs1";
	case MINIGAME_DURAK:
		return "Durák";
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
	case MINIGAME_1VS1:
		return "1vs1";
	case MINIGAME_DURAK:
		return "durak";
	}
	return "unknown";
}

void CGameContext::SetMinigame(int ClientID, int Minigame, bool Force, bool DoChatMsg)
{
	CPlayer *pPlayer = m_apPlayers[ClientID];
	if (!pPlayer)
		return;

	char aMsg[128];

	// check whether minigame is disabled
	if (Minigame != MINIGAME_NONE && m_aMinigameDisabled[Minigame])
	{
		if (DoChatMsg)
			SendChatTarget(ClientID, pPlayer->Localize("This minigame is disabled"));
		return;
	}

	// check if we are already in a minigame
	if (pPlayer->m_Minigame == Minigame)
	{
		// you can't leave when you're not in a minigame
		if (DoChatMsg)
		{
			if (Minigame == MINIGAME_NONE)
				SendChatTarget(ClientID, pPlayer->Localize("You are not in a minigame"));
			else
			{
				str_format(aMsg, sizeof(aMsg), pPlayer->Localize("You are already in minigame '%s'"), GetMinigameName(Minigame));
				SendChatTarget(ClientID, aMsg);
			}
		}
		return;
	}

	if (!Force)
	{
		if (Minigame == MINIGAME_DURAK && !Collision()->TileUsed(TILE_DURAK_LOBBY))
		{
			SendChatTarget(ClientID, pPlayer->Localize("This map has no Durák lobby, you have to find your way to the table yourself"));
			return;
		}
		else if (pPlayer->RequestMinigameChange(Minigame))
		{
			return;
		}
	}

	// leave minigame
	if (Minigame == MINIGAME_NONE)
	{
		if (DoChatMsg)
		{
			SendChatFormat(-1, CHAT_ALL, -1, CHATFLAG_ALL, Localizable("'%s' left the minigame '%s'"), Server()->ClientName(ClientID), GetMinigameName(pPlayer->m_Minigame));
		}

		//reset everything
		if (pPlayer->m_Minigame == MINIGAME_SURVIVAL)
		{
			Survival()->OnPlayerLeave(ClientID);
		}
		else if (pPlayer->m_Minigame == MINIGAME_1VS1)
		{
			Arenas()->OnPlayerLeave(ClientID);
		}
		else if (pPlayer->m_Minigame == MINIGAME_DURAK)
		{
			Durak()->OnPlayerLeave(ClientID);
		}
	}
	// join minigame
	else if (!pPlayer->IsMinigame())
	{
		if (DoChatMsg)
		{
			SendChatFormat(-1, CHAT_ALL, -1, CHATFLAG_ALL, Localizable("'%s' joined the minigame '%s', use '/%s' to join aswell"),
				Server()->ClientName(ClientID), GetMinigameName(Minigame), GetMinigameCommand(Minigame));
			SendChatTarget(ClientID, pPlayer->Localize("Say '/leave' to join the normal area again"));
		}

		// Save character stats to reload them after leaving
		pPlayer->SaveMinigameTee();

		//set minigame required stuff
		if (Minigame != MINIGAME_DURAK)
		{
			((CGameControllerDDRace *)m_pController)->m_Teams.SetForceCharacterTeam(ClientID, 0);
		}

		if (Minigame == MINIGAME_SURVIVAL)
		{
			Survival()->OnPlayerJoin(ClientID);
		}
		else if (Minigame == MINIGAME_1VS1)
		{
			Arenas()->OnPlayerJoin(ClientID);
		}
	}
	else
	{
		// you can't join minigames if you are already in another mingame
		SendChatTarget(ClientID, pPlayer->Localize("You have to leave first in order to join another minigame"));
		return;
	}

	pPlayer->KillCharacter(WEAPON_MINIGAME_CHANGE);
	pPlayer->m_Minigame = Minigame;
	pPlayer->SetPlaying();
	pPlayer->m_LastMovementTick = Server()->Tick();

	UpdateHidePlayers();

	// Update the gameinfo, add or remove GAMEFLAG_RACE as wanted (in minigames we disable it to properly show the scores)
	m_pController->UpdateGameInfo(ClientID);
}

void CGameContext::InstagibTick(int Type)
{
	Type = Type == 0 ? MINIGAME_INSTAGIB_BOOMFNG : MINIGAME_INSTAGIB_FNG;
	m_aMinigameDisabled[Type] = true;

	// if there are no spawn tiles, we cant play the game
	if (!m_aMinigameDisabled[Type] && !Collision()->TileUsed(Type == MINIGAME_INSTAGIB_BOOMFNG ? ENTITY_SPAWN_RED : ENTITY_SPAWN_BLUE))
	{
		m_aMinigameDisabled[Type] = true;
		return;
	}

	//m_apPlayers[Winner]->GiveXP(250, "for winning an instagib round");

	// add instagib here
}
