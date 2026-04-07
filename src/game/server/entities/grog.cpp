// made by fokkonaut

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "grog.h"
#include <game/server/teams.h>
#include "character.h"
#include <generated/server_data.h>

CGrog::CGrog(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_GROG, Pos, vec2(8, 20), Owner)
{
	m_Owner = Owner;
	m_Direction = -1;
	m_LastDirChange = 0;
	m_NumSips = 0;
	m_Lifetime = -1;
	m_ProcessedNudge = false;
	m_LastNudgePos = vec2(-1, -1);
	SetFlags(EFlags::CHECK_DEATH | EFlags::CHECK_GAME_LAYER_CLIPPED, false);

	vec2 aOffsets[NUM_GROG_LINES][2] = {
		{ vec2(0, 0), vec2(0, -40) },
		{ vec2(8, -20), vec2(16, -20) },
		{ vec2(8, 0), vec2(-8, 0) },
		{ vec2(-8, -40), vec2(-8, 0), },
		{ vec2(8, -40), vec2(8, 0) }
	};

	// Sort ids, so that liquid will always be behind glass lines
	int aID[NUM_GROG_LINES];
	for (int i = 0; i < NUM_GROG_LINES; i++)
		aID[i] = Server()->SnapNewID();
	std::sort(std::begin(aID), std::end(aID));

	for (int i = 0; i < NUM_GROG_LINES; i++)
	{
		m_aLines[i].m_From = aOffsets[i][0];
		m_aLines[i].m_To = aOffsets[i][1];
		m_aLines[i].m_ID = aID[i];
	}

	GameWorld()->InsertEntity(this);
}

CGrog::~CGrog()
{
	for (int i = 0; i < NUM_GROG_LINES; i++)
		Server()->SnapFreeID(m_aLines[i].m_ID);
}

void CGrog::Reset(bool CreateDeath)
{
	if (CreateDeath)
		GameServer()->CreateDeath(m_Pos, m_Owner, m_TeamMask);
	CAdvancedEntity::Reset();
}

void CGrog::DecreaseNumGrogsHolding()
{
	if (!GetOwner())
		return;

	GetOwner()->m_NumGrogsHolding--;
	GetOwner()->m_pGrog = GetOwner()->m_NumGrogsHolding ? new CGrog(GameWorld(), GetOwner()->GetPos(), m_Owner) : 0;
	GetOwner()->UpdateWeaponIndicator();

	if (!GetOwner()->m_pGrog)
	{
		GetOwner()->SetWeapon(WEAPON_HAMMER);
	}
}

void CGrog::OnSip()
{
	m_NumSips++;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, m_TeamMask);
	GetOwner()->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed() * 2);

	if (m_NumSips >= NUM_GROG_SIPS)
	{
		GetOwner()->IncreasePermille(3);
		DecreaseNumGrogsHolding();
		Reset(false);
		return;
	}
}

bool CGrog::Drop(float Dir, bool OnDeath)
{
	// Can't drop a grog which you started drinking. don't spread viruses. Drink it up first. If you drank before sharing with your friends, you're a donkey!
	if (m_NumSips)
	{
		// Still remove the grog from being held and remove it
		if (OnDeath)
		{
			DecreaseNumGrogsHolding();
			Reset();
		}
		return false;
	}

	// Dont allow dropping in wall
	if (GameServer()->Collision()->IsSolid(round_to_int(m_Pos.x), round_to_int(m_Pos.y)))
		m_Pos = GetOwner()->GetPos();

	// Remove after 5 min of being dropped
	m_Lifetime = Server()->TickSpeed() * 300;
	m_PickupDelay = Server()->TickSpeed() * (Config()->m_SvDropsPickupDelay / 1000.f);
	Dir = Dir == -3 ? 2.5f*GetOwner()->GetAimDir() : Dir;
	m_Vel = vec2(Dir, -4);
	DecreaseNumGrogsHolding();
	GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, m_TeamMask);
	SetFlags(EFlags::CHECK_DEATH | EFlags::CHECK_GAME_LAYER_CLIPPED, true);
	return true;
}

void CGrog::Tick()
{
	CAdvancedEntity::Tick();

	if (m_Lifetime == -1)
	{
		if (!GetOwner())
		{
			Reset();
			return;
		}

		int Dir = GetOwner()->GetAimDir();
		m_Pos = GetOwner()->GetPos();
		m_Pos.x += 32.f * Dir;
		if (Dir != m_Direction)
		{
			for (int i = 0; i < NUM_GROG_LINES; i++)
			{
				m_aLines[i].m_From.x *= -1;
				m_aLines[i].m_To.x *= -1;
			}
			m_Direction = Dir;
			m_LastDirChange = Server()->Tick();

			// Nudging
			m_ProcessedNudge = false;
			CGrog *apEnts[4];
			int Num = GameWorld()->FindEntities(m_Pos, 40.f, (CEntity * *)apEnts, 4, CGameWorld::ENTTYPE_GROG, m_DDTeam);
			for (int i = 0; i < Num; ++i)
			{
				CGrog *pGrog = apEnts[i];
				if (pGrog == this || pGrog->m_ProcessedNudge || pGrog->m_Lifetime != -1)
					continue;
				if (pGrog->m_LastDirChange + Server()->TickSpeed() / 3 < Server()->Tick())
					continue;
				if (m_Direction == pGrog->m_Direction || (m_Direction == -1 && m_Pos.x < pGrog->GetPos().x) || (m_Direction == 1 && m_Pos.x > pGrog->GetPos().x))
					continue;
				vec2 Diff = m_Pos - pGrog->GetPos();
				if (abs(Diff.x) > 30.f || abs(Diff.x) < 14.f || abs(Diff.y) > 37.f)
					continue;

				vec2 CenterPos = (m_Pos + pGrog->GetPos()) / 2;
				// For snapping own hammerhit effect in silentfarm, round position so it can be matched flawlessly later in eventhandler snap
				CenterPos = vec2(round_to_int(CenterPos.x), round_to_int(CenterPos.y));
				m_LastNudgePos = pGrog->m_LastNudgePos = CenterPos;

				GameServer()->CreateHammerHit(CenterPos, m_TeamMask);
				m_ProcessedNudge = true;
			}
		}
	}
	else
	{
		m_Lifetime--;
		if (m_Lifetime <= 0)
		{
			Reset();
			return;
		}

		HandleDropped();

		if (m_PickupDelay > 0)
			m_PickupDelay--;
		Pickup();
	}

	m_PrevPos = m_Pos;
}

void CGrog::Pickup()
{
	CCharacter *apEnts[MAX_CLIENTS];
	int Num = GameWorld()->FindEntities(m_Pos, 20.0f, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER, m_DDTeam);

	for (int i = 0; i < Num; i++)
	{
		CCharacter* pChr = apEnts[i];
		if (m_PickupDelay > 0 && pChr == GetOwner())
			continue;

		if (pChr->AddGrog())
		{
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, m_TeamMask);
			Reset(false);
			break;
		}
	}
}

void CGrog::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient, m_Pos))
		return;

	if (!CmaskIsSet(m_TeamMask, SnappingClient))
		return;

	if (GetOwner() && GetOwner()->IsPaused())
		return;

	for (int i = 0; i < NUM_GROG_LINES; i++)
	{
		bool IsLiquid = i == GROG_LINE_LIQUID;
		int StartTick = IsLiquid ? Server()->Tick() : Server()->Tick() - 2;
		vec2 PosTo = m_Pos + m_aLines[i].m_To;
		vec2 PosFrom = m_Pos + m_aLines[i].m_From;
		if (IsLiquid)
			PosTo.y += m_NumSips * 8.f;

		int SnappingClientVersion = GameServer()->GetClientDDNetVersion(SnappingClient);
		GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, Server()->IsSevendown(SnappingClient), SnappingClient), m_aLines[i].m_ID,
			PosTo, PosFrom, StartTick, m_Owner, IsLiquid ? LASERTYPE_SHOTGUN : LASERTYPE_FREEZE, -1, -1, LASERFLAG_NO_PREDICT);
	}
}
