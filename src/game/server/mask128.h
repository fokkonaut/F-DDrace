// made by fokkonaut

#include <engine/shared/protocol.h>

#ifndef MASK128
#define MASK128
struct Mask128
{
	int64_t m_aMask[2];

	Mask128()
	{
		m_aMask[0] = -1LL;
		m_aMask[1] = -1LL;
	}

	Mask128(const Mask128&) = default;

	Mask128(int ClientID)
	{
		m_aMask[0] = 0;
		m_aMask[1] = 0;

		if (ClientID == -1)
			return;

		if (ClientID < VANILLA_MAX_CLIENTS)
			m_aMask[0] = 1LL<<ClientID;
		else
			m_aMask[1] = 1LL<<(ClientID-VANILLA_MAX_CLIENTS);
	}

	Mask128(int64_t Mask0, int64_t Mask1)
	{
		m_aMask[0] = Mask0;
		m_aMask[1] = Mask1;
	}

	int64_t operator[](int ID)
	{
		return m_aMask[ID];
	}

	Mask128 operator~() const
	{
		return Mask128(~m_aMask[0], ~m_aMask[1]);
	}

	Mask128 operator^(Mask128 Mask)
	{
		return Mask128(m_aMask[0]^Mask[0], m_aMask[1]^Mask[1]);
	}

	Mask128 operator=(Mask128 Mask)
	{
		m_aMask[0] = Mask[0];
		m_aMask[1] = Mask[1];
		return *this;
	}

	void operator|=(Mask128 Mask)
	{
		m_aMask[0] |= Mask[0];
		m_aMask[1] |= Mask[1];
	}

	bool operator&(Mask128 Mask)
	{
		return (m_aMask[0]&Mask[0]) | (m_aMask[1]&Mask[1]);
	}

	void operator&=(Mask128 Mask)
	{
		m_aMask[0] &= Mask[0];
		m_aMask[1] &= Mask[1];
	}

	bool operator==(Mask128 Mask)
	{
		return m_aMask[0] == Mask[0] && m_aMask[1] == Mask[1];
	}
};
#endif
