#include "GameAPI.h"

/************************************************************
	Config Variables (Set these to whatever you need. They are automatically read by the game.)
*************************************************************/
float TickRate = 10;

const int Cloud_Walker_Block = 3037;
const int Height_Calibrator_Block = 3038;
const int Cloud_Block = 3039;

bool cloudWalkingEnabled = true;
// Personally 180 but should drop some for the distance to the headset anyway?
int playerHeight = 175;
int platformRadius = 4;
std::vector<CoordinateInBlocks> platformCoords;

UniqueID ThisModUniqueIDs[] = { Cloud_Walker_Block, Height_Calibrator_Block, Cloud_Block };

// Utility methods
//********************************
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

	if (newPlayerHeight > 0) {
		SpawnHintText(calibratorLocation + CoordinateInBlocks(0, 0, 1), L"Calibration Failed. Please stand in front of the calibrator.", 1, 1);
		return false;
	}
	else {
		SpawnHintText(calibratorLocation + CoordinateInBlocks(0, 0, 1), L"Calibration Sucessful.", 1, 1);
		playerHeight = newPlayerHeight;
		return true;
	}
}

void RemovePlatform() {
	for (int i = 0; i < platformCoords.size(); i++) {
		SetBlock(platformCoords[i], EBlockType::Air);
	}
}

void GeneratePlatform(CoordinateInBlocks centerBlock) {
	// TODO, Enabling replacing flora with clouds, clouds should be able to restore disturbed flora. Will make it easier to take off.
	std::vector newPlatformCoords = GetAllPointsInCircle(centerBlock, platformRadius);

	if (platformCoords.size() != 0) {
		for (int i = 0; i < platformCoords.size(); i++) {
			if (platformCoords[i].Z != centerBlock.Z ||
				!IsPointInCircle(centerBlock.X, centerBlock.Y, platformRadius, platformCoords[i].X, platformCoords[i].Y)) {
				
				SetBlock(platformCoords[i], EBlockType::Air);
			}
		}
	}
	platformCoords.clear();

	for (int i = 0; i < newPlatformCoords.size(); i++) {
		BlockInfo currentBlock = GetBlock(newPlatformCoords[i]);
		if (currentBlock.Type == EBlockType::Air) {
			SetBlock(newPlatformCoords[i], Cloud_Block);
			platformCoords.push_back(newPlatformCoords[i]);

		}else if(currentBlock.CustomBlockID == Cloud_Block) {
			platformCoords.push_back(newPlatformCoords[i]);
		}
	}
}

void ToggleCloudWalking() {
	cloudWalkingEnabled = !cloudWalkingEnabled;

	if (!cloudWalkingEnabled) {
		RemovePlatform();
	}
}

/************************************************************* 
//	Functions (Run automatically by the game, you can put any code you want into them)
*************************************************************/


void Event_BlockHitByTool(CoordinateInBlocks At, UniqueID CustomBlockID, wString ToolName)
{
	if (CustomBlockID == Cloud_Block) {
		if (ToolName == L"T_Stick") {
			// Set Target Height +1
		}
		else if (ToolName == L"T_Pickaxe_Stone" || ToolName == L"T_Pickaxe_Copper" || ToolName == L"T_Pickaxe_Iron") {
			// Set Target Height -1
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
	}
}

void Event_Tick()
{
	if (cloudWalkingEnabled) {
		// TODO Gradually transition up and down to target heights, pace the speed of the transition to allow the player to asced
		// or descend with the platform.
		GeneratePlatform(GetBlockUnderPlayerFoot());
	}
}

void Event_OnLoad()
{
	// Load the Player Height and cloud walking sate from Saved data
	// If the player is cloud walking load the cloud loations and carry on
}

void Event_OnExit()
{
	// TEMPORARY FUNCTION
	if (platformCoords.size() > 0) {
		RemovePlatform();
	}
	
	// Prune excess clouds
	// Save Player Height, cloud walking state, and cloud locations to Saved Data.
}

void Event_BlockPlaced(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{
	
}
void Event_BlockDestroyed(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{

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
