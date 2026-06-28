#ifndef GAME_SERVER_MISC_FLOOD_DETECTOR_H
#define GAME_SERVER_MISC_FLOOD_DETECTOR_H
#include <base/system.h>
#include <math.h>

class CFloodDetector
{
	float m_Short = 0.0f;
	float m_Med = 0.0f;
	float m_Long = 0.0f;

	float SHORT_THRESH = 0.0f;
	float MED_THRESH = 0.0f;
	float LONG_THRESH = 0.0f;

	static constexpr float RECOVERY_SILENCE = 120.0f;

	int64 m_LastEvent = 0;
	int64 m_LastTick  = 0;
	bool m_HasEvent = false;

	static float Decay(float Dt, float Tau)
	{
		return (Dt >= Tau * 5.0f) ? 0.0f : expf(-Dt / Tau);
	}

public:
	void Init()
	{
		m_Short = m_Med = m_Long = 0.0f;
		m_HasEvent = false;
		m_LastEvent = m_LastTick = time_get();
	}

	void SetThresholds(int Short, int Medium, int Long)
	{
		SHORT_THRESH = Short / 1000.f;
		MED_THRESH = Medium / 1000.f;
		LONG_THRESH = Long / 1000.f;
	}

	void RecordEvent()
	{
		int64 Now = time_get();
		if (!m_HasEvent)
		{
			m_LastEvent = m_LastTick = Now;
			m_HasEvent = true;
			return;
		}

		float Dt = (float)(Now - m_LastEvent) / (float)time_freq();
		if (Dt < 0.001f)
			Dt = 0.001f;

		float Inst = 1.0f / Dt;

		float DS = Decay(Dt, 3.0f);
		float DM = Decay(Dt, 20.0f);
		float DL = Decay(Dt, 60.0f);

		m_Short = DS * m_Short + (1.0f - DS) * Inst;
		m_Med = DM * m_Med + (1.0f - DM) * Inst;
		m_Long = DL * m_Long + (1.0f - DL) * Inst;

		m_LastEvent = m_LastTick = Now;
	}

	void Tick()
	{
		if (!m_HasEvent)
			return;

		int64 Now = time_get();
		float DtSec = (float)(Now - m_LastTick) / (float)time_freq();
		if (DtSec < 0.01f)
			return;

		float Silence = (float)(Now - m_LastEvent) / (float)time_freq();
		if (Silence > RECOVERY_SILENCE)
		{
			Reset();
			return;
		}

		m_Short *= Decay(DtSec, 3.0f);
		m_Med *= Decay(DtSec, 20.0f);
		m_Long *= Decay(DtSec, 60.0f);
		m_LastTick = Now;
	}

	bool IsFlooded() const
	{
		return m_HasEvent
		&& (m_Short > SHORT_THRESH
		|| m_Med > MED_THRESH
		|| m_Long > LONG_THRESH);
	}

	float GetScore() const
	{
		float S = m_Short / SHORT_THRESH;
		float M = m_Med / MED_THRESH;
		float L = m_Long / LONG_THRESH;
		return S > M ? (S > L ? S : L) : (M > L ? M : L);
	}

	void Reset()
	{
		m_Short = m_Med = m_Long = 0.0f;
		m_HasEvent = false;
	}
};
#endif
