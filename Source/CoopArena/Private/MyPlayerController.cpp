#include "MyPlayerController.h"
#include "MyGameState.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "MyPlayerState.h"
#include "PlayerCharacter.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "DefaultHUD.h"
#include "TimerManager.h"


/////////////////////////////////////////////////////
void AMyPlayerController::StartSpectating(AActor* ActorToSpectate /*= nullptr*/)
{
	ensureMsgf(HasAuthority(), TEXT("Only call this function as the server!"));

	APawn* pawn = GetPawn();
	if(pawn)
	{
		_DeathLocation = pawn->GetActorLocation();
		_MyCharacter = Cast<APlayerCharacter>(pawn);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Pawn is null!"), *GetName());
	}

	PlayerState->bIsSpectator = true;
	PlayerState->bOnlySpectator = true;
	bPlayerIsWaiting = true;

	UnPossess();
	APlayerCharacter* playerToSpectate = Cast<APlayerCharacter>(ActorToSpectate);
	StartSpectating_Client(playerToSpectate);
}

void AMyPlayerController::StartSpectating_Client_Implementation(APlayerCharacter* PlayerToSpectate = nullptr)
{
	if (PlayerToSpectate)
	{		
		PlayerToSpectate->SetThirdPersonCameraToActive();
	}
	GetDefaultHUD()->SetState(EHUDState::Spectating);
}

/////////////////////////////////////////////////////
void AMyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	FInputActionBinding& bindungNextPlayer = InputComponent->BindAction("SpectateNextPlayer", IE_Pressed, this, &AMyPlayerController::SpectateNextPlayer_Server);
	bindungNextPlayer.bConsumeInput = false;

	FInputActionBinding& bindungPreviusPlayer = InputComponent->BindAction("SpectatePreviousPlayer", IE_Pressed, this, &AMyPlayerController::SpectatePreviousPlayer_Server);
	bindungPreviusPlayer.bConsumeInput = false;
}

/////////////////////////////////////////////////////
bool AMyPlayerController::IsSpectating()
{
	return PlayerState->bIsSpectator;
}

/////////////////////////////////////////////////////
const FVector& AMyPlayerController::GetDeathLocation() const
{
	return _DeathLocation;
}

/////////////////////////////////////////////////////
APlayerCharacter* AMyPlayerController::GetLastPossessedCharacter()
{
	return _MyCharacter;
}

/////////////////////////////////////////////////////
void AMyPlayerController::ClientTeamMessage_Implementation(class APlayerState* SenderPlayerState, const FString& S, FName Type, float MsgLifeTime /*= 0*/)
{
	Super::ClientTeamMessage_Implementation(SenderPlayerState, S, Type, MsgLifeTime);
	UGameplayStatics::GetGameInstance(GetWorld())->GetEngine()->AddOnScreenDebugMessage(0, 3.0f, FColor::Green, S);
}

/////////////////////////////////////////////////////
void AMyPlayerController::Possess(APawn* aPawn)
{
	Super::Possess(aPawn);
	ADefaultHUD* hud = GetDefaultHUD();
	if (hud)
	{
		hud->Init(Cast<APlayerCharacter>(aPawn));
	}
}

/////////////////////////////////////////////////////
ADefaultHUD* AMyPlayerController::GetDefaultHUD() const
{
	return Cast<ADefaultHUD>(GetHUD());
}

/////////////////////////////////////////////////////
void AMyPlayerController::SpectateNextPlayer_Server_Implementation()
{
	if (PlayerState->bIsSpectator)
	{
		ViewAPlayer(1);
	}
}

bool AMyPlayerController::SpectateNextPlayer_Server_Validate()
{
	return true;
}

void AMyPlayerController::SpectatePreviousPlayer_Server_Implementation()
{
	if (PlayerState->bIsSpectator)
	{
		ViewAPlayer(-1);
	}
}

bool AMyPlayerController::SpectatePreviousPlayer_Server_Validate()
{
	return true;
}