// made by fokkonaut

/*   ! ALWAYS INCLUDE THIS FILE LAST !   */

// position and velocity
#define POS m_pCharacter->Core()->m_Pos
#define RAW_X POS.x
#define RAW_Y POS.y
#define X (RAW_X / 32)
#define Y (RAW_Y / 32)
#define VEL m_pCharacter->Core()->m_Vel
#define VEL_X VEL.x
#define VEL_Y VEL.y

// controls
#define RELEASE_MOVE m_pCharacter->Input()->m_Direction = 0
#define LEFT m_pCharacter->Input()->m_Direction = -1
#define RIGHT m_pCharacter->Input()->m_Direction = 1
#define JUMP m_pCharacter->Input()->m_Jump = 1
#define RELEASE_JUMP m_pCharacter->Input()->m_Jump = 0
#define HOOK m_pCharacter->Input()->m_Hook = 1
#define RELEASE_HOOK m_pCharacter->Input()->m_Hook = 0
#define TARGET_X(x) \
	do \
	{ \
		m_pCharacter->LatestInput()->m_TargetX = x; \
		m_pCharacter->Input()->m_TargetX = x; \
	} while(0)
#define TARGET_Y(y) \
	do \
	{ \
		m_pCharacter->LatestInput()->m_TargetY = y; \
		m_pCharacter->Input()->m_TargetY = y; \
	} while(0)
#define TARGET(x, y) \
	do \
	{ \
		TARGET_X(x); \
		TARGET_Y(y); \
	} while(0)
#define FIRE \
	do \
	{ \
		m_pCharacter->LatestInput()->m_Fire++; \
		m_pCharacter->Input()->m_Fire++; \
	} while(0)
#define RELEASE_FIRE \
	do \
	{ \
		m_pCharacter->LatestInput()->m_Fire = 0; \
		m_pCharacter->Input()->m_Fire = 0; \
	} while(0)

// other
#define TICKS_PASSED(ticks) (Server()->Tick() % (ticks) == 0)
#define _(pos) ((pos) * 32)
