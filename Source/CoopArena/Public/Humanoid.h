// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/BAS_Interface.h"
#include "Humanoid.generated.h"


class AGun;
class IInteractable;
class UDamageType;
class AItemBase;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEquipWeapon_Signature);


UCLASS()
class COOPARENA_API AHumanoid : public ACharacter, public IBAS_Interface
{
	GENERATED_BODY()


public:
	virtual void Tick(float DeltaTime) override;

	AHumanoid();

protected:
	virtual void BeginPlay() override;

	/////////////////////////////////////////////////////
					/* Movement */
	/////////////////////////////////////////////////////
protected:
	/** Handles moving forward/backward */
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void MoveForward(float value);
	/** Handles strafing movement, left and right */
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void MoveRight(float value);

	/**
	* Called via input to turn at a given rate.
	* @param Rate This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void TurnAtRate(float value);
	/**
	* Called via input to turn look up/down at a given rate.
	* @param Rate This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void LookUpAtRate(float value);

	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void SetProne(bool bProne);

	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void SetSprinting(bool bSprint);

	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void SetCrouch(bool bSprint);

	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void ToggleJump();

	/** Base turn rate, in deg/sec */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Humanoid)
	float _BaseTurnRate;

	/** Base look up/down rate, in deg/sec */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Humanoid)
	float _BaseLookUpRate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Humanoid)
	bool bToggleProne;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Humanoid)
	bool bToggleSprinting;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Humanoid)
	bool bToggleCrouching;

	UPROPERTY(BlueprintReadOnly, Category = Humanoid)
	bool bIsSprinting;

	UPROPERTY(BlueprintReadOnly, Category = Humanoid)
	bool bIsProne;

	UPROPERTY(BlueprintReadOnly, Category = Humanoid)
	bool bIsMovingForward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Humanoid)
	float _MaxSprintSpeed;

	UPROPERTY(BlueprintReadOnly, Category = Humanoid)
	float _MaxWalkingSpeed;


	/////////////////////////////////////////////////////
			/* Basic Animation System Interface */
	/////////////////////////////////////////////////////
public:
	bool IsAiming_Implementation() override;
	EWEaponType GetEquippedWeaponType_Implementation() override;
	EMovementType GetMovementType_Implementation() override;
	EMovementAdditive GetMovementAdditive_Implementation() override;


	/////////////////////////////////////////////////////
						/* Interaction */
	/////////////////////////////////////////////////////
public:
	/* 
	 * Attaches an item to a hand.
	 * @param ItemToGrab Must not be null.
	 * @param bKeepRelativeOffset Weather or not to keep the relative offset between the ActorToGrab and this actor.
	 */
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void GrabItem(AItemBase* ItemToGrab, bool bKeepRelativeOffset, FTransform Offset);

	/* Calculates and safes the offset between the currently held item and this character. */
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	FTransform CalcAndSafeActorOffset(AActor* OtherActor);

	/* Drops an actor and activates physics on that actor. */
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	AItemBase* DropItem();	

	void SetUpDefaultEquipment();			

protected:
	UPROPERTY(BlueprintReadOnly, Category = Humanoid)
	AItemBase* _ItemInHand;

	UPROPERTY(BlueprintReadOnly, Category = Humanoid)
	FTransform _ItemOffset;


	/////////////////////////////////////////////////////
						/* Weapon */
	/////////////////////////////////////////////////////
public:
	UFUNCTION(BlueprintPure, Category = Humanoid)
	FName GetWeaponAttachPoint() const;

	UFUNCTION(BlueprintPure, Category = Humanoid)
	AGun* GetEquippedGun() const;

	UFUNCTION(BlueprintPure, Category = Humanoid)
	bool CanFire() const;

	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void SetEquippedWeapon(AGun* weapon);

	/* 
	 * Use this function, when a component has to prevent this actor from firing his weapon.
	 * If the gun is blocked, then only the component that blocked it can unblock it.
	 * @param bisBlocking True if the actor shouldn't be allowed to fire his weapon.
	 * @param Component The component who wants to block this actor from firing. 
	 * Must not be null or this function will not do anything.
	 * @return True if the blocking status was successfully changed. E.g. if the given component
	 * was allowed to change the value.
	 */
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	bool SetComponentIsBlockingFiring(bool bIsBlocking, UActorComponent* Component);

	/* Called when the character wants to equip a weapon. */
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = Humanoid)
	FEquipWeapon_Signature EquipWeapon_Event;

protected:
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	virtual void OnEquipWeapon();	

	/* Fire the currently equipped weapon */
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	virtual void FireEquippedWeapon();

	/* Stops firing the currently equipped weapon */
	UFUNCTION(BlueprintCallable, Category = Humanoid)
	virtual void StopFireEquippedWeapon();		

	UFUNCTION(BlueprintCallable, Category = Humanoid)
	virtual void ToggleAiming();	

	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void ReloadWeapon();

	UFUNCTION(BlueprintCallable, Category = Humanoid)
	void ChangeWeaponFireMode();

	/** The tool or weapon that the character will start the game with */
	UPROPERTY(EditDefaultsOnly, Category = Humanoid)
	TSubclassOf<AGun> _DefaultGun;

	/** Socket or bone name for attaching weapons when equipped */
	UPROPERTY(EditDefaultsOnly, Category = Humanoid)
	FName _WeaponAttachPoint;	

	/* When the Kill() function is called and the damage type does not have any specific impulse, this value will be used instead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Humanoid)
	float _DefaultInpulsOnDeath;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Humanoid)
	UArrowComponent* _DroppedItemSpawnPoint;

	/* The characters currently held weapon */
	UPROPERTY(BlueprintReadWrite, Category = Humanoid)
	AGun* _EquippedWeapon;

	UPROPERTY(BlueprintReadWrite, Category = Humanoid)
	bool bIsAiming;

private:
	/* True if a component attached to this actor wants to prevent it from firing it's weapon. */
	UPROPERTY()
	bool bComponentBlocksFiring;

	/* The component that wants to prevent this actor from firing. */
	UPROPERTY()
	UActorComponent* _BlockingComponent;
};