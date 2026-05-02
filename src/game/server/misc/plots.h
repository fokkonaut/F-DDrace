// made by fokkonaut

#ifndef GAME_SERVER_MISC_PLOTS_H
#define GAME_SERVER_MISC_PLOTS_H

#include <engine/shared/protocol.h>
#include <generated/protocol.h>
#include <vector>
#include <base/vmath.h>
#include <game/collision.h>
#include <game/server/entities/map/draweditor/drawtile.h>

class CGameContext;
class CGameWorld;
class IServer;
class CEntity;
class CConfig;

class CPlots
{
	CGameContext *m_pGameServer;
	CGameContext *GameServer() const;
	IServer *Server() const;
	CConfig *Config() const;
	CCollision *Collision() const;
	CGameWorld *GameWorld() const;

	enum PlotVariables
	{
		PLOT_OWNER_ACC_USERNAME,
		PLOT_DISPLAY_NAME,
		PLOT_EXPIRE_DATE,
		PLOT_DOOR_STATUS,
		PLOT_OBJECTS,
		NUM_PLOT_VARIABLES
	};

	struct SPlot
	{
		char m_aOwner[32];
		char m_aDisplayName[32];
		time_t m_ExpireDate;

		int m_Size;
		vec2 m_ToTele;
		std::vector<CEntity *> m_vObjects;
		int64 m_DestroyEndTick;
		int m_DoorHealth;
	} m_aPlots[MAX_PLOTS];

	static int LoadPresetListCallback(const char *pName, int IsDir, int StorageType, void *pUser);

	void ReadPlotStats(int ID);
	void WritePlotStats(int ID);
	void ExpirePlots();

	//void SetPlotDrawDoorStatus(int Number, bool Close);
	//bool IsPlotEmpty(int PlotID);

public:
	void Init(CGameContext *pGameServer);
	void LoadData();
	void WriteData();
	void Tick();

	void InitPlot(int PlotID, vec2 ToTele, int Size);

	std::vector<CEntity *> ReadPlotObjects(const char *pLine, int PlotID);
	void WritePlotObject(CEntity *pEntity, std::ofstream *pFile, vec2 *pPos = 0);

	void SetPlotInfo(int PlotID, int AccID);
	void SetPlotExpire(int PlotID);

	int GetMaxPlotSpeedups(int PlotID);
	int GetMaxPlotTeleporters(int PlotID);
	unsigned int GetMaxPlotObjects(int PlotID);
	const char *GetPlotSizeString(int PlotID);

	void SetPlotDoorStatus(int PlotID, bool Close);
	void SetPlotDrawDoorStatus(int PlotID, int Door, bool Close);
	void ClearPlot(int PlotID);
	int GetPlotID(int AccID);
	int GetTilePlotID(vec2 Pos, bool CheckDoor = false);

	bool HasPlotByIP(int ClientID);

	void RemovePortalsFromPlot(int PlotID);

	bool PlotCanBeRaided(int PlotID);
	bool PlotDoorDestroyed(int PlotID);
	bool OnPlotDoorTaser(int PlotID, int TaserStrength, int ClientID, vec2 Pos);
	bool SetPlotDestroyEndTick(int PlotID, int64 NewEndTick);

	void PrintPlotInfo(int PlotID);

	bool CanInsertNewEntity(int PlotID);
	bool InsertPlotDrawEntity(CEntity *pEntity);
	bool ErasePlotDrawEntity(CEntity *pEntity);

	unsigned int NumPlotObjects(int PlotID) { return m_aPlots[PlotID].m_vObjects.size(); }
	CEntity *GetPlotObject(int PlotID, int Index) { return m_aPlots[PlotID].m_vObjects[Index]; }
	const char *GetPlotExpireDate(int PlotID);

	vec2 GetToTele(int PlotID) { return m_aPlots[PlotID].m_ToTele; }
	int GetSize(int PlotID) { return m_aPlots[PlotID].m_Size; }
	const char *GetOwner(int PlotID) { return m_aPlots[PlotID].m_aOwner; }
	const char *GetDisplayName(int PlotID) { return m_aPlots[PlotID].m_aDisplayName; }
	int GetDoorHealth(int PlotID) { return m_aPlots[PlotID].m_DoorHealth; }
	int64 GetDestroyEndTick(int PlotID) { return m_aPlots[PlotID].m_DestroyEndTick; }

	CDrawTile *HasDrawTile(int MapIndex, CDrawTile *pMatch = 0);

	// draweditor preset list
	std::vector<std::string> m_vPresetList;
};
#endif //GAME_SERVER_MISC_PLOTS_H
