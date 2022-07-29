#include "snake.h"
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <engine/shared/config.h>

CGameContext *CSnake::GameServer() const { return m_pCharacter->GameServer(); }
IServer *CSnake::Server() const { return GameServer()->Server(); }

void CSnake::Init(CCharacter *pChr)
{
	m_pCharacter = pChr;
	m_Active = false;
	m_MoveLifespan = 0;
	m_Dir = vec2(0, 0);
}

bool CSnake::Active()
{
	return m_Active;
}

bool CSnake::SetActive(bool Active)
{
	if (Active && IsInAnySnake(m_pCharacter))
		return false;

	m_Active = Active;
	m_MoveLifespan = 0;
	m_Dir = vec2(0, 0);

	if (m_Active)
	{
		SSnakeData Data;
		Data.m_pChr = m_pCharacter;
		Data.m_Pos = GameServer()->RoundPos(m_pCharacter->Core()->m_Pos);
		m_vSnake.push_back(Data);
		GameServer()->SendTuningParams(m_pCharacter->GetPlayer()->GetCID(), m_pCharacter->m_TuneZone);
	}
	else
	{
		for (unsigned int i = 0; i < m_vSnake.size(); i++)
			GameServer()->SendTuningParams(m_vSnake[i].m_pChr->GetPlayer()->GetCID(), m_vSnake[i].m_pChr->m_TuneZone);
		m_vSnake.clear();
	}
	return true;
}

void CSnake::OnInput(CNetObj_PlayerInput *pNewInput)
{
	m_Input.m_Jump = pNewInput->m_Jump;
	m_Input.m_Hook = pNewInput->m_Hook;
	m_Input.m_Direction = pNewInput->m_Direction;
}

void CSnake::Tick()
{
	if (!Active())
		return;

	if (m_MoveLifespan)
		m_MoveLifespan--;

	InvalidateTees();
	if (HandleInput())
		return;

	AddNewTees();
	UpdateTees();
}

void CSnake::InvalidateTees()
{
	for (unsigned int i = 0; i < m_vSnake.size(); i++)
	{
		if (!m_vSnake[i].m_pChr)
		{
			m_vSnake.erase(m_vSnake.begin() + i);
			i--;
		}
	}
}

bool CSnake::HandleInput()
{
	vec2 Dir = vec2(m_Input.m_Direction, 0);
	if (m_Input.m_Jump != m_Input.m_Hook)
		Dir.y = m_Input.m_Jump ? -1 : 1;

	if (Dir != vec2(0, 0) && !(Dir.x && Dir.y))
	{
		m_Dir = Dir;
	}
	else if (!GameServer()->Config()->m_SvSnakeAutoMove)
	{
		m_Dir = vec2(0, 0);
		return false;
	}

	if (m_MoveLifespan > 0)
		return false;

	m_MoveLifespan = Server()->TickSpeed() / GameServer()->Config()->m_SvSnakeSpeed;

	if (m_vSnake.size() > 1)
		for (unsigned int i = m_vSnake.size() - 1; i >= 1; i--)
			m_vSnake[i].m_Pos = m_vSnake[i-1].m_Pos;

	m_vSnake[0].m_Pos = GameServer()->RoundPos(m_vSnake[0].m_Pos + m_Dir * 32.f);
	if (GameServer()->Collision()->TestBox(m_vSnake[0].m_Pos, vec2(CCharacterCore::PHYS_SIZE, CCharacterCore::PHYS_SIZE)))
	{
		GameServer()->CreateExplosion(m_vSnake[0].m_Pos, m_pCharacter->GetPlayer()->GetCID(), WEAPON_GRENADE, true, m_pCharacter->Team(), m_pCharacter->TeamMask());
		SetActive(false);
		return true;
	}
	return false;
}

void CSnake::AddNewTees()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(i);
		if (!pChr || IsInSnake(pChr))
			continue;

		if (distance(m_vSnake[0].m_pChr->Core()->m_Pos, pChr->Core()->m_Pos) <= 32.f)
		{
			pChr->Core()->m_Pos = m_vSnake[m_vSnake.size()-1].m_pChr->Core()->m_Pos;
			SSnakeData Data;
			Data.m_pChr = pChr;
			Data.m_Pos = pChr->Core()->m_Pos;
			m_vSnake.push_back(Data);
			GameServer()->SendTuningParams(i, pChr->m_TuneZone);
		}
	}
}

void CSnake::UpdateTees()
{
	for (unsigned int i = 0; i < m_vSnake.size(); i++)
	{
		m_vSnake[i].m_pChr->Core()->m_Vel = vec2(0, 0);
		m_vSnake[i].m_pChr->Core()->m_Pos = m_vSnake[i].m_Pos;
	}
}

bool CSnake::IsInSnake(CCharacter *pChr)
{
	for (unsigned int i = 0; i < m_vSnake.size(); i++)
		if (m_vSnake[i].m_pChr == pChr)
			return true;
	return false;
}

bool CSnake::IsInAnySnake(CCharacter *pCheck)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(i);
		if (pChr && pChr->m_Snake.Active() && pChr->m_Snake.IsInSnake(pCheck))
			return true;
	}
	return false;
}
