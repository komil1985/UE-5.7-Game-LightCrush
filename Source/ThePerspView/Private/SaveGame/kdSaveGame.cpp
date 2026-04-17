// Copyright ASKD Games


#include "SaveGame/kdSaveGame.h"

UkdSaveGame::UkdSaveGame()
{
	// Level 0 (main menu) is always unlocked
	UnlockedLevels.Add(0);
	UnlockedLevels.Add(1);  // First playable level also unlocked by default
}

int32 UkdSaveGame::GetHighScore(int32 LevelIdx) const
{
	return LevelHighScores.IsValidIndex(LevelIdx) ? LevelHighScores[LevelIdx] : 0;
}

bool UkdSaveGame::TrySetHighScore(int32 LevelIdx, int32 Score)
{
	EnsureCapacity(LevelIdx);
	if (Score > LevelHighScores[LevelIdx])
	{
		LevelHighScores[LevelIdx] = Score;
		return true;
	}
	return false;
}

float UkdSaveGame::GetBestTime(int32 LevelIdx) const
{
	return LevelBestTimes.IsValidIndex(LevelIdx) ? LevelBestTimes[LevelIdx] : 0.f;
}

bool UkdSaveGame::TrySetBestTime(int32 LevelIdx, float Seconds)
{
	EnsureCapacity(LevelIdx);
	const float Existing = LevelBestTimes[LevelIdx];
	if (Existing <= 0.f || Seconds < Existing)
	{
		LevelBestTimes[LevelIdx] = Seconds;
		return true;
	}
	return false;
}

bool UkdSaveGame::IsLevelUnlocked(int32 LevelIdx) const
{
	return LevelIdx == 0 || UnlockedLevels.Contains(LevelIdx);
}

void UkdSaveGame::UnlockLevel(int32 LevelIdx)
{
	UnlockedLevels.AddUnique(LevelIdx);
}

void UkdSaveGame::EnsureCapacity(int32 LevelIdx)
{
	while (LevelHighScores.Num() <= LevelIdx) LevelHighScores.Add(0);
	while (LevelBestTimes.Num() <= LevelIdx) LevelBestTimes.Add(0.f);
}
