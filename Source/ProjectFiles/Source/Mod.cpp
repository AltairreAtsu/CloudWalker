#include "GameAPI.h"
#include <iostream>
#include <fstream>
#include <string>

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
const int Save_Tick_Interval = 10;
const int Decent_Tick_Interval = 5;
const int Accent_Tick_Interval = 5;
const int Hand_Trigger_Distance_Threshold = 15;
const double Rise_Height_Trigger_Threshold = .30;
const int Player_Sunk_Off_Platform_Threshold = -50;

bool cloudWalkingEnabled = false;
int playerHeight = 175;
int platformRadius = 3;
int progressToBlock = 0;
int progressToSave = 0;
int16_t platformHeight = 0;

UniqueID ThisModUniqueIDs[] = { Cloud_Walker_Block, Height_Calibrator_Block, Cloud_Block };

struct Cloud 
{
	CoordinateInBlocks location;
	BlockInfo originalBlock;

	void RestoreBlock() {
		SetBlock(location, originalBlock);
	}
};

std::vector<Cloud> platformCoords;

// Utility methods
//********************************
static bool IsBlockCloudReplacable(CoordinateInBlocks At) 
{
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

std::wstring GetFilePath() 
{
	wchar_t path[MAX_PATH];
	HMODULE hm = NULL;

	if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCWSTR)&IsBlockCloudReplacable, &hm) == 0)
	{
		Log(L"Failed");

	}
	if (GetModuleFileNameW(hm, path, sizeof(path)) == 0)
	{
		Log(L"Failed");
	}
	
	std::wstring p = path;
	size_t found = p.find_last_of(L"/\\");
	std::wstring p1 = p.substr(0, found);
	p1 = p1 + std::wstring(L"\\") + GetWorldName() + L".txt";
	return p1;
}

CoordinateInBlocks GetBlockUnderPlayerFoot() 
{
	return GetPlayerLocation() - CoordinateInCentimeters(0,0,25);
}

bool IsPointInCircle(int64_t circleX, int64_t circleY, int64_t radius, int64_t x, int64_t y) 
{
	return ((x - circleX) * (x - circleX) + (y - circleY) * (y - circleY) <= radius * radius);
}

std::vector<CoordinateInBlocks> GetAllPointsInCircle(CoordinateInBlocks At, int radius) 
{
	std::vector<CoordinateInBlocks> circleCords;
	int64_t centerX = At.X;
	int64_t centerY = At.Y;

	for (int64_t y = centerY - radius; y <= centerY + radius; y++) {
		for (int64_t x = centerX - radius; x <= centerX + radius; x++) {
			if (IsPointInCircle(centerX, centerY, radius, x, y) ) {
				circleCords.push_back((CoordinateInBlocks(x, y, At.Z)));
			}
		}
	}
	circleCords.push_back(At);
	return circleCords;
}

int GetDistanceBetweenHands() 
{
	CoordinateInCentimeters rightHandLocation = GetHandLocation(false);
	CoordinateInCentimeters leftHandLocation = GetHandLocation(true);

	int64_t distanceX = rightHandLocation.X - leftHandLocation.X;
	int64_t distanceY = rightHandLocation.Y - leftHandLocation.Y;
	int16_t distanceZ = rightHandLocation.Z - leftHandLocation.Z;

	return sqrt((distanceX * distanceX) + (distanceY * distanceY) + (distanceZ * distanceZ));
}

bool IsHandAboveRiseThreshold() 
{
	return GetHandLocation(false).Z >= GetPlayerLocationHead().Z - (playerHeight * Rise_Height_Trigger_Threshold);
}

// File Methods
//********************************
std::string BoolToString(bool value) 
{
	return (value) ? "1" : "0";
}
bool StringToBool(std::string string) 
{
	return string == "1";
}

std::string CoordinateToString(const CoordinateInBlocks& coord) 
{
	return std::to_string(coord.X) + "," + std::to_string(coord.Y) + "," + std::to_string(coord.Z);
}
CoordinateInBlocks StringToCoordinate(std::string text) 
{
	std::string delimeter = ",";
	size_t pos = text.find(delimeter);
	std::string token = text.substr(0, pos);
	text.erase(0, pos + delimeter.length());
	int x = std::stoi(token);

	pos = text.find(delimeter);
	token = text.substr(0, pos);
	text.erase(0, pos + delimeter.length());
	int y = std::stoi(token);

	pos = text.find(delimeter);
	token = text.substr(0, pos);
	text.erase(0, pos + delimeter.length());
	int z = std::stoi(token);

	return CoordinateInBlocks(x, y, z);
}

std::string BlockInfoToString(BlockInfo block) 
{
	return std::to_string((uint8_t)block.Type);
}
BlockInfo StringToBlockInfo(std::string text) 
{
	return BlockInfo((EBlockType)stoi(text));
}

std::string BlockCordToString(const Cloud& cloud) 
{
	return CoordinateToString(cloud.location) + " " + BlockInfoToString(cloud.originalBlock);
}
Cloud StringToBlockCoord(std::string text) 
{
	std::string delimeter = " ";
	size_t pos = text.find(delimeter);
	std::string coordinateText = text.substr(0, pos);
	text.erase(0, pos + delimeter.length());

	CoordinateInBlocks coord = StringToCoordinate(coordinateText);
	BlockInfo block = StringToBlockInfo(text);

	return Cloud(coord, block);
}

std::string PlatformToString() 
{
	std::string platformString;
	for (int i = 0; i <= platformCoords.size(); i++) 
	{
		platformString += BlockCordToString(platformCoords[i]) + std::string("\n");
	}
	return platformString;
}

void SaveData() 
{
	std::fstream saveFile;
	saveFile.open(GetFilePath(), std::ios::out);
	if (saveFile.is_open()) 
	{
		saveFile << std::to_string(playerHeight) + "\n";
		saveFile << BoolToString(cloudWalkingEnabled) + "\n";
		saveFile << std::to_string(platformRadius) + "\n";
		if(platformCoords.size() > 0)
			saveFile << PlatformToString();
		saveFile.close();
	}
}

void LoadData() 
{
	std::fstream saveFile;
	saveFile.open(GetFilePath(), std::ios::in);
	if (saveFile.is_open()) 
	{
		std::string line;
		std::getline(saveFile, line);
		playerHeight = std::stoi(line);

		std::getline(saveFile, line);
		cloudWalkingEnabled = StringToBool(line);

		std::getline(saveFile, line);
		platformRadius = std::stoi(line);

		while (std::getline(saveFile, line)) 
		{
			platformCoords.push_back(StringToBlockCoord(line));
		}

		saveFile.close();
	}
}

// Height Calibration
//********************************
bool SetPlayerHeightFromCalibrator(CoordinateInBlocks calibratorLocation) {
	int16_t newPlayerHeight = GetPlayerLocationHead().Z - GetPlayerLocation().Z;

	if (newPlayerHeight < 0) 
	{
		SpawnHintText(calibratorLocation + CoordinateInBlocks(0, 0, 1), L"Calibration Failed. Please stand in front of the calibrator.", 1, 1);
		return false;
	}
	else 
	{
		playerHeight = newPlayerHeight;
		SpawnHintText(calibratorLocation + CoordinateInBlocks(0, 0, 1), L"Calibration Sucessful.", 1, 1);
		return true;
	}
}

// Platform Control Methods
//********************************
void RemovePlatform() 
{
	for (int i = 0; i < platformCoords.size(); i++) 
	{
		platformCoords[i].RestoreBlock();
	}
	platformCoords.clear();
}

void SetCloudBlock(CoordinateInBlocks location) 
{
	BlockInfo currentBlock = GetAndSetBlock(location, Cloud_Block);
	platformCoords.push_back( Cloud(location, currentBlock));
}

void PruneOldClouds(CoordinateInBlocks centerBlock) 
{
	if (platformCoords.size() != 0) {
		for (int i = 0; i < platformCoords.size(); i++) {
			bool IsOnAcceptableZLevel = platformCoords[i].location.Z == centerBlock.Z || platformCoords[i].location.Z == centerBlock.Z - 1;

			if (!IsOnAcceptableZLevel ||
				!IsPointInCircle(centerBlock.X, centerBlock.Y, platformRadius, platformCoords[i].location.X, platformCoords[i].location.Y)) 
			{

				platformCoords[i].RestoreBlock();
				platformCoords.erase(platformCoords.begin() + i);
			}
		}
	}
}
void GeneratePlatformPlane(std::vector<CoordinateInBlocks> coords) 
{
	for (int i = 0; i < coords.size(); i++) 
	{
		BlockInfo currentBlock = GetBlock(coords[i]);
		if (IsBlockCloudReplacable(coords[i])) 
		{
			SetCloudBlock(coords[i]);
		}
	}
}

void GeneratePlatform(CoordinateInBlocks centerBlock) 
{
	std::vector newPlatformTopPlaneCoords = GetAllPointsInCircle(centerBlock, platformRadius);
	std::vector newPlatformBottomPlaneCoords = GetAllPointsInCircle(centerBlock - CoordinateInBlocks(0,0,1), platformRadius);

	PruneOldClouds(centerBlock);

	GeneratePlatformPlane(newPlatformBottomPlaneCoords);
	GeneratePlatformPlane(newPlatformTopPlaneCoords);
}

void PurgeClouds(CoordinateInBlocks At)
{
	RemovePlatform();
	auto coords = GetAllCoordinatesInRadius(At, 10);
	for (int i = 0; i < coords.size(); i++) 
	{
		BlockInfo block = GetBlock(coords[i]);
		if (block.CustomBlockID == Cloud_Block) 
		{
			SetBlock(coords[i], EBlockType::Air);
		}
	}
}

// Setters and Variable Management
//********************************
bool SetPlatformHeight(int16_t newHeight) 
{
	if (newHeight < World_Min_Height || newHeight > World_Max_Height) 
	{
		return false;
	}
	platformHeight = newHeight;
	return true;
}

void ToggleCloudWalking(CoordinateInBlocks At) 
{
	cloudWalkingEnabled = !cloudWalkingEnabled;

	if (!cloudWalkingEnabled) 
	{
		RemovePlatform();
	}
	std::wstring message = (cloudWalkingEnabled) ? L"Cloud Walking Enabled" : L"Cloud Walking Disabled";
	SpawnHintText(At + CoordinateInBlocks(0, 0, 1), message, 1, 1);
}

void CyclePlatformRadius() 
{
	platformRadius++;
	if (platformRadius > Maximum_Platform_Radius) 
	{
		platformRadius = Minimum_Platform_Radius;
	}
}

// Must have access to Setters
//********************************
void TeleportToNearestSolidBlockBelow() {
	CoordinateInBlocks playerLocation = GetPlayerLocation();
	int i = 0;
	BlockInfo block;
	while (true) {
		if (playerLocation.Z - i <= 0) break;
		CoordinateInBlocks coord = CoordinateInBlocks(playerLocation.X, playerLocation.Y, playerLocation.Z - i);
		block = GetBlock(coord);
		if (block.Type != EBlockType::Air && block.CustomBlockID != Cloud_Block) {
			SetPlayerLocation(coord + CoordinateInBlocks(0, 0, 1));
			SetPlatformHeight(coord.Z);
			break;
		}
		i++;
	}
}

/************************************************************* 
//	Functions (Run automatically by the game, you can put any code you want into them)
*************************************************************/


void Event_BlockHitByTool(CoordinateInBlocks At, UniqueID CustomBlockID, wString ToolName, CoordinateInCentimeters ExactHitLocation, bool ToolHeldByHandLeft)
{
	if (CustomBlockID == Cloud_Block) 
	{
		if ( ToolName == L"T_Axe_Stone" || ToolName == L"T_Axe_Copper" || ToolName == L"T_Axe_Iron") {
			PurgeClouds(At);
		}
		if (ToolName == L"T_Pickaxe_Stone" || ToolName == L"T_Pickaxe_Copper" || ToolName == L"T_Pickaxe_Iron") {
			TeleportToNearestSolidBlockBelow();
		}
		else if (ToolName == L"T_Arrow") 
		{
			CyclePlatformRadius();
		}
		else if (ToolName == L"T_Stick")
		{
			ToggleCloudWalking(At);
		}
	}

	if (CustomBlockID == Height_Calibrator_Block) {
		if (ToolName == L"T_Stick") 
		{
			SetPlayerHeightFromCalibrator(At);
		}
	}

	if (CustomBlockID == Cloud_Walker_Block) 
	{
		if (ToolName == L"T_Stick") 
		{
			ToggleCloudWalking(At);
		}
		if (ToolName == L"T_Axe_Stone" || ToolName == L"T_Axe_Copper" || ToolName == L"T_Axe_Iron") 
		{
			PurgeClouds(At);
		}
		if (ToolName == L"T_Arrow") 
		{
			CoordinateInBlocks HeightCalibratorLocation = At + CoordinateInBlocks(1, 0, 0);
			BlockInfo currentBlock = GetBlock(HeightCalibratorLocation);
			if (currentBlock.Type == EBlockType::Air) 
			{
				SpawnHintText(At + CoordinateInBlocks(0, 0, 1), L"Please remember to stand up straight before calibrating your height.", 1, 1);
				SetBlock(HeightCalibratorLocation, Height_Calibrator_Block);
			}
			else if (currentBlock.CustomBlockID == Height_Calibrator_Block) 
			{
				SetBlock(HeightCalibratorLocation, EBlockType::Air);
			}
			
		}
	}
}

void Event_Tick()
{
	if (cloudWalkingEnabled) 
	{
		BlockInfo blockUnderFoot = GetBlock(GetBlockUnderPlayerFoot());

		if (blockUnderFoot.CustomBlockID != Cloud_Block &&
			blockUnderFoot.Type != EBlockType::Air) 
		{
			SetPlatformHeight(GetBlockUnderPlayerFoot().Z);
		}

		if (GetDistanceBetweenHands() <= Hand_Trigger_Distance_Threshold) 
		{
			if (IsHandAboveRiseThreshold()) 
			{
				progressToBlock++;
				if (progressToBlock >= Accent_Tick_Interval) 
				{
					progressToBlock = 0;
					platformHeight++;
				}
			}
			else {
				progressToBlock++;
				if (progressToBlock >= Decent_Tick_Interval)
				{
					progressToBlock = 0;
					platformHeight--;
				}
			}
		}

		CoordinateInCentimeters playerLocation = GetPlayerLocation();
		if ( playerLocation.Z - (platformHeight * 50) < Player_Sunk_Off_Platform_Threshold ) {
			SetPlayerLocation(CoordinateInCentimeters(playerLocation.X, playerLocation.Y, (platformHeight*50) + 25));
		}
		CoordinateInBlocks playerLocationInBlocks = GetPlayerLocation();
		CoordinateInBlocks platformCenter = CoordinateInBlocks(playerLocationInBlocks.X, playerLocationInBlocks.Y, platformHeight);

		GeneratePlatform(platformCenter);
	}

	

	progressToSave++;
	if (progressToSave >= Save_Tick_Interval) 
	{
		progressToSave = 0;
		SaveData();
	}


}

void Event_OnLoad()
{
	LoadData();
	if (cloudWalkingEnabled) {
		CoordinateInBlocks blockUnderFoot = GetBlockUnderPlayerFoot();
		SetPlatformHeight(blockUnderFoot.Z);
		
		CoordinateInBlocks playerLocation = GetPlayerLocation();
		PruneOldClouds(CoordinateInBlocks(playerLocation.X, playerLocation.Y, platformHeight));
	}
}

void Event_OnExit()
{
	// Only used for memory cleanup
}

void Event_BlockPlaced(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{}
void Event_BlockDestroyed(CoordinateInBlocks At, UniqueID CustomBlockID, bool Moved)
{
	if (CustomBlockID == Cloud_Walker_Block) 
	{
		if (GetBlock(At + CoordinateInBlocks(1, 0, 0)).CustomBlockID == Height_Calibrator_Block)
			SetBlock(At + CoordinateInBlocks(1, 0, 0), EBlockType::Air);
	}
}
/*******************************************************
Advanced functions
*******************************************************/
void Event_AnyBlockPlaced(CoordinateInBlocks At, BlockInfo Type, bool Moved)
{}
void Event_AnyBlockDestroyed(CoordinateInBlocks At, BlockInfo Type, bool Moved)
{}
void Event_AnyBlockHitByTool(CoordinateInBlocks At, BlockInfo Type, wString ToolName, CoordinateInCentimeters ExactHitLocation, bool ToolHeldByHandLeft)
{}
