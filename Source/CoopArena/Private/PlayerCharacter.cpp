// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerCharacter.h"
#include "Engine/World.h"
#include "Interactable.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InputComponent.h"
#include "Components/BasicAnimationSystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Enums/WeaponEnums.h"
#include "Gun.h"
#include "CoopArena.h"
#include "CoopArenaGameMode.h"
#include "HealthComponent.h"
#include "SimpleInventory.h"
#include "AudioThread.h"
#include "SoundNodeLocalPlayer.h"
#include "Projectile.h"
#include "Kismet/GameplayStatics.h"
#include "MyPlayerController.h"
#include "DefaultHUD.h"


/////////////////////////////////////////////////////
APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	_BaseTurnRate = 0.5f;
	_BaseLookUpRate = 0.5f;

	_bToggleProne = true;
	_bToggleCrouching = true;
	_bToggleWalking = true;

	_InteractionRange = 200.0f;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("First person camera"));
	FirstPersonCamera->SetupAttachment(GetMesh(), "head");
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetRelativeLocationAndRotation(FVector(7.0f, 5.0f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Spring arm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetRelativeLocation(FVector(0.0f, 20.0f, 60.0f));
	SpringArm->TargetArmLength = 200.0f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->bEnableCameraRotationLag = true;

	ProjectileInteraction = CreateDefaultSubobject<USphereComponent>(TEXT("Projectile interaction"));
	ProjectileInteraction->SetupAttachment(RootComponent);
	ProjectileInteraction->SetSphereRadius(150.0f);
	ProjectileInteraction->SetGenerateOverlapEvents(true);
	ProjectileInteraction->SetCollisionResponseToAllChannels(ECR_Ignore);
	ProjectileInteraction->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	ProjectileInteraction->OnComponentBeginOverlap.AddDynamic(this, &APlayerCharacter::HandleProjectileOverlap);

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Third person camera"));
	ThirdPersonCamera->SetupAttachment(SpringArm, "SpringEndpoint");

	_TeamName = "Player Team";

	_StopSoundDelay = 3.0f;
}

/////////////////////////////////////////////////////
void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsLocallyControlled() && InputEnabled())
	{
		CheckForInteractables();
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::TurnAtRate(float Rate)
{
	float sprintModifier = _Gait == EGait::Sprinting ? 0.5f : 1.0f;
	AddControllerYawInput(Rate * _BaseTurnRate * sprintModifier);
}

void APlayerCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * _BaseTurnRate);
}

/////////////////////////////////////////////////////
void APlayerCharacter::SetSprinting(bool bWantsToSprint)
{
	Super::SetSprinting(bWantsToSprint);
	UWorld* world = GetWorld();
	if (bWantsToSprint)
	{
		if(world)
		{
			_TimeOfSprintStart = world->GetTimeSeconds();
			_PlayingSprintingSound = UGameplayStatics::SpawnSoundAttached(_SprintingSound, GetRootComponent());
		}
	}
	else
	{
		if (_PlayingSprintingSound)
		{
			_PlayingSprintingSound->Stop();
		}

		if(world)
		{
			float currentTime = world->GetTimeSeconds();
			if ((currentTime - _TimeOfSprintStart) >= _StopSoundDelay)
			{
				UGameplayStatics::SpawnSoundAttached(_StopSprintingSound, GetRootComponent());
			}
		}
	}	
}

/////////////////////////////////////////////////////
void APlayerCharacter::StopRunningSound()
{
	if (_PlayingSprintingSound)
	{
		_PlayingSprintingSound->Stop();
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::CheckForInteractables()
{
	if (IsPlayerControlled() && InteractionLineTrace(_InteractionHitResult))
	{
		AActor* hitActor = _InteractionHitResult.GetActor();
		UPrimitiveComponent* hitComponent = _InteractionHitResult.GetComponent();

		// Only do Begin/End line traces calls if we are pointing at something new
		if (hitActor != _ActorInFocus)
		{
			if (_ActorInFocus)
			{
				IInteractable::Execute_OnEndLineTraceOver(_ActorInFocus, this);
			}

			IInteractable::Execute_OnBeginLineTraceOver(hitActor, this, hitComponent);				
			SetActorInFocus(hitActor);
		}
		SetComponentInFocus(hitComponent);
	}
	else if (_ActorInFocus)
	{
		IInteractable::Execute_OnEndLineTraceOver(_ActorInFocus, this);
		SetActorInFocus(nullptr);
		SetComponentInFocus(nullptr);
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		ACoopArenaGameMode* gameMode = GetWorld()->GetAuthGameMode<ACoopArenaGameMode>();
		gameMode->RegisterPlayerCharacter(this);
	}
	HealthComponent->OnDeath.AddDynamic(this, &APlayerCharacter::HandleOnDeath);
}

/////////////////////////////////////////////////////
void APlayerCharacter::HandleOnDestroyed(AActor* DestroyedActor)
{
	Super::HandleOnDestroyed(DestroyedActor);

	ACoopArenaGameMode* gameMode = GetWorld()->GetAuthGameMode<ACoopArenaGameMode>();
	if (gameMode)
	{
		gameMode->UnregisterPlayerCharacter(this);
	}
}

void APlayerCharacter::HandleOnDeath(AActor* DeadActor, AController* ActorController, AController* Killer)
{
	if(IsAiming())
	{
		OnAimingReleased();
	}
	//StopRunningSound();
}

/////////////////////////////////////////////////////
void APlayerCharacter::HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(IsLocallyControlled() && OtherActor->GetOwner() != this)
	{
		AProjectile* projectile = Cast<AProjectile>(OtherActor);
		ensure(projectile);

		projectile->PlayFlyBySound();
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::SetActorInFocus(AActor* actor)
{
	_ActorInFocus = actor;
}

void APlayerCharacter::SetComponentInFocus(UPrimitiveComponent* Component)
{
	_ComponentInFocus = Component;
}

/////////////////////////////////////////////////////
bool APlayerCharacter::InteractionLineTrace(FHitResult& outHitresult)
{
	FVector cameraLocation = GetActiveCamera()->GetComponentLocation();
	FVector forwardVector = GetActiveCamera()->GetForwardVector();
	FVector traceEndLoaction = cameraLocation + forwardVector * _InteractionRange;
	FCollisionQueryParams params;
	params.AddIgnoredActor(this);
	if (_EquippedWeapon)
	{
		params.AddIgnoredActor((AActor*)_EquippedWeapon);
	}
	return GetWorld()->LineTraceSingleByChannel(outHitresult, cameraLocation, traceEndLoaction, ECC_Interactable, params);
}

/////////////////////////////////////////////////////
void APlayerCharacter::OnPronePressed()
{
	if (_bToggleProne)
	{
		_bIsProne = !_bIsProne;
	}
	else
	{
		_bIsProne = true;
	}
}

void APlayerCharacter::OnProneReleased()
{
	if (!_bToggleProne)
	{
		_bIsProne = false;
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::OnWalkPressed()
{
	if (_bToggleWalking)
	{
		if (_Gait == EGait::Walking)
		{
			SetWalking(false);
		}
		else
		{
			SetWalking(true);
		}
	}
	else
	{
		SetWalking(true);
	}
}

void APlayerCharacter::OnWalkReleased()
{
	if (!_bToggleWalking)
	{
		SetWalking(false);
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::OnSprintPressed()
{
	if (_bToggleSprinting)
	{
		const bool bIsSprinting = _Gait == EGait::Sprinting;
		SetSprinting(!bIsSprinting);
	}
	else if (_Gait != EGait::Sprinting)
	{
		SetSprinting(true);
	}
}

void APlayerCharacter::OnSprintReleased()
{
	if (!_bToggleSprinting)
	{
		SetSprinting(false);
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::OnCrouchPressed()
{
	if (_bToggleCrouching)
	{
		SetCrouch(!bIsCrouched);
	}
	else
	{
		BASComponent->GetActorVariables().MovementAdditive = EMovementAdditive::Crouch;
		Crouch();
	}
}

void APlayerCharacter::OnCrouchReleased()
{
	if (!_bToggleCrouching)
	{
		BASComponent->GetActorVariables().MovementAdditive = EMovementAdditive::None;
		UnCrouch();
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::OnChangeCameraPressed()
{
	FirstPersonCamera->ToggleActive();
	ThirdPersonCamera->ToggleActive();

	if (ThirdPersonCamera->IsActive())
	{
		_InteractionRange += SpringArm->TargetArmLength;
	}
	else
	{
		_InteractionRange -= SpringArm->TargetArmLength;
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::OnAimingPressed()
{
	if (_EquippedWeapon == nullptr || _Gait == EGait::Sprinting)
	{
		return;
	}

	EWeaponState weaponState = _EquippedWeapon->GetWeaponState();
	if (weaponState == EWeaponState::Equipping || weaponState == EWeaponState::Reloading)
	{
		return;
	}

	_bIsAiming = true;
	BASComponent->GetActorVariables().bIsAiming = true;

	APlayerController* pc = Cast<APlayerController>(GetController());
	if (pc)
	{
		pc->SetViewTargetWithBlend(_EquippedWeapon, 0.2f);
	}
}

void APlayerCharacter::OnAimingReleased()
{
	if (_EquippedWeapon && _Gait != EGait::Sprinting)
	{
		_bIsAiming = false;
		BASComponent->GetActorVariables().bIsAiming = false;

		APlayerController* pc = Cast<APlayerController>(GetController());
		if (pc)
		{
			pc->SetViewTargetWithBlend(pc->GetPawn(), 0.2f);
		}
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::OnBeginInteracting()
{
	if (_ActorInFocus)
	{
		Server_OnBeginInteracting(_ActorInFocus, _ComponentInFocus);
	}
}

void APlayerCharacter::OnEndInteracting()
{
	if (_ActorInFocus)
	{
		IInteractable::Execute_OnEndInteract(_ActorInFocus, this);
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	check(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &APlayerCharacter::ToggleJump);

	PlayerInputComponent->BindAxis("MoveForward", this, &APlayerCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APlayerCharacter::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &APlayerCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APlayerCharacter::LookUpAtRate);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &APlayerCharacter::FireEquippedWeapon);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &APlayerCharacter::StopFireEquippedWeapon);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &APlayerCharacter::OnCrouchPressed);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &APlayerCharacter::OnCrouchReleased);

	PlayerInputComponent->BindAction("Prone", IE_Pressed, this, &APlayerCharacter::OnPronePressed);
	PlayerInputComponent->BindAction("Prone", IE_Released, this, &APlayerCharacter::OnProneReleased);

	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &APlayerCharacter::OnSprintPressed);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &APlayerCharacter::OnSprintReleased);

	PlayerInputComponent->BindAction("Walk", IE_Pressed, this, &APlayerCharacter::OnWalkPressed);
	PlayerInputComponent->BindAction("Walk", IE_Released, this, &APlayerCharacter::OnWalkReleased);

	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &APlayerCharacter::OnBeginInteracting);
	PlayerInputComponent->BindAction("Interact", IE_Released, this, &APlayerCharacter::OnEndInteracting);

	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &APlayerCharacter::OnAimingPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &APlayerCharacter::OnAimingReleased);
	
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &APlayerCharacter::ReloadWeapon);

	PlayerInputComponent->BindAction("ChangeFireMode", IE_Pressed, this, &APlayerCharacter::ChangeWeaponFireMode);

	PlayerInputComponent->BindAction("ChangeWeapon", IE_Pressed, this, &APlayerCharacter::OnWeaponChange);	

	if (WITH_EDITOR)
	{
		PlayerInputComponent->BindAction("ChangeCamera", IE_Pressed, this, &APlayerCharacter::OnChangeCameraPressed);
	}
}

/////////////////////////////////////////////////////
void APlayerCharacter::SetThirdPersonCameraToActive()
{
	FirstPersonCamera->SetActive(false);
	ThirdPersonCamera->SetActive(true);
}

/////////////////////////////////////////////////////
const FHitResult& APlayerCharacter::GetInteractionLineTraceHitResult() const
{
	return _InteractionHitResult;
}

/////////////////////////////////////////////////////
void APlayerCharacter::Server_OnBeginInteracting_Implementation(AActor* ActorInFocus, UPrimitiveComponent* ComponentInFocus)
{
	IInteractable::Execute_OnBeginInteract(ActorInFocus, this, ComponentInFocus);
}

bool APlayerCharacter::Server_OnBeginInteracting_Validate(AActor* ActorInFocus, UPrimitiveComponent* ComponentInFocus)
{
	return true;
}

/////////////////////////////////////////////////////
void APlayerCharacter::Server_OnEndInteracting_Implementation(AActor* ActorInFocus)
{
	IInteractable::Execute_OnEndInteract(ActorInFocus, this);
}

bool APlayerCharacter::Server_OnEndInteracting_Validate(AActor* ActorInFocus)
{
	return true;
}