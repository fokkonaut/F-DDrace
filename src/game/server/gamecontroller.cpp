/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/mapitems.h>

#include "entities/character.h"
#include "entities/pickup.h"
#include "entities/playercounter.h"
#include "gamecontext.h"
#include "gamecontroller.h"
#include "player.h"

#include "entities/light.h"
#include "entities/dragger.h"
#include "entities/gun.h"
#include "entities/projectile.h"
#include "entities/plasma.h"
#include "entities/door.h"
#include "entities/clock.h"
#include <game/layers.h>


IGameController::IGameController(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pConfig = m_pGameServer->Config();
	m_pServer = m_pGameServer->Server();

	// game
	m_GameStartTick = Server()->Tick();
	m_MatchCount = 0;
	m_RoundCount = 0;
	m_SuddenDeath = 0;

	// info
	m_GameFlags = 0;
	m_pGameType = "unknown";

	m_GameInfo.m_MatchCurrent = 0;
	m_GameInfo.m_MatchNum = 0;
	m_GameInfo.m_ScoreLimit = Config()->m_SvScorelimit;
	m_GameInfo.m_TimeLimit = Config()->m_SvTimelimit;

	// map
	m_aMapWish[0] = 0;

	// F-DDrace

	m_CurrentRecord = 0;
}

bool IGameController::CanSpawn(vec2 *pOutPos, int Index, int DDTeam) const
{
	if (Index < TILE_AIR || Index >= NUM_INDICES)
		return false;

	CSpawnEval Eval;
	EvaluateSpawnType(&Eval, Index, DDTeam);
	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

float IGameController::EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos, int DDTeam) const
{
	float Score = 0.0f;
	CCharacter *pC = static_cast<CCharacter *>(GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER));
	for(; pC; pC = (CCharacter *)pC->TypeNext())
	{
		// ignore players in other teams
		if(GameServer()->GetDDRaceTeam(pC->GetPlayer()->GetCID()) != DDTeam)
			continue;

		// team mates are not as dangerous as enemies
		float Scoremod = 1.0f;
		if(pEval->m_FriendlyTeam != -1 && pC->GetPlayer()->GetTeam() == pEval->m_FriendlyTeam)
			Scoremod = 0.5f;

		float d = distance(Pos, pC->GetPos());
		Score += Scoremod * (d == 0 ? 1000000000.0f : 1.0f/d);
	}

	return Score;
}

void IGameController::EvaluateSpawnType(CSpawnEval *pEval, int MapIndex, int DDTeam) const
{
	// get spawn point
	for(unsigned int i = 0; i < GameServer()->Collision()->m_vTiles[MapIndex].size(); i++)
	{
		// check if the position is occupado
		CCharacter *aEnts[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(GameServer()->Collision()->m_vTiles[MapIndex][i], 64, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		vec2 Positions[5] = { vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f) };	// start, left, up, right, down
		int Result = -1;
		for(int Index = 0; Index < 5 && Result == -1; ++Index)
		{
			Result = Index;
			for(int c = 0; c < Num; ++c)
				if(GameServer()->Collision()->CheckPoint(GameServer()->Collision()->m_vTiles[MapIndex][i] +Positions[Index]) ||
					distance(aEnts[c]->GetPos(), GameServer()->Collision()->m_vTiles[MapIndex][i] +Positions[Index]) <= aEnts[c]->GetProximityRadius())
				{
					Result = -1;
					break;
				}
		}
		if(Result == -1)
			continue;	// try next spawn point

		vec2 P = GameServer()->Collision()->m_vTiles[MapIndex][i] + Positions[Result];
		float S = EvaluateSpawnPos(pEval, P, DDTeam);
		if(!pEval->m_Got || pEval->m_Score > S)
		{
			pEval->m_Got = true;
			pEval->m_Score = S;
			pEval->m_Pos = P;
		}
	}
}

int IGameController::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	return 0;
}

void IGameController::OnCharacterSpawn(CCharacter *pChr)
{
	// default health
	pChr->IncreaseHealth(10);

	// give default weapons
	switch (pChr->GetPlayer()->m_Minigame)
	{
	case MINIGAME_SURVIVAL:
	{
		pChr->GiveWeapon(WEAPON_HAMMER);
		pChr->SetActiveWeapon(WEAPON_HAMMER);
		break;
	}
	case MINIGAME_INSTAGIB_BOOMFNG:
	{
		pChr->GiveWeapon(WEAPON_HAMMER);
		pChr->GiveWeapon(WEAPON_GRENADE);
		pChr->SetActiveWeapon(WEAPON_GRENADE);
		break;
	}
	case MINIGAME_INSTAGIB_FNG:
	{
		pChr->GiveWeapon(WEAPON_HAMMER);
		pChr->GiveWeapon(WEAPON_LASER);
		pChr->SetActiveWeapon(WEAPON_LASER);
		break;
	}
	case MINIGAME_1VS1:
	{
		if (GameServer()->Arenas()->OnCharacterSpawn(pChr->GetPlayer()->GetCID()))
			break;
		goto default_case;
	}
	case MINIGAME_DURAK:
	{
		GameServer()->Durak()->OnCharacterSpawn(pChr);
		goto default_case;
	}
	default_case:
	default:
	{
		pChr->GiveWeapon(WEAPON_HAMMER);
		pChr->GiveWeapon(WEAPON_GUN, false, pChr->GetPlayer()->m_Gamemode == GAMEMODE_VANILLA ? 10 : -1);
		pChr->SetActiveWeapon(WEAPON_GUN);
	}
	}
}

bool IGameController::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	if (Index < 0)
		return false;

	int Type = -1;
	int SubType = 0;

	int x,y;
	x=(Pos.x-16.0f)/32.0f;
	y=(Pos.y-16.0f)/32.0f;
	int sides[8];
	vec2 sidepos[8];
	sidepos[0]=vec2(x,y+1);
	sidepos[1]=vec2(x+1,y+1);
	sidepos[2]=vec2(x+1,y);
	sidepos[3]=vec2(x+1,y-1);
	sidepos[4]=vec2(x,y-1);
	sidepos[5]=vec2(x-1,y-1);
	sidepos[6]=vec2(x-1,y);
	sidepos[7]=vec2(x-1,y+1);
	for (int i = 0; i < 8; i++)
		sides[i] = GameServer()->Collision()->Entity(sidepos[i].x, sidepos[i].y, Layer);

	if(Index == ENTITY_DOOR)
	{
		for(int i = 0; i < 8;i++)
		{
			if (sides[i] >= ENTITY_LASER_SHORT && sides[i] <= ENTITY_LASER_LONG)
			{
				new CDoor
				(
					&GameServer()->m_World, //GameWorld
					Pos, //Pos
					pi / 4 * i, //Rotation
					32 * 3 + 32 *(sides[i] - ENTITY_LASER_SHORT) * 3, //Length
					Number //Number
				);
			}
		}
	}
	else if (Layer == LAYER_SWITCH && Index == TILE_SWITCH_PLOT_DOOR && Number > 0
		&& GameServer()->Collision()->GetSwitchDelay(GameServer()->Collision()->GetMapIndex(Pos)) == 0)
	{
		for(int i = 0; i < 8;i++)
		{
			vec2 Pos2 = vec2(sidepos[i].x*32.f+16.f, sidepos[i].y*32.f+16.f);
			int Delay = GameServer()->Collision()->GetSwitchDelay(GameServer()->Collision()->GetMapIndex(Pos2));
			if (sides[i] == TILE_SWITCH_PLOT_DOOR && Delay > 0)
			{
				CDoor *pDoor = new CDoor
				(
					&GameServer()->m_World, //GameWorld
					Pos, //Pos
					pi / 4 * i, //Rotation
					32 * Delay, //Length
					Number //Number
				);
				pDoor->m_PlotID = GameServer()->Collision()->GetPlotBySwitch(Number);
			}
		}
	}
	else if (Layer == LAYER_SWITCH && Index == TILE_SWITCH_PLOT_TOTELE && Number > 0)
	{
		int PlotID = GameServer()->Collision()->GetPlotBySwitch(Number);
		GameServer()->m_aPlots[PlotID].m_ToTele = Pos;
		GameServer()->m_aPlots[PlotID].m_Size = GameServer()->Collision()->m_apPlotSize[PlotID];
	}
	else if (Layer == LAYER_SWITCH && Index == TILE_SWITCH_REDIRECT_SERVER_TO && Number > 0)
	{
		int Delay = GameServer()->Collision()->GetSwitchDelay(GameServer()->Collision()->GetMapIndex(Pos));
		if (Delay == 0)
		{
			CCollision::SRedirectTile RedirectTile;
			RedirectTile.m_Number = Number;
			RedirectTile.m_Pos = Pos;
			GameServer()->Collision()->m_vRedirectTiles.push_back(RedirectTile);
		}
		else
		{
			int Port = GameServer()->GetRediretListPort(Number);
			if (Port > 0)
			{
				new CPlayerCounter(&GameServer()->m_World, Pos, Port);
			}
		}
	}
	else if (Layer == LAYER_SWITCH && Index == TILE_SWITCH_HELICOPTER_SPAWN)
	{
		int Delay = GameServer()->Collision()->GetSwitchDelay(GameServer()->Collision()->GetMapIndex(Pos));
		GameServer()->SpawnHelicopter(-1, 0, Pos, Delay, 1.f, true, Number);
	}
	else if (Layer == LAYER_SWITCH && Index == TILE_DURAK_TABLE)
	{
		GameServer()->Durak()->AddMapTableTile(Number, Pos);
	}
	else if (Layer == LAYER_SWITCH && Index == TILE_DURAK_SEAT)
	{
		int MapIndex = GameServer()->Collision()->GetMapIndex(Pos);
		int Delay = GameServer()->Collision()->GetSwitchDelay(MapIndex);
		if (Delay > 0)
		{
			GameServer()->Durak()->AddMapSeatTile(Number, MapIndex, Delay-1);
		}
	}
	else if(Index == ENTITY_CRAZY_SHOTGUN_EX)
	{
		int Dir;
		if(!Flags)
			Dir = 0;
		else if(Flags == ROTATION_90)
			Dir = 1;
		else if(Flags == ROTATION_180)
			Dir = 2;
		else
			Dir = 3;
		float Deg = Dir * (pi / 2);
		CProjectile *pBullet = new CProjectile
			(
			&GameServer()->m_World,
			WEAPON_SHOTGUN, //Type
			-1, //Owner
			Pos, //Pos
			vec2(sin(Deg), cos(Deg)), //Dir
			-2, //Span
			true, //Freeze
			true, //Explosive
			0, //Force
			(Config()->m_SvShotgunBulletSound)?SOUND_GRENADE_EXPLODE:-1,//SoundImpact
			Layer,
			Number
			);
		pBullet->SetBouncing(2 - (Dir % 2));
	}
	else if(Index == ENTITY_CRAZY_SHOTGUN)
	{
		int Dir;
		if(!Flags)
			Dir=0;
		else if(Flags == (TILEFLAG_ROTATE))
			Dir = 1;
		else if(Flags == (TILEFLAG_VFLIP|TILEFLAG_HFLIP))
			Dir = 2;
		else
			Dir = 3;
		float Deg = Dir * ( pi / 2);
		CProjectile *pBullet = new CProjectile
			(
			&GameServer()->m_World,
			WEAPON_SHOTGUN, //Type
			-1, //Owner
			Pos, //Pos
			vec2(sin(Deg), cos(Deg)), //Dir
			-2, //Span
			true, //Freeze
			false, //Explosive
			0,
			SOUND_GRENADE_EXPLODE,
			Layer,
			Number
			);
		pBullet->SetBouncing(2 - (Dir % 2));
	}

	if(Index == ENTITY_ARMOR_1)
		Type = POWERUP_ARMOR;
	else if(Index == ENTITY_HEALTH_1)
		Type = POWERUP_HEALTH;
	else if(Index == ENTITY_WEAPON_SHOTGUN)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_SHOTGUN;
	}
	else if(Index == ENTITY_WEAPON_GRENADE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_GRENADE;
	}
	else if(Index == ENTITY_WEAPON_LASER)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_LASER;
	}
	else if(Index == ENTITY_POWERUP_NINJA)
	{
		Type = POWERUP_NINJA;
		SubType = WEAPON_NINJA;
	}
	else if (Index == ENTITY_WEAPON_GUN)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_GUN;
	}
	else if (Index == ENTITY_WEAPON_HAMMER)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_HAMMER;
	}
	else if (Index == ENTITY_WEAPON_PLASMA_RIFLE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_PLASMA_RIFLE;
	}
	else if (Index == ENTITY_WEAPON_HEART_GUN)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_HEART_GUN;
	}
	else if (Index == ENTITY_WEAPON_STRAIGHT_GRENADE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_STRAIGHT_GRENADE;
	}
	else if (Index == ENTITY_WEAPON_TELEKINESIS)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_TELEKINESIS;
	}
	else if (Index == ENTITY_WEAPON_LIGHTSABER)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_LIGHTSABER;
	}
	else if (Index == ENTITY_WEAPON_PORTAL_RIFLE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_PORTAL_RIFLE;
	}
	else if (Index == ENTITY_WEAPON_PROJECTILE_RIFLE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_PROJECTILE_RIFLE;
	}
	else if (Index == ENTITY_WEAPON_BALL_GRENADE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_BALL_GRENADE;
	}
	else if (Index == ENTITY_WEAPON_TELE_RIFLE)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_TELE_RIFLE;
	}
	else if (Index == ENTITY_WEAPON_TASER)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_TASER;
	}
	else if (Index == ENTITY_WEAPON_LIGHTNING_LASER)
	{
		Type = POWERUP_WEAPON;
		SubType = WEAPON_LIGHTNING_LASER;
	}
	else if (Index == ENTITY_PICKUP_TASER_BATTERY)
	{
		Type = POWERUP_BATTERY;
		SubType = WEAPON_TASER;
	}
	else if (Index == ENTITY_PICKUP_PORTAL_BATTERY)
	{
		Type = POWERUP_BATTERY;
		SubType = WEAPON_PORTAL_RIFLE;
	}
	else if (Index == ENTITY_CLOCK)
	{
		new CClock(&GameServer()->m_World, Pos);
	}
	else if(Index >= ENTITY_LASER_FAST_CCW && Index <= ENTITY_LASER_FAST_CW)
	{
		int sides2[8];
		sides2[0]=GameServer()->Collision()->Entity(x, y + 2, Layer);
		sides2[1]=GameServer()->Collision()->Entity(x + 2, y + 2, Layer);
		sides2[2]=GameServer()->Collision()->Entity(x + 2, y, Layer);
		sides2[3]=GameServer()->Collision()->Entity(x + 2, y - 2, Layer);
		sides2[4]=GameServer()->Collision()->Entity(x,y - 2, Layer);
		sides2[5]=GameServer()->Collision()->Entity(x - 2, y - 2, Layer);
		sides2[6]=GameServer()->Collision()->Entity(x - 2, y, Layer);
		sides2[7]=GameServer()->Collision()->Entity(x - 2, y + 2, Layer);

		float AngularSpeed = 0.0f;
		int Ind=Index-ENTITY_LASER_STOP;
		int M;
		if( Ind < 0)
		{
			Ind = -Ind;
			M = 1;
		}
		else if(Ind == 0)
			M = 0;
		else
			M = -1;


		if(Ind == 0)
			AngularSpeed = 0.0f;
		else if(Ind == 1)
			AngularSpeed = pi / 360;
		else if(Ind == 2)
			AngularSpeed = pi / 180;
		else if(Ind == 3)
			AngularSpeed = pi / 90;
		AngularSpeed *= M;

		for(int i=0; i<8;i++)
		{
			if(sides[i] >= ENTITY_LASER_SHORT && sides[i] <= ENTITY_LASER_LONG)
			{
				CLight *Lgt = new CLight(&GameServer()->m_World, Pos, pi / 4 * i, 32 * 3 + 32 * (sides[i] - ENTITY_LASER_SHORT) * 3, Layer, Number);
				Lgt->m_AngularSpeed = AngularSpeed;
				if(sides2[i] >= ENTITY_LASER_C_SLOW && sides2[i] <= ENTITY_LASER_C_FAST)
				{
					Lgt->m_Speed = 1 + (sides2[i] - ENTITY_LASER_C_SLOW) * 2;
					Lgt->m_CurveLength = Lgt->m_Length;
				}
				else if(sides2[i] >= ENTITY_LASER_O_SLOW && sides2[i] <= ENTITY_LASER_O_FAST)
				{
					Lgt->m_Speed = 1 + (sides2[i] - ENTITY_LASER_O_SLOW) * 2;
					Lgt->m_CurveLength = 0;
				}
				else
					Lgt->m_CurveLength = Lgt->m_Length;
			}
		}

	}
	else if(Index >= ENTITY_DRAGGER_WEAK && Index <= ENTITY_DRAGGER_STRONG)
	{
		new CDragger(&GameServer()->m_World, Pos, Index - ENTITY_DRAGGER_WEAK + 1, false, Layer, Number);
	}
	else if(Index >= ENTITY_DRAGGER_WEAK_NW && Index <= ENTITY_DRAGGER_STRONG_NW)
	{
		new CDragger(&GameServer()->m_World, Pos, Index - ENTITY_DRAGGER_WEAK_NW + 1, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAE)
	{
		new CGun(&GameServer()->m_World, Pos, false, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAF)
	{
		new CGun(&GameServer()->m_World, Pos, true, false, Layer, Number);
	}
	else if(Index == ENTITY_PLASMA)
	{
		new CGun(&GameServer()->m_World, Pos, true, true, Layer, Number);
	}
	else if(Index == ENTITY_PLASMAU)
	{
		new CGun(&GameServer()->m_World, Pos, false, false, Layer, Number);
	}

	if(Type != -1)
	{
		new CPickup(&GameServer()->m_World, Pos, Type, SubType, Layer, Number);
		return true;
	}

	return false;
}

void IGameController::ResetGame()
{
	GameServer()->m_World.m_ResetRequested = true;
}

void IGameController::StartRound()
{
	ResetGame();

	m_RoundCount = 0;
	m_SuddenDeath = 0;
	GameServer()->m_World.m_Paused = false;
	Server()->DemoRecorder_HandleAutoStart();
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "start match type='%s' teamplay='%d'", m_pGameType, m_GameFlags & GAMEFLAG_TEAMS);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
}

void IGameController::Snap(int SnappingClient)
{
	int Size = Server()->IsSevendown(SnappingClient) ? 8*4 : sizeof(CNetObj_GameData);
	CNetObj_GameData *pGameData = static_cast<CNetObj_GameData *>(Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, Size));
	if(!pGameData)
		return;

	CPlayer *pSnap = GameServer()->m_apPlayers[SnappingClient];
	CCharacter *pSnappingChar = GameServer()->GetPlayerChar(SnappingClient);
	CCharacter *pSpectator = !pSnap ? 0 : (pSnap->GetTeam() == TEAM_SPECTATORS || pSnap->IsPaused()) ? GameServer()->GetPlayerChar(pSnap->GetSpectatorID()) : pSnap->m_pControlledTee ? pSnap->m_pControlledTee->GetCharacter() : 0;
	int GameStartTick = m_GameStartTick;
	bool IsBirthdayPresent = false;
	{
		CCharacter *pChr = pSpectator ? pSpectator : pSnappingChar;
		if (pChr)
		{
			if (pChr->m_BirthdayGiftEndTick > Server()->Tick())
			{
				GameStartTick = pChr->m_BirthdayGiftEndTick - Server()->TickSpeed() * 60;
				IsBirthdayPresent = true;
			}
			else if (pChr->m_DDRaceState == DDRACE_STARTED)
				GameStartTick = pChr->m_StartTime;
		}
	}

	int GameStateFlags = 0;
	{
		if(m_SuddenDeath)
			GameStateFlags |= GAMESTATEFLAG_SUDDENDEATH;
		if (GameServer()->m_World.m_Paused)
			GameStateFlags |= GAMESTATEFLAG_PAUSED;
	}

	// demo recording
	if(SnappingClient == -1)
	{
		CNetObj_De_GameInfo *pGameInfo = static_cast<CNetObj_De_GameInfo *>(Server()->SnapNewItem(NETOBJTYPE_DE_GAMEINFO, 0, sizeof(CNetObj_De_GameInfo)));
		if(!pGameInfo)
			return;

		pGameInfo->m_GameFlags = m_GameFlags;
		pGameInfo->m_ScoreLimit = 0;
		pGameInfo->m_TimeLimit = 0;
		pGameInfo->m_MatchNum = 0;
		pGameInfo->m_MatchCurrent = 0;
	}

	if (Server()->IsSevendown(SnappingClient))
	{
		int TranslatedGameStateFlags = 0;
		if (GameStateFlags&GAMESTATEFLAG_GAMEOVER)
			TranslatedGameStateFlags |= 1;
		if (GameStateFlags&GAMESTATEFLAG_SUDDENDEATH)
			TranslatedGameStateFlags |= 2;
		if (GameStateFlags&GAMESTATEFLAG_PAUSED)
			TranslatedGameStateFlags |= 4;

		int ScoreLimit = m_GameInfo.m_ScoreLimit;
		int ScoreLimitID = pSpectator ? pSpectator->GetPlayer()->GetCID() : SnappingClient;
		if (GameServer()->Arenas()->FightStarted(ScoreLimitID))
			ScoreLimit = GameServer()->Arenas()->GetScoreLimit(ScoreLimitID);
		else if (pSnap->m_ScoreMode == SCORE_BONUS)
			ScoreLimit = Config()->m_SvNoBonusScoreTreshold;

		((int*)pGameData)[0] = m_GameFlags;
		((int*)pGameData)[1] = TranslatedGameStateFlags;
		((int*)pGameData)[2] = GameStartTick;
		((int*)pGameData)[3] = 0;
		((int*)pGameData)[4] = ScoreLimit;
		((int*)pGameData)[5] = IsBirthdayPresent ? 1 : m_GameInfo.m_TimeLimit;
		((int*)pGameData)[6] = 0;
		((int*)pGameData)[7] = m_RoundCount+1;
	}
	else
	{
		// 0.7 clients cant handle a negative game start tick, thats why we just give them a 0:00 time to "indicate" this is not their real time
		if (GameStartTick < 0)
			GameStartTick = Server()->Tick();

		pGameData->m_GameStartTick = GameStartTick;
		pGameData->m_GameStateFlags = GameStateFlags;
		pGameData->m_GameStateEndTick = 0; // no timer/infinite = 0, on end = GameEndTick, otherwise = GameStateEndTick

		CNetObj_GameDataRace* pGameDataRace = static_cast<CNetObj_GameDataRace*>(Server()->SnapNewItem(NETOBJTYPE_GAMEDATARACE, 0, sizeof(CNetObj_GameDataRace)));
		if (!pGameDataRace)
			return;

		pGameDataRace->m_BestTime = m_CurrentRecord == 0 ? -1 : m_CurrentRecord * 1000.0f;
		pGameDataRace->m_Precision = 0;
		pGameDataRace->m_RaceFlags = 0;
	}

	if(GameServer()->Collision()->m_pSwitchers)
	{
		int Team = pSnappingChar ? pSnappingChar->Team() : 0;

		if(pSnappingChar && (pSnap->GetTeam() == TEAM_SPECTATORS || pSnappingChar->IsPaused()) && pSnap->GetSpecMode() != SPEC_FREEVIEW && GameServer()->m_apPlayers[pSnap->GetSpectatorID()] && GameServer()->m_apPlayers[pSnap->GetSpectatorID()]->GetCharacter())
			Team = GameServer()->m_apPlayers[pSnap->GetSpectatorID()]->GetCharacter()->Team();

		if(Team != TEAM_SUPER)
		{
			CNetObj_SwitchState *pSwitchState = static_cast<CNetObj_SwitchState *>(Server()->SnapNewItem(NETOBJTYPE_SWITCHSTATE, Team, sizeof(CNetObj_SwitchState)));
			if(!pSwitchState)
				return;

			pSwitchState->m_HighestSwitchNumber = clamp(GameServer()->Collision()->GetNumAllSwitchers(), 0, 255);
			mem_zero(pSwitchState->m_Status, sizeof(pSwitchState->m_Status));

			std::vector<std::pair<int, int>> vEndTicks; // <EndTick, SwitchNumber>

			for(int i = 0; i < pSwitchState->m_HighestSwitchNumber + 1; i++)
			{
				int Status = (int)GameServer()->Collision()->m_pSwitchers[i].m_Status[Team];
				pSwitchState->m_Status[i / 32] |= (Status << (i % 32));

				int EndTick = GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team];
				if(EndTick > 0 && EndTick < Server()->Tick() + 3 * Server()->TickSpeed() && GameServer()->Collision()->m_pSwitchers[i].m_LastUpdateTick[Team] < Server()->Tick())
				{
					// only keep track of EndTicks that have less than three second left and are not currently being updated by a player being present on a switch tile, to limit how often these are sent
					vEndTicks.emplace_back(std::pair<int, int>(GameServer()->Collision()->m_pSwitchers[i].m_EndTick[Team], i));
				}
			}

			// send the endtick of switchers that are about to toggle back (up to four, prioritizing those with the earliest endticks)
			mem_zero(pSwitchState->m_aSwitchNumbers, sizeof(pSwitchState->m_aSwitchNumbers));
			mem_zero(pSwitchState->m_aEndTicks, sizeof(pSwitchState->m_aEndTicks));

			std::sort(vEndTicks.begin(), vEndTicks.end());
			const int NumTimedSwitchers = min((int)vEndTicks.size(), (int)std::size(pSwitchState->m_aEndTicks));

			for(int i = 0; i < NumTimedSwitchers; i++)
			{
				pSwitchState->m_aSwitchNumbers[i] = vEndTicks[i].second;
				pSwitchState->m_aEndTicks[i] = vEndTicks[i].first;
			}
		}
	}

	CNetObj_GameInfoEx *pGameInfoEx = (CNetObj_GameInfoEx *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
	if (!pGameInfoEx)
		return;

	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;
	pGameInfoEx->m_Flags = 0
		| GAMEINFOFLAG_GAMETYPE_RACE
		| GAMEINFOFLAG_GAMETYPE_DDRACE
		| GAMEINFOFLAG_GAMETYPE_DDNET
		| GAMEINFOFLAG_RACE_RECORD_MESSAGE
		| GAMEINFOFLAG_ALLOW_EYE_WHEEL
		| GAMEINFOFLAG_ALLOW_HOOK_COLL
		| GAMEINFOFLAG_BUG_DDRACE_GHOST
		| GAMEINFOFLAG_BUG_DDRACE_INPUT
		| GAMEINFOFLAG_PREDICT_DDRACE
		| GAMEINFOFLAG_PREDICT_DDRACE_TILES
		| GAMEINFOFLAG_ENTITIES_DDNET
		| GAMEINFOFLAG_ENTITIES_DDRACE
		| GAMEINFOFLAG_ENTITIES_RACE
		| GAMEINFOFLAG_RACE
		| GAMEINFOFLAG_DONT_MASK_ENTITIES;
	pGameInfoEx->m_Flags2 = 0
		| GAMEINFOFLAG2_GAMETYPE_FDDRACE
		| GAMEINFOFLAG2_ENTITIES_FDDRACE;

	if (Config()->m_SvAllowXSkins)
		pGameInfoEx->m_Flags2 |= GAMEINFOFLAG2_ALLOW_X_SKINS;

	if (!Config()->m_SvWeakHook)
		pGameInfoEx->m_Flags2 |= GAMEINFOFLAG2_NO_WEAK_HOOK_AND_BOUNCE;

	if (!pSnap->RestrictZoom())
		pGameInfoEx->m_Flags |= GAMEINFOFLAG_ALLOW_ZOOM;

	if (!pSnap->IsMinigame() && pSnap->m_ScoreMode == SCORE_TIME)
		pGameInfoEx->m_Flags |= GAMEINFOFLAG_TIMESCORE;

	if (pSnap->ShowDDraceHud())
		pGameInfoEx->m_Flags2 |= GAMEINFOFLAG2_HUD_DDRACE;
	else // fng, survival
		pGameInfoEx->m_Flags2 |= GAMEINFOFLAG2_HUD_HEALTH_ARMOR;

	// allow inputs for arena placing while in spec, or for click to spectate in spectators
	if (GameServer()->Arenas()->IsConfiguring(SnappingClient) || pSnap->GetTeam() == TEAM_SPECTATORS)
		pGameInfoEx->m_Flags &= ~GAMEINFOFLAG_BUG_DDRACE_INPUT;

	if (!pSnappingChar)
		return;

	// only allow click to spectate if the tee has no input applied while pausing, because otherwise it would have unexpected hook releases after resuming because this flag resets inputs
	if (pSnap->IsPaused() && pSnappingChar->IsIdle())
		pGameInfoEx->m_Flags &= ~GAMEINFOFLAG_BUG_DDRACE_INPUT;

	if (pSnappingChar->GetWeaponAmmo(pSnappingChar->GetActiveWeapon()) == -1)
		pGameInfoEx->m_Flags |= GAMEINFOFLAG_UNLIMITED_AMMO;

	int W = pSnappingChar->GetActiveWeapon();
	if (!pSnap->m_ZoomCursor && (pSnappingChar->m_DrawEditor.Active() || W == WEAPON_TELEKINESIS || W == WEAPON_PORTAL_RIFLE || W == WEAPON_TELE_RIFLE || pSnappingChar->m_IsPortalBlocker))
		pGameInfoEx->m_Flags &= ~GAMEINFOFLAG_ALLOW_ZOOM;

	if (pSnappingChar->ShowAmmoHud())
		pGameInfoEx->m_Flags2 |= GAMEINFOFLAG2_HUD_AMMO;
}

void IGameController::Tick()
{
	// check for inactive players
	DoActivityCheck();
}

void IGameController::DoActivityCheck()
{
	if(Config()->m_SvInactiveKickTime == 0)
		return;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameServer()->m_apPlayers[i] && !GameServer()->m_apPlayers[i]->IsDummy() && !GameServer()->m_apPlayers[i]->m_IsDummy && (GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS || Config()->m_SvInactiveKick > 0) &&
			Server()->GetAuthedState(i) == AUTHED_NO && (Server()->Tick() > GameServer()->m_apPlayers[i]->m_LastActionTick + Config()->m_SvInactiveKickTime * Server()->TickSpeed() * 60))
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
			{
				if(Config()->m_SvInactiveKickSpec)
					Server()->Kick(i, "Kicked for inactivity");
			}
			else
			{
				switch(Config()->m_SvInactiveKick)
				{
				case 1:
					{
						// move player to spectator
						GameServer()->m_apPlayers[i]->SetTeam(TEAM_SPECTATORS);
					}
					break;
				case 2:
					{
						// move player to spectator if the reserved slots aren't filled yet, kick him otherwise
						int Spectators = 0;
						for(int j = 0; j < MAX_CLIENTS; ++j)
							if(GameServer()->m_apPlayers[j] && GameServer()->m_apPlayers[j]->GetTeam() == TEAM_SPECTATORS)
								++Spectators;
						if(Spectators >= Config()->m_SvMaxClients - Config()->m_SvPlayerSlots)
							Server()->Kick(i, "Kicked for inactivity");
						else
							GameServer()->m_apPlayers[i]->SetTeam(TEAM_SPECTATORS);
					}
					break;
				case 3:
					{
						// kick the player
						Server()->Kick(i, "Kicked for inactivity");
					}
				}
			}
		}
	}
}

void IGameController::UpdateGameInfo(int ClientID)
{
	CNetMsg_Sv_GameInfo GameInfoMsg;
	GameInfoMsg.m_GameFlags = m_GameFlags;
	GameInfoMsg.m_ScoreLimit = m_GameInfo.m_ScoreLimit;
	GameInfoMsg.m_TimeLimit = m_GameInfo.m_TimeLimit;
	GameInfoMsg.m_MatchNum = m_GameInfo.m_MatchNum;
	GameInfoMsg.m_MatchCurrent = m_GameInfo.m_MatchCurrent;

	CNetMsg_Sv_GameInfo GameInfoMsgNoRace = GameInfoMsg;
	GameInfoMsgNoRace.m_GameFlags &= ~GAMEFLAG_RACE;

	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pPlayer = GameServer()->m_apPlayers[i];
			if(!pPlayer || !Server()->ClientIngame(i))
				continue;

			// F-DDrace
			if (pPlayer->m_ScoreMode == SCORE_TIME && !pPlayer->IsMinigame() && !GameServer()->Durak()->IsPlayerOnSeat(ClientID))
				GameInfoMsg.m_GameFlags |= GAMEFLAG_RACE;

			if (GameServer()->Arenas()->FightStarted(i))
				GameInfoMsg.m_ScoreLimit = GameServer()->Arenas()->GetScoreLimit(i);
			else if (pPlayer->m_ScoreMode == SCORE_BONUS)
				GameInfoMsg.m_ScoreLimit = Config()->m_SvNoBonusScoreTreshold;

			if (pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_BirthdayGiftEndTick > Server()->Tick())
				GameInfoMsg.m_TimeLimit = 1;

			CNetMsg_Sv_GameInfo *pInfoMsg = (Server()->GetClientVersion(i) < CGameContext::MIN_RACE_CLIENTVERSION) ? &GameInfoMsgNoRace : &GameInfoMsg;
			Server()->SendPackMsg(pInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
		}
	}
	else
	{
		// F-DDrace
		CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
		if (pPlayer && pPlayer->m_ScoreMode == SCORE_TIME && !pPlayer->IsMinigame() && !GameServer()->Durak()->IsPlayerOnSeat(ClientID))
			GameInfoMsg.m_GameFlags |= GAMEFLAG_RACE;

		if (GameServer()->Arenas()->FightStarted(ClientID))
			GameInfoMsg.m_ScoreLimit = GameServer()->Arenas()->GetScoreLimit(ClientID);
		else if (pPlayer->m_ScoreMode == SCORE_BONUS)
			GameInfoMsg.m_ScoreLimit = Config()->m_SvNoBonusScoreTreshold;

		if (pPlayer->GetCharacter() && pPlayer->GetCharacter()->m_BirthdayGiftEndTick > Server()->Tick())
			GameInfoMsg.m_TimeLimit = 1;

		CNetMsg_Sv_GameInfo *pInfoMsg = (Server()->GetClientVersion(ClientID) < CGameContext::MIN_RACE_CLIENTVERSION) ? &GameInfoMsgNoRace : &GameInfoMsg;
		Server()->SendPackMsg(pInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);
	}
}

void IGameController::ChangeMap(const char *pToMap)
{
	str_copy(Config()->m_SvMap, pToMap, sizeof(Config()->m_SvMap));
}

bool IGameController::CanJoinTeam(int Team, int NotThisID) const
{
	if (Team == TEAM_SPECTATORS || (GameServer()->m_apPlayers[NotThisID] && GameServer()->m_apPlayers[NotThisID]->GetTeam() != TEAM_SPECTATORS))
		return true;

	int aNumplayers[2] = { 0,0 };
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (GameServer()->m_apPlayers[i] && i != NotThisID)
		{
			if (GameServer()->m_apPlayers[i]->GetTeam() >= TEAM_RED && GameServer()->m_apPlayers[i]->GetTeam() <= TEAM_BLUE)
				aNumplayers[GameServer()->m_apPlayers[i]->GetTeam()]++;
		}
	}

	return (aNumplayers[0] + aNumplayers[1]) < Config()->m_SvPlayerSlots;
}

int IGameController::ClampTeam(int Team) const
{
	if (Team < TEAM_RED)
		return TEAM_SPECTATORS;
	return TEAM_RED;
}

const char* IGameController::GetTeamName(int Team)
{
	if (Team == TEAM_RED)
		return "game";
	return "spectators";
}

int IGameController::GetStartTeam(int NotThisID)
{
	if(Config()->m_SvTournamentMode)
		return TEAM_SPECTATORS;

	int aNumplayers[2] = {0,0};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i] && i != NotThisID)
		{
			if(GameServer()->m_apPlayers[i]->GetTeam() >= TEAM_RED && GameServer()->m_apPlayers[i]->GetTeam() <= TEAM_BLUE)
				aNumplayers[GameServer()->m_apPlayers[i]->GetTeam()]++;
		}
	}

	int Team = TEAM_RED;

	if(CanJoinTeam(Team, NotThisID))
		return Team;
	return TEAM_SPECTATORS;
}


void IGameController::RegisterChatCommands(CCommandManager *pManager)
{
	//pManager->AddCommand("test", "Test the command system", "r", Com_Example, this);
}

/*void IGameController::Com_Example(IConsole::IResult *pResult, void *pContext)
{
	CCommandManager::SCommandContext *pComContext = (CCommandManager::SCommandContext *)pContext;
	IGameController *pSelf = (IGameController *)pComContext->m_pContext;
	pSelf->GameServer()->SendBroadcast(pResult->GetString(0), -1);
}*/
