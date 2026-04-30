//
// Created by Matq on 18/04/2026.
//

#include "healthbar.h"
#include <game/server/entities/interactive/vehicle/base/vehicle.h>
#include <game/server/gamecontext.h>

void CHealthBar::SetNumIndicator(CPickupNode *& apPickups, int& NumPickups, int NewNumPickups, int PowerupType)
{
	if (!m_pVehicle)
		return;

	NewNumPickups = maximum(0, NewNumPickups);

	int DeltaPickups = NewNumPickups - NumPickups;
	if (!DeltaPickups)
		return;

	for (int i = 0; i < NumPickups; i++)
		m_pVehicle->Server()->SnapFreeID(apPickups[i].m_ID);

	delete[] apPickups;
	apPickups = NewNumPickups ? new CPickupNode[NewNumPickups] : nullptr;

	if (NewNumPickups > 0)
		for (int i = 0; i < NewNumPickups; i++)
			apPickups[i] = CPickupNode(m_pVehicle, m_pVehicle->Server()->SnapNewID(), PowerupType, vec2(0.f, 0.f));

	NumPickups = NewNumPickups;
	UpdateIndicator();
}

CHealthBar::CHealthBar(IVehicle *pVehicle, bool VerticalLayout)
{
	m_pVehicle = pVehicle;
	m_VerticalLayout = VerticalLayout;

	m_apHeartsIndicator = nullptr;
	m_NumHeartsIndicator = 0;
	m_apArmorIndicator = nullptr;
	m_NumArmorIndicator = 0;
}

void CHealthBar::SetNumHeartsIndicator(int NumHearts)
{
	SetNumIndicator(m_apHeartsIndicator, m_NumHeartsIndicator, NumHearts, POWERUP_HEALTH);
}

void CHealthBar::SetNumArmorIndicator(int NumArmor)
{
	SetNumIndicator(m_apArmorIndicator, m_NumArmorIndicator, NumArmor, POWERUP_ARMOR);
}

void CHealthBar::SetIndicator(int NumHearts, int NumArmor)
{
	SetNumHeartsIndicator(NumHearts);
	SetNumArmorIndicator(NumArmor);
}

void CHealthBar::UpdateIndicator()
{
	if (!m_pVehicle || !m_pVehicle->Model())
		return;

	IVehicleModel *pModel = m_pVehicle->Model();

	float HealthPercentage = m_pVehicle->Health() / m_pVehicle->MaxHealth(); // Health 0..1
	float HeartsPrecise = (float)m_NumHeartsIndicator * HealthPercentage; // Hearts like 3.7
	int HeartsFull = floor(HeartsPrecise); // Hearts like 3.0
	int ShowNumHearts = ceil(HeartsPrecise); // How many are shown, including fraction heart (3 + 0.7 = 4)
	float CurrentHeart = HeartsPrecise - (float)HeartsFull; // Last heart 0..1, example 0.7
	bool FlashingHalfHeart = CurrentHeart > 0.0f && CurrentHeart <= 0.5f; // Flash or not, if last heart below half

	for (int i = 0; i < m_NumHeartsIndicator; i++)
	{
		if (i < ShowNumHearts - FlashingHalfHeart) //
			m_apHeartsIndicator[i].m_Enabled = true;
		else if (i == HeartsFull && FlashingHalfHeart) // Flash heart if last was below 0.5
			m_apHeartsIndicator[i].m_Enabled = (m_pVehicle->Server()->Tick() / 4) % 2 == 0;
		else
			m_apHeartsIndicator[i].m_Enabled = false;
	}

	float ArmorPercentage = m_pVehicle->MaxArmor() > 0.0f ? m_pVehicle->Armor() / m_pVehicle->MaxArmor() : 0.0f; // Armor 0..1
	float ArmorPrecise = (float)m_NumArmorIndicator * ArmorPercentage; // Armor like 2.7
	int ArmorFull = floor(ArmorPrecise); // Armor like 2.0
	int ShowNumArmor = ceil(ArmorPrecise);
	float CurrentArmor = ArmorPrecise - (float)ArmorFull; // Last armor 0..1, example 0.7
	bool FlashingHalfArmor = CurrentArmor > 0.0f && CurrentArmor <= 0.5f;

	for (int i = 0; i < m_NumArmorIndicator; i++)
	{
		if (i < ShowNumArmor - FlashingHalfArmor) //
			m_apArmorIndicator[i].m_Enabled = true;
		else if (i == ArmorFull && FlashingHalfArmor) // Flash if last was below 0.5
			m_apArmorIndicator[i].m_Enabled = (m_pVehicle->Server()->Tick() / 4) % 2 == 0;
		else
			m_apArmorIndicator[i].m_Enabled = false;
	}

	// Positions yay

	bool UsingVertical = (m_VerticalLayout && ShowNumArmor);

	float Gap = 40.f;
	float OffsetY = pModel->GetCachedBounds().m_Top - 30.f + (UsingVertical ? -Gap : 0.0f);
	float OffsetX = -(float)(ShowNumHearts + (UsingVertical ? 0 : ShowNumArmor) - 1) / 2.f * Gap;

	for (int i = 0; i < ShowNumHearts; i++)
	{
		m_apHeartsIndicator[i].m_Pos.x = OffsetX;
		m_apHeartsIndicator[i].m_Pos.y = OffsetY;
		OffsetX += Gap;
	}

	if (UsingVertical)
	{
		OffsetY += Gap;
		OffsetX = -(float)(ShowNumArmor - 1) / 2.f * Gap;
	}

	for (int i = 0; i < ShowNumArmor; i++)
	{
		m_apArmorIndicator[i].m_Pos.x = OffsetX;
		m_apArmorIndicator[i].m_Pos.y = OffsetY;
		OffsetX += Gap;
	}
}

void CHealthBar::Snap(int SnappingClient)
{
	for (int i = 0; i < m_NumHeartsIndicator; i++)
		m_apHeartsIndicator[i].Snap(SnappingClient);
	for (int i = 0; i < m_NumArmorIndicator; i++)
		m_apArmorIndicator[i].Snap(SnappingClient);
}
