// Fill out your copyright notice in the Description page of Project Settings.

#include "Humanoid.h"
#include "Engine/World.h"
#include "Interactable.h"
#include "TimerManager.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/HealthComponent.h"
#include "Components/BasicAnimationSystemComponent.h"
#include "Components/SimpleInventory.h"
#include "Components/RespawnComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapons/Gun.h"
#include "GameFramework/Controller.h"
#include "UnrealNetwork.h"
#include "GameModes/CoopArenaGameMode.h"
#include "AudioThread.h"
#include "Sound/SoundNodeLocalPlayer.h"
#include "Animation/AnimInstance.h"


AHumanoid::AHumanoid()
{
	PrimaryActorTick.bCanEverTick = true;

	_EquippedWeaponAttachPoint = "GripPoint";

	m_DroppedItemSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("Dropped item spawn point"));
	m_DroppedItemSpawnPoint->SetupAttachment(RootComponent);

	_bIsProne = false;
	bIsCrouched = false;
	_bIsAiming = false;

	_Gait = EGait::Jogging;

	_SprintingSpeed = 600.0f;
	_JoggingSpeed = 450.0f;
	_WalkingSpeed = 150.0f;
	_MaxCrouchingSpeed = 200.0f;

	SetReplicates(true);
	SetReplicateMovement(true);

	USkeletalMeshComponent* mesh = GetMesh();
	mesh->SetGenerateOverlapEvents(true);
	mesh->SetCollisionObjectType(ECC_PhysicsBody);
	mesh->SetCustomDepthStencilValue(253);

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Inventory = CreateDefaultSubobject<USimpleInventory>(TEXT("Inventory"));
	BASComponent = CreateDefaultSubobject<UBasicAnimationSystemComponent>(TEXT("Basic Animation System"));
	RespawnComponent = CreateDefaultSubobject<URespawnComponent>(TEXT("Respawn"));

	AddOwnedComponent(Inventory);
	AddOwnedComponent(BASComponent);
	AddOwnedComponent(HealthComponent);
	AddOwnedComponent(RespawnComponent);

	UCharacterMovementComponent* moveComp = GetCharacterMovement();
	moveComp->JumpZVelocity = 300.0f;
	moveComp->AirControl = 0.1f;
	moveComp->MaxAcceleration = 1000.0f;
	moveComp->BrakingDecelerationWalking = 1000.0f;
	moveComp->GroundFriction = 8.0f;
	moveComp->BrakingFriction = 8.0f;
	moveComp->bUseSeparateBrakingFriction = true;
	moveComp->CrouchedHalfHeight = 65.0f;
	moveComp->MaxWalkSpeed = _JoggingSpeed;
	moveComp->MaxWalkSpeedCrouched = _MaxCrouchingSpeed;
	moveComp->bCanWalkOffLedgesWhenCrouching = true;
	moveComp->MaxCustomMovementSpeed = _SprintingSpeed;
	moveComp->MovementState.bCanCrouch = true;

	_bCanBeInteractedWith = false;
}

/////////////////////////////////////////////////////
void AHumanoid::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	ACoopArenaGameMode* gameMode = Cast<ACoopArenaGameMode>(GetWorld()->GetAuthGameMode());
	if (gameMode)
	{
		_TeamName = (IsPlayerControlled() ? gameMode->GetPlayerTeamName() : gameMode->GetBotTeamName()).ToString();
	}
}

/////////////////////////////////////////////////////
bool AHumanoid::IsAlive() const
{
	return HealthComponent->IsAlive();
}

/////////////////////////////////////////////////////
void AHumanoid::Revive(bool bSpawnDefaultEquipment /*= false*/)
{
	RespawnComponent->Respawn();
}

/////////////////////////////////////////////////////
AGun* AHumanoid::GetEquippedGun() const
{
	return _EquippedWeapon;
}

/////////////////////////////////////////////////////
bool AHumanoid::SetComponentIsBlockingFiring(bool bIsBlocking, UActorComponent* Component)
{
	if (Component == nullptr || (m_BlockingComponent && m_BlockingComponent != Component))
	{
		return false;
	}

	m_bComponentBlocksFiring = bIsBlocking;

	if (bIsBlocking)
	{
		m_BlockingComponent = Component;
	}
	else
	{
		m_BlockingComponent = nullptr;
	}
	
	return true;
}

/////////////////////////////////////////////////////
void AHumanoid::OnWeaponChange()
{
	if (Inventory->GetGunAtAttachPoint())
	{
		Inventory->ChangeWeapon();
	}
}

/////////////////////////////////////////////////////
void AHumanoid::BeginPlay()
{
	Super::BeginPlay();

	BASComponent->SetSprintingSpeedThreshold(_JoggingSpeed + 10.0f);
	GetCharacterMovement()->MaxWalkSpeed = _JoggingSpeed;

	if (HasAuthority())
	{
		SetUpDefaultEquipment();
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bLocallyControlled = (PC ? PC->IsLocalController() : false);
	const uint32 UniqueID = GetUniqueID();
	FAudioThread::RunCommandOnAudioThread([UniqueID, bLocallyControlled]()
	{
		USoundNodeLocalPlayer::GetLocallyControlledActorCache().Add(UniqueID, bLocallyControlled);
	});

	OnDestroyed.AddDynamic(this, &AHumanoid::HandleOnDestroyed);
}

/////////////////////////////////////////////////////
void AHumanoid::HandleOnDestroyed(AActor* DestroyedActor)
{
	TInlineComponentArray<UPrimitiveComponent*> Components(DestroyedActor);
	GetComponents(Components);

	FTimerManager& timerManager = GetWorld()->GetTimerManager();
	for (UPrimitiveComponent* component : Components)
	{
		timerManager.ClearAllTimersForObject(component);
	}

	timerManager.ClearAllTimersForObject(this);

	if (_EquippedWeapon)
	{
		timerManager.ClearAllTimersForObject(_EquippedWeapon);
	}
}

/////////////////////////////////////////////////////
void AHumanoid::OnBeginInteract_Implementation(APawn* InteractingPawn, UPrimitiveComponent* HitComponent)
{
	OnBeginInteract_Event.Broadcast(InteractingPawn, HitComponent);
}

UUserWidget* AHumanoid::OnBeginLineTraceOver_Implementation(APawn* Pawn, UPrimitiveComponent* HitComponent)
{
	GetMesh()->SetRenderCustomDepth(true);
	OnBeginLineTraceOver_Event.Broadcast(Pawn, HitComponent);
	return _LineTraceOverUserWidget;
}

void AHumanoid::OnEndLineTraceOver_Implementation(APawn* Pawn)
{
	GetMesh()->SetRenderCustomDepth(false);
	OnEndLineTraceOver_Event.Broadcast(Pawn);
}

void AHumanoid::SetCanBeInteractedWith_Implementation(bool bCanbeInteractedWith)
{
	_bCanBeInteractedWith = bCanbeInteractedWith;
}

/////////////////////////////////////////////////////
void AHumanoid::SetSprinting(bool bWantsToSprint)
{
	bool bCanSprint = BASComponent->GetActorVariables().bIsMovingForward;
	if (bWantsToSprint && bCanSprint)
	{
		if (_bIsAiming)
		{
			ToggleAiming();
		}
		GetCharacterMovement()->MaxWalkSpeed = _SprintingSpeed;
		_Gait = EGait::Sprinting;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = _JoggingSpeed;
		_Gait = EGait::Jogging;
	}

	if (!HasAuthority())
	{
		SetSprinting_Server(bWantsToSprint);
	}
}

void AHumanoid::SetSprinting_Server_Implementation(bool bWantsToSprint)
{
	SetSprinting(bWantsToSprint);
}

bool AHumanoid::SetSprinting_Server_Validate(bool bWantsToSprint)
{
	return true;
}

/////////////////////////////////////////////////////
void AHumanoid::SetWalking(bool bWantsToWalk)
{
	if (bWantsToWalk)
	{
		_Gait = EGait::Walking;
		GetCharacterMovement()->MaxWalkSpeed = _WalkingSpeed;
	}
	else
	{
		_Gait = EGait::Jogging;
		GetCharacterMovement()->MaxWalkSpeed = _JoggingSpeed;
	}
}

/////////////////////////////////////////////////////
void AHumanoid::FireEquippedWeapon()
{
	if (CanFire() && _EquippedWeapon)
	{
		_EquippedWeapon->FireGun();
	}
}

void AHumanoid::StopFireEquippedWeapon()
{
	if (_EquippedWeapon)
	{
		_EquippedWeapon->OnStopFire();
	}
}

/////////////////////////////////////////////////////
void AHumanoid::SetVelocity(float NewVelocity)
{
	GetCharacterMovement()->MaxWalkSpeed = FMath::Clamp(NewVelocity, -_JoggingSpeed, _SprintingSpeed);
	GetCharacterMovement()->MaxWalkSpeedCrouched = FMath::Clamp(NewVelocity, -_MaxCrouchingSpeed, _MaxCrouchingSpeed);
	
	if (!HasAuthority())
	{
		SetVelocity_Server(NewVelocity);
	}
}

void AHumanoid::SetVelocity_Server_Implementation(float NewVelocity)
{
	SetVelocity(NewVelocity);
}

bool AHumanoid::SetVelocity_Server_Validate(float NewVelocity)
{
	return true;
}

/////////////////////////////////////////////////////
void AHumanoid::EquipWeapon(AGun* WeaponToEquip, bool bRequestNetMulticast /*= true*/)
{
	verifyf(WeaponToEquip, TEXT("Do not call this function if 'WeaponToEquip' is nullptr!"));
	if (bRequestNetMulticast)
	{
		if (HasAuthority() == false)
		{
			SetWeaponToEquip_Server(WeaponToEquip);
		}
		else
		{
			_WeaponToEquip = WeaponToEquip;
			HandleWeaponEquip();
		}
	}
	else
	{
		_EquippedWeapon = WeaponToEquip;
		_EquippedWeapon->OnEquip(this);
		BASComponent->GetActorVariables().EquippedWeaponType = _EquippedWeapon->GetWeaponType();
		OnWeaponEquipped.Broadcast(this, WeaponToEquip);
	}
}

void AHumanoid::UnequipWeapon(bool bDropGun, bool bRequestNetMulticast /*= true*/)
{
	if (bRequestNetMulticast)
	{
		if (HasAuthority() == false)
		{
			UnequipWeapon_Server(bDropGun);
		}
		else
		{
			HandleWeaponUnEquip_Multicast(bDropGun);
		}
	}
	else
	{
		HandleWeaponUnEquip(bDropGun);
	}	
}

/////////////////////////////////////////////////////
void AHumanoid::SetEquippedWeaponFireMode(EFireMode NewFireMode)
{
	if (_EquippedWeapon)
	{
		_EquippedWeapon->SetFireMode(NewFireMode);
	}
}

/////////////////////////////////////////////////////
void AHumanoid::MoveForward(float Value)
{
	if (Value == 0.0f || Controller == nullptr)
	{
		return;
	}

	if (Value < 0.0f && _Gait == EGait::Sprinting)
	{
		SetSprinting(false);
	}
	else
	{
		FRotator Rotation = Controller->GetControlRotation();
		Rotation.Pitch = 0.0f;
		const FVector Direction = Rotation.Vector();
		AddMovementInput(Direction, Value);
	}
}

void AHumanoid::MoveRight(float Value)
{
	if (Controller && Value != 0.0f && _Gait != EGait::Sprinting)
	{
		FRotator Rotation = Controller->GetControlRotation();
		Rotation.Pitch = 0.0f;
		const FVector Direction = Rotation.Vector().RotateAngleAxis(90.0f, FVector(0.0f, 0.0f, 1.0f));
		AddMovementInput(Direction, Value);
	}
}

/////////////////////////////////////////////////////
void AHumanoid::SetProne(bool bProne)
{
	_bIsProne = bProne;
}

/////////////////////////////////////////////////////
void AHumanoid::ToggleAiming()
{
	if (_Gait != EGait::Sprinting)
	{
		_bIsAiming = !_bIsAiming;
		BASComponent->GetActorVariables().bIsAiming = _bIsAiming;
	}
	else
	{
		_bIsAiming = false;
		BASComponent->GetActorVariables().bIsAiming = false;
	}
}

/////////////////////////////////////////////////////
void AHumanoid::SetCrouch(bool bCrouch)
{
	if (bCrouch)
	{
		BASComponent->GetActorVariables().MovementAdditive = EMovementAdditive::Crouch;
		Crouch();
	}
	else
	{
		BASComponent->GetActorVariables().MovementAdditive = EMovementAdditive::None;
		UnCrouch();
	}
}

/////////////////////////////////////////////////////
void AHumanoid::ToggleJump()
{
	if (GetCharacterMovement()->IsMovingOnGround())
	{
		BASComponent->OnJumpEvent.Broadcast();
	}
	else
	{
		StopJumping();
	}
}

/////////////////////////////////////////////////////
void AHumanoid::ReloadWeapon()
{
	UAnimInstance* animInstance = GetMesh()->GetAnimInstance();
	const bool bIsReloading = animInstance ? animInstance->GetCurveValue("IsReloading") : false;
	
	if (_EquippedWeapon && !bIsReloading)
	{
		_EquippedWeapon->Reload();
	}		
}

/////////////////////////////////////////////////////
void AHumanoid::ChangeWeaponFireMode()
{
	if (_EquippedWeapon)
	{
		EFireMode newMode = _EquippedWeapon->ToggleFireMode();
		OnFireModeChanged.Broadcast(this, newMode);
	}
}

/////////////////////////////////////////////////////
void AHumanoid::GrabItem(AItemBase* ItemToGrab, bool bKeepRelativeOffset)
{
	if (ItemToGrab == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Called ItemToGrab but the item was nullptr."), *GetName());
		return;
	}
	m_ItemInHand = ItemToGrab;
	CalcAndSafeActorOffset(ItemToGrab);
	FName handSocket = "HandLeft";
	ItemToGrab->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, handSocket);
	
	if (bKeepRelativeOffset)
	{
		ItemToGrab->AddActorLocalTransform(m_ItemOffset);		
	}
}

/////////////////////////////////////////////////////
FTransform AHumanoid::CalcAndSafeActorOffset(AActor* OtherActor)
{
	FTransform offset;
	FTransform handTransform = GetMesh()->GetSocketTransform("HandLeft");

	FVector itemLocation = OtherActor->GetActorLocation();
	offset.SetLocation(handTransform.InverseTransformPosition(itemLocation));

	FRotator itemRotation = OtherActor->GetActorRotation();
	offset.SetRotation(handTransform.InverseTransformRotation(itemRotation.Quaternion()));
	
	m_ItemOffset = offset;
	return offset;
}

/////////////////////////////////////////////////////
AItemBase* AHumanoid::DropItem()
{
	verifyf(m_ItemInHand, TEXT("%s tried to drop an item without having any to drop!"), *GetName());

	AItemBase* itemToDrop = m_ItemInHand;	
	itemToDrop->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	itemToDrop->ShouldSimulatePhysics(true);
	
	m_ItemInHand = nullptr;
	return itemToDrop;
}

/////////////////////////////////////////////////////
bool AHumanoid::CanFire() const
{
	return _Gait != EGait::Sprinting && GetCharacterMovement()->IsMovingOnGround() && !m_bComponentBlocksFiring;
}

/////////////////////////////////////////////////////
FName AHumanoid::GetEquippedWeaponAttachPoint() const
{
	return _EquippedWeaponAttachPoint;
}

/////////////////////////////////////////////////////
void AHumanoid::SetUpDefaultEquipment()
{
	verifyf(HasAuthority(), TEXT("Only the server is allowed to setup the default equippment."));

	if (m_DefaultGun == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("DefaultWeapon is null."));
		return;
	}
	if (GetEquippedWeaponAttachPoint().IsValid() == false)
	{
		UE_LOG(LogTemp, Error, TEXT("WeaponAttachPoint is not valid!"));
		return;
	}

	AGun* newWeapon = SpawnWeapon(m_DefaultGun);
	EquipWeapon(newWeapon);
}

/////////////////////////////////////////////////////
AGun* AHumanoid::SpawnWeapon(TSubclassOf<AGun> Class)
{
	FTransform spawnTransform = FTransform();
	GetWeaponSpawnTransform(spawnTransform);

	FActorSpawnParameters params;
	params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	params.Owner = this;
	params.Instigator = this;

	return GetWorld()->SpawnActor<AGun>(Class, spawnTransform, params);
}

/////////////////////////////////////////////////////
bool AHumanoid::IsAiming() const
{
	return _bIsAiming;
}

/////////////////////////////////////////////////////
int32 AHumanoid::GetNumRoundsLeft()
{
	if (_EquippedWeapon == nullptr)
	{
		return 0;
	}

	AMagazine* magazine = _EquippedWeapon->GetMagazine();
	return magazine ? magazine->RoundsLeft() : 0;	
}

/////////////////////////////////////////////////////
void AHumanoid::AttachWeaponToHolster(AGun* GunToAttach)
{
	if (GunToAttach)
	{
		Inventory->AttachGun(GunToAttach);
	}
}

/////////////////////////////////////////////////////
void AHumanoid::SetHolsterWeapon(AGun* Weapon)
{
	Inventory->SetHolsterWeapon_Multicast(Weapon);
}

/////////////////////////////////////////////////////
void AHumanoid::GetWeaponSpawnTransform(FTransform& OutTransform)
{
	FVector location;
	FRotator rotation;
	FName AttachPoint = GetEquippedWeaponAttachPoint();
	GetMesh()->GetSocketWorldLocationAndRotation(AttachPoint, location, rotation);

	OutTransform.SetLocation(location);
	OutTransform.SetRotation(rotation.Quaternion());
}

/////////////////////////////////////////////////////
FTransform AHumanoid::GetItemOffset(bool bInLocalSpace /*= true*/)
{
	FTransform offset = FTransform(m_ItemOffset);
	if (!bInLocalSpace)
	{
		const FTransform handTransform = GetMesh()->GetSocketTransform("HandLeft");
		offset.SetLocation(handTransform.TransformPosition(offset.GetLocation()));
		offset.SetRotation(handTransform.TransformRotation(offset.GetRotation()));
	}
	return offset;
}

/////////////////////////////////////////////////////
void AHumanoid::Multicast_ClearItemInHand_Implementation()
{
	if (m_ItemInHand && !m_ItemInHand->IsAttachedTo(this))
	{
		m_ItemInHand = nullptr;
	}
}


/////////////////////////////////////////////////////
					/* Networking */
/////////////////////////////////////////////////////
void AHumanoid::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHumanoid, _Gait);
	DOREPLIFETIME(AHumanoid, _bIsAiming);
	DOREPLIFETIME(AHumanoid, _bIsProne);
	DOREPLIFETIME(AHumanoid, _WeaponToEquip);
}

/////////////////////////////////////////////////////
void AHumanoid::EquipWeapon_Server_Implementation(AGun* Gun)
{
	SetWeaponToEquip_Server(Gun);
}

bool AHumanoid::EquipWeapon_Server_Validate(AGun* Gun)
{
	return true;
}

/////////////////////////////////////////////////////
void AHumanoid::SetWeaponToEquip_Server_Implementation(AGun* Weapon)
{
	if (_WeaponToEquip == Weapon)
	{
		_WeaponToEquip = nullptr;
	}
	_WeaponToEquip = Weapon;
	HandleWeaponEquip();
}

bool AHumanoid::SetWeaponToEquip_Server_Validate(AGun* Weapon)
{
	return true;
}

/////////////////////////////////////////////////////
void AHumanoid::HandleWeaponEquip()
{
	if(_WeaponToEquip)
	{
		_EquippedWeapon = _WeaponToEquip;
		_EquippedWeapon->OnEquip(this);
		BASComponent->GetActorVariables().EquippedWeaponType = _EquippedWeapon->GetWeaponType();
		OnWeaponEquipped.Broadcast(this, _EquippedWeapon);
	}
}

void AHumanoid::HandleWeaponUnEquip(bool bDropGun)
{
	if (_EquippedWeapon)
	{
		_EquippedWeapon->Unequip(bDropGun);
		_EquippedWeapon = nullptr;
		BASComponent->GetActorVariables().EquippedWeaponType = EWEaponType::None;
	}
}

/////////////////////////////////////////////////////
void AHumanoid::HandleWeaponUnEquip_Multicast_Implementation(bool bDropGun)
{
	HandleWeaponUnEquip(bDropGun);
}

/////////////////////////////////////////////////////
void AHumanoid::UnequipWeapon_Server_Implementation(bool bDropGun)
{
	UnequipWeapon(bDropGun);
}

bool AHumanoid::UnequipWeapon_Server_Validate(bool bDropGun)
{
	return true;
}

/////////////////////////////////////////////////////
void AHumanoid::RepMaxWalkSpeed_Implementation(float NewMaxWalkSpeed)
{
	GetCharacterMovement()->MaxWalkSpeed = NewMaxWalkSpeed;
}

bool AHumanoid::RepMaxWalkSpeed_Validate(float NewMaxWalkSpeed)
{
	return true;
}
