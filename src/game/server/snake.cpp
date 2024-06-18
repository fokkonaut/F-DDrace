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
	Reset();
}

void CSnake::Reset()
{
	m_MoveLifespan = 0;
	m_Dir = vec2(0, 0);
	m_WantedDir = vec2(0, 0);
}

bool CSnake::Active()
{
	return m_Active;
}

bool CSnake::SetActive(bool Active)
{
	if (m_Active == Active || (Active && m_pCharacter->m_InSnake))
		return false;

	m_Active = Active;
	Reset();

	if (m_Active)
	{
		SSnakeData Data;
		Data.m_pChr = m_pCharacter;
		Data.m_Pos = GameServer()->RoundPos(m_pCharacter->Core()->m_Pos);
		m_vSnake.push_back(Data);

		m_pCharacter->GetPlayer()->m_ShowName = false;
		m_pCharacter->m_InSnake = true;
		GameServer()->SendTuningParams(m_pCharacter->GetPlayer()->GetCID(), m_pCharacter->m_TuneZone);
		GameServer()->UnsetTelekinesis(m_pCharacter);
		m_pCharacter->GetPlayer()->StopPlotEditing();
		if (m_pCharacter->m_pHelicopter)
			m_pCharacter->m_pHelicopter->Dismount();
	}
	else
	{
		InvalidateTees();
		for (unsigned int i = 0; i < m_vSnake.size(); i++)
		{
			m_vSnake[i].m_pChr->GetPlayer()->m_ShowName = true;
			m_vSnake[i].m_pChr->m_InSnake = false;
			GameServer()->SendTuningParams(m_vSnake[i].m_pChr->GetPlayer()->GetCID(), m_vSnake[i].m_pChr->m_TuneZone);
		}
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
		if (!m_vSnake[i].m_pChr || !m_vSnake[i].m_pChr->IsAlive())
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

	if (Dir != vec2(0, 0) && (!(Dir.x && Dir.y) || GameServer()->Config()->m_SvSnakeDiagonal))
	{
		m_WantedDir = Dir;
	}
	else if (!GameServer()->Config()->m_SvSnakeAutoMove)
	{
		m_WantedDir = vec2(0, 0);
	}

	if (m_MoveLifespan > 0)
		return false;

	m_Dir = m_WantedDir;
	if (m_Dir == vec2(0, 0))
		return false;

	m_MoveLifespan = Server()->TickSpeed() / GameServer()->Config()->m_SvSnakeSpeed;

	m_PrevLastPos = m_vSnake[m_vSnake.size() - 1].m_Pos;
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
		if (!pChr || pChr->m_InSnake || pChr->Team() != m_pCharacter->Team())
			continue;

		if (distance(m_vSnake[0].m_pChr->Core()->m_Pos, pChr->Core()->m_Pos) <= 40.f)
		{
			pChr->Core()->m_Pos = m_vSnake[m_vSnake.size()-1].m_pChr->Core()->m_Pos;
			SSnakeData Data;
			Data.m_pChr = pChr;
			Data.m_Pos = pChr->Core()->m_Pos;
			m_vSnake.push_back(Data);

			pChr->GetPlayer()->m_ShowName = false;
			pChr->m_InSnake = true;
			GameServer()->SendTuningParams(i, pChr->m_TuneZone);
			GameServer()->UnsetTelekinesis(pChr);
			pChr->GetPlayer()->StopPlotEditing();
			if (pChr->m_pHelicopter)
				pChr->m_pHelicopter->Dismount();
		}
	}
}

void CSnake::UpdateTees()
{
	float Amount = (float)m_MoveLifespan / (Server()->TickSpeed() / GameServer()->Config()->m_SvSnakeSpeed);
	for (unsigned int i = 0; i < m_vSnake.size(); i++)
	{
		m_vSnake[i].m_pChr->Core()->m_Vel = vec2(0, 0);
		if (GameServer()->Config()->m_SvSnakeSmooth)
		{
			vec2 PrevPos = i == (m_vSnake.size() - 1) ? m_PrevLastPos : m_vSnake[i + 1].m_Pos;
			vec2 NewPos = vec2(mix(m_vSnake[i].m_Pos.x, PrevPos.x, Amount), mix(m_vSnake[i].m_Pos.y, PrevPos.y, Amount));
			m_vSnake[i].m_pChr->Core()->m_Pos = NewPos;
		}
		else
		{
			m_vSnake[i].m_pChr->Core()->m_Pos = m_vSnake[i].m_Pos;
		}
	}
}

void CSnake::OnPlayerDeath()
{
	SetActive(false);
	if (m_pCharacter->m_InSnake)
		m_pCharacter->GetPlayer()->m_ShowName = true;
}
