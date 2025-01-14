// Fill out your copyright notice in the Description page of Project Settings.

#include "World/MyPlayerState.h"
#include "World/MyGameState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UnrealNames.h"
#include "CoopArenaGameInstance.h"
#include "MyPlayerController.h"


/////////////////////////////////////////////////////
AMyPlayerState::AMyPlayerState(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	_TeamNumber = 0;
	_NumKills = 0;
	_NumDeaths = 0;
	Score = 0;
	_bIsAlive = false;
	_bIsReady = false;
	bUseCustomPlayerNames = true;
}

/////////////////////////////////////////////////////
void AMyPlayerState::Reset()
{
	Super::Reset();

	_NumKills = 0;
	_NumDeaths = 0;
	Score = 0;
	_bIsAlive = false;
	_bIsReady = false;
}

/////////////////////////////////////////////////////
void AMyPlayerState::ScorePoints(int32 Points, bool bAddKill /*= true*/)
{
	Score += Points;

	AMyGameState* gameState = GetWorld()->GetGameState<AMyGameState>();
	ensureMsgf(gameState, TEXT("Game state is not from class 'AMyGameState'"));
	gameState->AddScore(Points);

	if (bAddKill)
	{
		AddKill();
	}
}

/////////////////////////////////////////////////////
void AMyPlayerState::SetTeamNumber(int32 NewTeamNumber)
{
	_TeamNumber = NewTeamNumber;
}

int32 AMyPlayerState::GetTeamNumber() const
{
	return _TeamNumber;
}

/////////////////////////////////////////////////////
void AMyPlayerState::AddKill()
{
	_NumKills++;
}

int32 AMyPlayerState::GetKills() const
{
	return _NumKills;
}

/////////////////////////////////////////////////////
void AMyPlayerState::AddDeath()
{
	_NumDeaths++;
}

int32 AMyPlayerState::GetDeaths() const
{
	return _NumDeaths;
}

/////////////////////////////////////////////////////
float AMyPlayerState::GetScore() const
{
	return Score;
}

/////////////////////////////////////////////////////
bool AMyPlayerState::IsAlive() const
{
	return _bIsAlive;
}

/////////////////////////////////////////////////////
bool AMyPlayerState::IsReady() const
{
	return _bIsReady;
}

/////////////////////////////////////////////////////
void AMyPlayerState::SetPlayerName_Server_Implementation(const FString& NewPlayerName)
{
	if (!NewPlayerName.IsEmpty())
	{
		SetPlayerName(NewPlayerName);
	}
}

bool AMyPlayerState::SetPlayerName_Server_Validate(const FString& NewPlayerName)
{
	return true;
}

/////////////////////////////////////////////////////
void AMyPlayerState::SeamlessTravelTo(class APlayerState* NewPlayerState)
{
	FString playerNameTemp = GetPlayerName();
	Super::SeamlessTravelTo(NewPlayerState);
	NewPlayerState->SetPlayerName(playerNameTemp);
}

/////////////////////////////////////////////////////
void AMyPlayerState::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* pc = GetGameInstance()->GetPrimaryPlayerController();
	if (pc && pc->IsLocalController() && pc->PlayerState == this)
	{
		UCoopArenaGameInstance* gameInstance = Cast<UCoopArenaGameInstance>(GetGameInstance());
		FString customPlayerName = gameInstance->GetSavedPlayerName();
		if (!customPlayerName.IsEmpty())
		{
			SetPlayerName_Server(customPlayerName);
		}
	}
}

/////////////////////////////////////////////////////
void AMyPlayerState::OnReadyStatusReplicated()
{
	OnPlayerChangedReadyStatus.Broadcast(this, _bIsReady);
}

/////////////////////////////////////////////////////
void AMyPlayerState::SetReadyStatus_Server_Implementation(bool bReady)
{
	_bIsReady = bReady;
	OnReadyStatusReplicated();
}

bool AMyPlayerState::SetReadyStatus_Server_Validate(bool bReady)
{
	return true;
}

/////////////////////////////////////////////////////
void AMyPlayerState::ToggleReadyStatus_Server_Implementation()
{
	_bIsReady = !_bIsReady;
	OnReadyStatusReplicated();
}

bool AMyPlayerState::ToggleReadyStatus_Server_Validate()
{
	return true;
}

/////////////////////////////////////////////////////
void AMyPlayerState::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMyPlayerState, _NumKills);
	DOREPLIFETIME(AMyPlayerState, _NumDeaths);
	DOREPLIFETIME(AMyPlayerState, _TeamNumber);
	DOREPLIFETIME(AMyPlayerState, _bIsAlive);
	DOREPLIFETIME(AMyPlayerState, _bIsReady);
}