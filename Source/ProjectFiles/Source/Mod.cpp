#include "GameAPI.h"

/************************************************************
	Config Variables (Set these to whatever you need. They are automatically read by the game.)
*************************************************************/
float TickRate = 10;

const int Cloud_Walker_Block = 3037;
const int Height_Calibrator_Block = 3038;
const int Cloud_Block = 3039;
const int Minimum_Platform_Radius = 2;
const int Maximum_Platform_Radius = 4;
const int World_Max_Height = 720;
const int World_Min_Height = 0;

bool cloudWalkingEnabled = true;
bool platformIsLanding = false;
int playerHeight = 175;
int platformRadius = 4;
int progressToBlock = 0;
int16_t platformHeight = 0;

UniqueID ThisModUniqueIDs[] = { Cloud_Walker_Block, Height_Calibrator_Block, Cloud_Block };

struct Cloud {
	CoordinateInBlocks location;
	BlockInfo originalBlock;

	void RestoreBlock() {
		SetBlock(location, originalBlock);
	}
};

std::vector<Cloud> platformCoords;

// Utility methods
//********************************
static bool IsBlockCloudReplacable(CoordinateInBlocks At) {
	BlockInfo block = GetBlock(At);

	if (block.Type == EBlockType::Air
		|| block.Type == EBlockType::GrassFoliage
		|| block.Type == EBlockType::Flower1
		|| block.Type == EBlockType::Flower2
		|| block.Type == EBlockType::Flower3
		|| block.Type == EBlockType::Flower4
		|| block.Type == EBlockType::FlowerRainbow)
	{
		return true;
	}
	return false;
}

std::wstring GetPath() {
	LPWSTR path;
	HMODULE hm = NULL;

	if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCWSTR)&IsBlockCloudReplacable, &hm) == 0)
	{
		int ret = GetLastError();
		fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
		// Return or however you want to handle an error.
	}
	if (GetModuleFileName(hm, path, sizeof(path)) == 0)
	{
		int ret = GetLastError();
		fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
		// Return or however you want to handle an error.
	}
	return (std::wstring)path;
}

CoordinateInBlocks GetBlockUnderPlayerFoot() {
	return GetPlayerLocation() - CoordinateInCentimeters(0, 0, playerHeight + 25);
}

bool IsPointInCircle(int circleX, int circleY, int radius, int x, int y) {
	return ((x - circleX) * (x - circleX) + (y - circleY) * (y - circleY) <= radius * radius);
}

std::vector<CoordinateInBlocks> GetAllPointsInCircle(CoordinateInBlocks At, int radius) {
	std::vector<CoordinateInBlocks> circleCords;
	int centerX = At.X;
	int centerY = At.Y;

	for (int y = centerY - radius; y <= centerY + radius; y++) {
		for (int x = centerX - radius; x <= centerX + radius; x++) {
			if (IsPointInCircle(centerX, centerY, radius, x, y) ) {
				circleCords.push_back((CoordinateInBlocks(x, y, At.Z)));
			}
		}
	}
	circleCords.push_back(At);
	return circleCords;
}

bool SetPlayerHeightFromCalibrator(CoordinateInBlocks calibratorLocation) {
	CoordinateInCentimeters groundLocation = calibratorLocation - CoordinateInCentimeters(0, 0, 25);
	CoordinateInCentimeters playerLocation = GetPlayerLocation();
	int newPlayerHeight = playerLocation.Z - groundLocation.Z;

	if (newPlayerHeight < 0) {
		SpawnHintText(calibratorLocation + CoordinateInBlocks(0, 0, 1), L"Calibration Failed. Please stand in front of the calibrator.", 1, 1);
		return false;
	}
	else {
		playerHeight = newPlayerHeight;
		SpawnHintText(calibratorLocation + CoordinateInBlocks(0, 0, 1), L"Calibration Sucessful.", 1, 1);
		return true;
	}
}

void RemovePlatform() {
	for (int i = 0; i < platformCoords.size(); i++) {
		platformCoords[i].RestoreBlock();
	}
	platformCoords.clear();
}

void SetCloudBlock(CoordinateInBlocks location) {
	BlockInfo currentBlock = GetBlock(location);
	SetBlock(location, Cloud_Block);
	platformCoords.push_back( Cloud(location, currentBlock));
}

void PruneOldClouds(CoordinateInBlocks centerBlock) {
	if (platformCoords.size() != 0) {
		for (int i = 0; i < platformCoords.size(); i++) {
			bool IsOnAcceptableZLevel = platformCoords[i].location.Z == centerBlock.Z || platformCoords[i].location.Z == centerBlock.Z - 1;

			if (!IsOnAcceptableZLevel ||
				!IsPointInCircle(centerBlock.X, centerBlock.Y, platformRadius, platformCoords[i].location.X, platformCoords[i].location.Y)) {

				platformCoords[i].RestoreBlock();
				platformCoords.erase(platformCoords.begin() + i);
			}
		}
	}
}
void GeneratePlatformPlane(std::vector<CoordinateInBlocks> coords) {
	for (int i = 0; i < coords.size(); i++) {
		BlockInfo currentBlock = GetBlock(coords[i]);
		if (IsBlockCloudReplacable(coords[i])) {
			SetCloudBlock(coords[i]);
		}
	}
}

void GeneratePlatform(CoordinateInBlocks centerBlock) {
	std::vector newPlatformTopPlaneCoords = GetAllPointsInCircle(centerBlock, platformRadius);
	std::vector newPlatformBottomPlaneCoords = GetAllPointsInCircle(centerBlock - CoordinateInBlocks(0,0,1), platformRadius);

	PruneOldClouds(centerBlock);

	GeneratePlatformPlane(newPlatformBottomPlaneCoords);
	GeneratePlatformPlane(newPlatformTopPlaneCoords);
}

void ToggleCloudWalking() {
	cloudWalkingEnabled = !cloudWalkingEnabled;

	if (!cloudWalkingEnabled) {
		RemovePlatform();
	}
}

void CyclePlatformRadius() {
	platformRadius++;
	if (platformRadius < Maximum_Platform_Radius) {
		platformRadius = Minimum_Platform_Radius;
	}
}

bool SetPlatformHeight(int16_t newHeight) {
	if (newHeight > World_Min_Height || newHeight < World_Max_Height) {
		return false;
	}
	platformHeight = newHeight;
	return true;
}

/************************************************************* 
//	Functions (Run automatically by the game, you can put any code you want into them)
*************************************************************/


void Event_BlockHitByTool(CoordinateInBlocks At, UniqueID CustomBlockID, wString ToolName)
{
	if (CustomBlockID == Cloud_Block) {
		if (ToolName == L"T_Stick") {
			platformIsLanding = !platformIsLanding;
		}
		if (!platformIsLanding) {
			if (ToolName == L"T_Axe_Stone" || ToolName == L"T_Axe_Copper" || ToolName == L"T_Axe_Iron") {
				SetPlatformHeight(platformHeight + 1);
			}
			else if (ToolName == L"T_Pickaxe_Stone" || ToolName == L"T_Pickaxe_Copper" || ToolName == L"T_Pickaxe_Iron") {
				SetPlatformHeight(platformHeight - 1);
			}
			else if (ToolName == L"T_Arrow") {
				CyclePlatformRadius();
			}
		}
	}

	if (CustomBlockID == Height_Calibrator_Block) {
		if (ToolName == L"T_Stick") {
			SetPlayerHeightFromCalibrator(At);
		}
	}

	if (CustomBlockID == Cloud_Walker_Block) {
		if (ToolName == L"T_Stick") {
			ToggleCloudWalking();
		}
		if (ToolName == L"T_Arrow") {
			CoordinateInBlocks HeightCalibratorLocation = At + CoordinateInBlocks(1, 0, 0);
			BlockInfo currentBlock = GetBlock(HeightCalibratorLocation);
			if (currentBlock.Type == EBlockType::Air) {
				SpawnHintText(At + CoordinateInBlocks(0, 0, 1), L"Please remember to stand up straight and keep your feet level with the calibrator before calibrating your height.", 1, 1);
				SetBlock(HeightCalibratorLocation, Height_Calibrator_Block);
			}
			else if (currentBlock.CustomBlockID == Height_Calibrator_Block) {
				SetBlock(HeightCalibratorLocation, EBlockType::Air);
			}
			
		}
	}
}

void Event_Tick()
{
	if (cloudWalkingEnabled) {
		CoordinateInBlocks playerLocation = GetPlayerLocation();
		BlockInfo blockUnderFoot = GetBlock(GetBlockUnderPlayerFoot());

		if (blockUnderFoot.CustomBlockID != Cloud_Block &&
			blockUnderFoot.Type != EBlockType::Air) {
			SetPlatformHeight(GetBlockUnderPlayerFoot().Z);
			platformIsLanding = false;
		}
		if (platformIsLanding) {
			progressToBlock++;
			if (progressToBlock <= 4) {
				progressToBlock = 0;
				SetPlatformHeight(platformHeight - 1);
			}
		}

		CoordinateInBlocks platformCenter = CoordinateInBlocks(playerLocation.X, playerLocation.Y, platformHeight);

		GeneratePlatform(platformCenter);
	}
}

void Event_OnLoad()
{
	// Load the Player Height, cloud walking, and platform radius sate from Saved data
	// If the player is cloud walking load the cloud loations and carry on
}

void Event_OnExit()
{
	// TEMPORARY FUNCTION
	if (platformCoords.size() > 0) {
		RemovePlatform();
	}
	
	// Prune excess clouds
	// Save Player Height, cloud walking state, platform radius, and cloud locations to Saved Data.
}

void Event_BlockPlaced(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{
	
}
void Event_BlockDestroyed(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{
	if (CustomBlockID == Cloud_Walker_Block) {
		if (GetBlock(At + CoordinateInBlocks(1, 0, 0)).CustomBlockID == Height_Calibrator_Block)
			SetBlock(At + CoordinateInBlocks(1, 0, 0), EBlockType::Air);
	}
}
/*******************************************************
Advanced functions
*******************************************************/
void Event_AnyBlockPlaced(CoordinateInBlocks At, BlockInfo Type, bool Moved)
{

}
void Event_AnyBlockDestroyed(CoordinateInBlocks At, BlockInfo Type, bool Moved)
{

}
void Event_AnyBlockHitByTool(CoordinateInBlocks At, BlockInfo Type, wString ToolName)
{
}
