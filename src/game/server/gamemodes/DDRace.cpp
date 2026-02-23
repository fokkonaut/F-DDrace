#include <engine/server.h>
#include <engine/shared/config.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include "DDRace.h"
#include "gamemode.h"

// following functions based on src/game/client/gameclient.h + some optimizations
inline float HueToRgb(const float & v1, const float & v2, float h)
{
	if (h < 0.0f) h += 1;
	if (h > 1.0f) h -= 1;
	if ((6.0f * h) < 1.0f) return v1 + (v2 - v1) * 6.0f * h;
	if ((2.0f * h) < 1.0f) return v2;
	if ((3.0f * h) < 2.0f) return v1 + (v2 - v1) * ((2.0f / 3.0f) - h) * 6.0f;
	return v1;
}
inline vec3 HslToRgb(const vec3 & HSL)
{
	if (HSL.s == 0.0f)
		return vec3(HSL.l, HSL.l, HSL.l);
	else
	{
		float v2 = HSL.l < 0.5f ? HSL.l * (1.0f + HSL.s) : (HSL.l + HSL.s) - (HSL.s*HSL.l);
		float v1 = 2.0f * HSL.l - v2;
		return vec3(HueToRgb(v1, v2, HSL.h + (1.0f / 3.0f)), HueToRgb(v1, v2, HSL.h), HueToRgb(v1, v2, HSL.h - (1.0f / 3.0f)));
	}
}
// Attention, getting the true HSL value involves retrieving
// the method from: src/game/client/components/skins.cpp -> GetColorV3
const int CGameControllerDDRace::ZombieBodyValue = 0x44ff00;
const int CGameControllerDDRace::ZombieFeetValue = 0x0;
const vec3 CGameControllerDDRace::ZombieBodyHSL =
vec3(static_cast<float>((CGameControllerDDRace::ZombieBodyValue >> 16) & 0xff) / 255.f,
	static_cast<float>((CGameControllerDDRace::ZombieBodyValue >> 8) & 0xff) / 255.f,
	0.5f + static_cast<float>((CGameControllerDDRace::ZombieBodyValue) & 0xff) / 255.f*0.5f);

const vec3 CGameControllerDDRace::ZombieFeetHSL =
vec3(static_cast<float>((CGameControllerDDRace::ZombieFeetValue >> 16) & 0xff) / 255.f,
	static_cast<float>((CGameControllerDDRace::ZombieFeetValue >> 8) & 0xff) / 255.f,
	0.5f + static_cast<float>((CGameControllerDDRace::ZombieFeetValue) & 0xff) / 255.f*0.5f);

static const vec3 zombieBodyColor(HslToRgb(CGameControllerDDRace::ZombieBodyHSL));
static const vec3 zombieFeetColor(HslToRgb(CGameControllerDDRace::ZombieFeetHSL));


CGameControllerDDRace::CGameControllerDDRace(class CGameContext *pGameServer) :
		IGameController(pGameServer), m_Teams(pGameServer)
{
	m_apFlags[0] = 0;
	m_apFlags[1] = 0;
	m_ZombieTick = Server()->Tick();

	m_pGameType = Config()->m_SvTestingCommands ? TEST_NAME : GAME_NAME;

	InitTeleporter();
}

CGameControllerDDRace::~CGameControllerDDRace()
{
	// Nothing to clean
}

void CGameControllerDDRace::Tick()
{
	IGameController::Tick();

	if (Server()->Tick() > m_ZombieTick + Server()->TickSpeed() * 1)
	{
		m_ZombieTick = Server()->Tick();

		for (int i = 0, j = 0; i < MAX_CLIENTS; i++)
		{
			CPlayer *pPlayer = GameServer()->m_apPlayers[i];
			if (pPlayer && Server()->ClientIngame(pPlayer->GetCID()) && pPlayer->GetTeam() != TEAM_SPECTATORS)
			{
				if (Server()->Tick() > pPlayer->m_LastCustomColorsCheckTick + Server()->TickSpeed() * 5)
				{
					pPlayer->m_LastCustomColorsCheckTick = Server()->Tick();

					if (pPlayer->m_DisableCustomColorsTick && pPlayer->m_DisableCustomColorsTick + Server()->TickSpeed() * 11 < Server()->Tick())
					{
						pPlayer->m_DisableCustomColorsTick = 0;
						GameServer()->SendSkinChange(pPlayer->m_TeeInfos, i, -1);
					}
					bool SetDefaultColors = false;
					if (pPlayer->m_TeeInfos.m_Sevendown.m_UseCustomColor)
					{
						const int & b = pPlayer->m_TeeInfos.m_Sevendown.m_ColorBody;
						const int & f = pPlayer->m_TeeInfos.m_Sevendown.m_ColorFeet;
						float bodyHue = ((b >> 16) & 0xff) / 255.f;
						if (bodyHue < ZombieBodyHSL.h)
						{
							bodyHue = mix(bodyHue, ZombieBodyHSL.h, 0.15f);
						}
						else if (bodyHue > ZombieBodyHSL.h)
						{
							bodyHue = mix(bodyHue, ZombieBodyHSL.h, 0.3f);
						}

						float bodySaturation = ((b >> 8) & 0xff) / 255.f;
						if (bodySaturation > 0.15f)
						{
							bodySaturation = mix(bodySaturation, ZombieBodyHSL.s, 0.5f);
						}

						float bodyLight = 0.5f + (b & 0xff) / 255.f*0.5f;
						if (bodyLight < 0.85f)
						{
							bodyLight = mix(bodyLight, ZombieBodyHSL.l, 0.3f);
						}

						const vec3 bodyColor(HslToRgb(vec3(bodyHue, bodySaturation, bodyLight)));
						float dist_a = distance(zombieBodyColor, bodyColor);
						if (dist_a <= 0.501f)
						{
							SetDefaultColors = true;
						}
						else if (dist_a <= 0.531f)
						{
							const vec3 feetColor(HslToRgb(vec3(
								((f >> 16) & 0xff) / 255.f,
								((f >> 8) & 0xff) / 255.f,
								0.5f + (f & 0xff) / 255.f*0.5f)));
							float dist_b = distance(zombieFeetColor, feetColor);
							if (dist_b <= 0.561f)
							{
								SetDefaultColors = true;
							}
						}

						if (SetDefaultColors)
						{
							pPlayer->m_DisableCustomColorsTick = Server()->Tick();
							if (!str_comp(pPlayer->m_TeeInfos.m_Sevendown.m_SkinName, "cammo"))
							{
								CTeeInfo Info = pPlayer->m_TeeInfos;
								Info.m_Sevendown.m_UseCustomColor = 1;
								Info.m_Sevendown.m_ColorBody = 0xffffff;
								Info.m_Sevendown.m_ColorFeet = 0xffffff;
								Info.Translate(true);
								GameServer()->SendSkinChange(Info, i, -1);
							}
						}

						if (++j >= 5)
							break;
					}
				}
			}
		}
	}
}

void CGameControllerDDRace::InitTeleporter()
{
	if (!GameServer()->Collision()->Layers()->TeleLayer())
		return;
	int Width = GameServer()->Collision()->Layers()->TeleLayer()->m_Width;
	int Height = GameServer()->Collision()->Layers()->TeleLayer()->m_Height;

	for (int i = 0; i < Width * Height; i++)
	{
		int Number = GameServer()->Collision()->TeleLayer()[i].m_Number;
		int Type = GameServer()->Collision()->TeleLayer()[i].m_Type;
		if (Number > 0)
		{
			if (Type == TILE_TELEOUT || Type == TILE_TELE_INOUT || Type == TILE_TELE_INOUT_EVIL)
			{
				m_TeleOuts[Number - 1].push_back(
						vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}
			else if (Type == TILE_TELECHECKOUT)
			{
				m_TeleCheckOuts[Number - 1].push_back(
						vec2(i % Width * 32 + 16, i / Width * 32 + 16));
			}

			if (Number > GameServer()->Collision()->m_NumTeleporters)
				GameServer()->Collision()->m_NumTeleporters = Number;
		}
	}
}

// F-DDrace

bool CGameControllerDDRace::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	if(IGameController::OnEntity(Index, Pos, Layer, Flags, Number))
		return true;

	int Team = -1;
	if (Index == ENTITY_FLAGSTAND_RED && (Layer == LAYER_GAME || Layer == LAYER_FRONT)) Team = TEAM_RED;
	if (Index == ENTITY_FLAGSTAND_BLUE && (Layer == LAYER_GAME || Layer == LAYER_FRONT)) Team = TEAM_BLUE;
	if (Team == -1 || m_apFlags[Team])
		return false;

	m_GameFlags |= GAMEFLAG_FLAGS;

	CFlag *F = new CFlag(&GameServer()->m_World, Team, Pos);
	m_apFlags[Team] = F;
	return true;
}

int CGameControllerDDRace::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	int HadFlag = 0;

	// drop flags
	for (int i = 0; i < 2; i++)
	{
		CFlag *F = m_apFlags[i];
		if (!F || !pKiller || !pKiller->GetCharacter())
			continue;

		if (F->GetCarrier() == pKiller->GetCharacter())
			HadFlag |= 2;
		if (F->GetCarrier() == pVictim)
		{
			/* disallow instant flag owner changes through killers for now, cuz people can take flags into the vip+ room or between the big plot doors
			if (HasFlag(pKiller->GetCharacter()) == -1 && pKiller->m_Minigame == pVictim->GetPlayer()->m_Minigame && !pKiller->m_JailTime) // avoid giving flags to unreachable places
				ChangeFlagOwner(pVictim, pKiller->GetCharacter());
			else*/
			F->Drop();
			HadFlag |= 1;
		}
	}

	return HadFlag;
}

void CGameControllerDDRace::ChangeFlagOwner(CCharacter *pOldCarrier, CCharacter *pNewCarrier)
{
	for (int i = 0; i < 2; i++)
	{
		CFlag *F = m_apFlags[i];

		if (!F || !pNewCarrier)
			return;

		if (F->GetCarrier() == pOldCarrier && HasFlag(pNewCarrier) == -1)
			F->Grab(pNewCarrier->GetPlayer()->GetCID());
	}
}

void CGameControllerDDRace::ForceFlagOwner(int ClientID, int Team, bool PreventTeleport)
{
	CFlag *F = m_apFlags[Team];
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if (!F || (Team != TEAM_RED && Team != TEAM_BLUE) || (!pChr && ClientID >= 0))
		return;
	if (ClientID >= 0 && HasFlag(pChr) == -1)
	{
		if (F->GetCarrier())
			F->SetLastCarrier(F->GetCarrier()->GetPlayer()->GetCID());
		F->Grab(ClientID, PreventTeleport);
		F->SetPos(pChr->GetPos());
		F->SetPrevPos(pChr->GetPos());
	}
	else if (ClientID == -1)
		F->Reset();
}

int CGameControllerDDRace::HasFlag(CCharacter *pChr)
{
	if (!pChr)
		return -1;

	for (int i = 0; i < 2; i++)
	{
		if (!m_apFlags[i])
			continue;

		if (m_apFlags[i]->GetCarrier() == pChr)
			return i;
	}
	return -1;
}

void CGameControllerDDRace::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameDataFlag *pGameDataFlag = static_cast<CNetObj_GameDataFlag*>(Server()->SnapNewItem(NETOBJTYPE_GAMEDATAFLAG, 0, sizeof(CNetObj_GameDataFlag)));
	if (!pGameDataFlag)
		return;

	int FlagDropTickRed = 0;
	int FlagDropTickBlue = 0;
	int FlagCarrierRed;
	int FlagCarrierBlue;

	if (m_apFlags[TEAM_RED])
	{
		if (m_apFlags[TEAM_RED]->IsAtStand())
			FlagCarrierRed = FLAG_ATSTAND;
		else if (m_apFlags[TEAM_RED]->GetCarrier() && m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer())
			FlagCarrierRed = m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer()->GetCID();
		else
		{
			FlagCarrierRed = FLAG_TAKEN;
			FlagDropTickRed = m_apFlags[TEAM_RED]->GetDropTick();
		}
	}
	else
		FlagCarrierRed = FLAG_MISSING;

	if (m_apFlags[TEAM_BLUE])
	{
		if (m_apFlags[TEAM_BLUE]->IsAtStand())
			FlagCarrierBlue = FLAG_ATSTAND;
		else if (m_apFlags[TEAM_BLUE]->GetCarrier() && m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer())
			FlagCarrierBlue = m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer()->GetCID();
		else
		{
			FlagCarrierBlue = FLAG_TAKEN;
			FlagDropTickBlue = m_apFlags[TEAM_BLUE]->GetDropTick();
		}
	}
	else
		FlagCarrierBlue = FLAG_MISSING;

	if (SnappingClient > -1 && FlagCarrierRed >= 0 && !Server()->Translate(FlagCarrierRed, SnappingClient))
		FlagCarrierRed = FLAG_TAKEN;
	if (SnappingClient > -1 && FlagCarrierBlue >= 0 && !Server()->Translate(FlagCarrierBlue, SnappingClient))
		FlagCarrierBlue = FLAG_TAKEN;

	if (Server()->IsSevendown(SnappingClient))
	{
		((int*)pGameDataFlag)[0] = 0;
		((int*)pGameDataFlag)[1] = 0;
		((int*)pGameDataFlag)[2] = FlagCarrierRed;
		((int*)pGameDataFlag)[3] = FlagCarrierBlue;
	}
	else
	{
		pGameDataFlag->m_FlagCarrierRed = FlagCarrierRed;
		pGameDataFlag->m_FlagCarrierBlue = FlagCarrierBlue;
		pGameDataFlag->m_FlagDropTickRed = FlagDropTickRed;
		pGameDataFlag->m_FlagDropTickBlue = FlagDropTickBlue;
	}
}
