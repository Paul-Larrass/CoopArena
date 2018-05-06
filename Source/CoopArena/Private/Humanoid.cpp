// Fill out your copyright notice in the Description page of Project Settings.

#include "Humanoid.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "Weapons/Weapon.h"


// Sets default values
AHumanoid::AHumanoid()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	bAlreadyDied = false;
	WeaponAttachPoint = "GripPoint";

}

// Called when the game starts or when spawned
void AHumanoid::BeginPlay()
{
	Super::BeginPlay();
	
}


/////////////////////////////////////////////////////
void AHumanoid::FireEquippedWeapon()
{
	if (CanFire())
	{

	}
}


void AHumanoid::StopFireEquippedWeapon()
{

}


/////////////////////////////////////////////////////
void AHumanoid::OnDeath(const FHitResult& hitInfo, FVector direction)
{

}


/////////////////////////////////////////////////////
void AHumanoid::DeactivateCollisionCapsuleComponent()
{
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->bGenerateOverlapEvents = false;
}


/////////////////////////////////////////////////////
void AHumanoid::MoveForward(float value)
{
	if (value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), value);
	}
}


void AHumanoid::MoveRight(float value)
{
	if (value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), value);
	}
}


void AHumanoid::TurnAtRate(float rate)
{
	AddControllerYawInput(rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}


void AHumanoid::LookUpAtRate(float rate)
{
	AddControllerPitchInput(rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}


void AHumanoid::ToggleCrouch()
{
	if (this->bIsCrouched)
	{
		this->UnCrouch();
	}
	else
	{
		this->Crouch();
	}
}


bool AHumanoid::IsAlive() const
{
	return true;
}

/////////////////////////////////////////////////////
bool AHumanoid::CanFire() const
{
	return EquippedWeapon && IsAlive();
}


FName AHumanoid::GetWeaponAttachPoint() const
{
	return WeaponAttachPoint;
}


void AHumanoid::SetUpDefaultEquipment()
{
	if (DefaultWeapon == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("DefaultWeapon is null!"));
		return;
	}
	if (!GetWeaponAttachPoint().IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("WeaponAttachPoint is not valid!"));
		return;
	}

	EquippedWeapon = GetWorld()->SpawnActor<AWeapon>(DefaultWeapon);
	//EquippedWeapon->EquipWeapon(this);
}


void AHumanoid::Kill()
{
}


// Called every frame
void AHumanoid::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


// Called to bind functionality to input
void AHumanoid::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &AHumanoid::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AHumanoid::MoveRight);

	// Bind mouse movement.
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AHumanoid::FireEquippedWeapon);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AHumanoid::StopFireEquippedWeapon);

	// Bind crouch event
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &AHumanoid::ToggleCrouch);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &AHumanoid::ToggleCrouch);
}