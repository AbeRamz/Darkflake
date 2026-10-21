// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatAttacker.h"
#include "CombatDamageable.h"
#include "Animation/AnimInstance.h"
#include "CombatCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UCombatLifeBar;
class UWidgetComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogCombatCharacter, Log, All);

/**
 *  An enhanced Third Person Character with melee combat capabilities:
 *  - Combo attack string
 *  - Press and hold charged attack
 *  - Damage dealing and reaction
 *  - Death
 *  - Respawning
 */
UCLASS(abstract)
class ACombatCharacter : public ACharacter, public ICombatAttacker, public ICombatDamageable
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Life bar widget component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* LifeBar;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Combo Attack Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ComboAttackAction;

	/** Charged Attack Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ChargedAttackAction;

	/** Toggle Camera Side Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ToggleCameraAction;

	/** Dodge Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* DodgeAction;

	/** Max amount of HP the character will have on respawn */
	UPROPERTY(EditAnywhere, Category="Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MaxHP = 5.0f;

	/** Current amount of HP the character has */
	UPROPERTY(VisibleAnywhere, Category="Damage")
	float CurrentHP = 0.0f;

	/** Stamina */
	UPROPERTY(EditAnywhere, Category = "Stamina", meta = (ClampMin = 1))
	float MaxStamina = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	float CurrentStamina = 0.0f;

	/** Stamina que cuesta cada golpe (cada golpe del combo y el ataque cargado) */
	UPROPERTY(EditAnywhere, Category = "Stamina")
	float AttackStaminaCost = 15.0f;

	/** Stamina que cuesta rodar */
	UPROPERTY(EditAnywhere, Category = "Stamina")
	float DodgeStaminaCost = 25.0f;

	/** Stamina que se recupera por segundo */
	UPROPERTY(EditAnywhere, Category = "Stamina")
	float StaminaRegenRate = 20.0f;

	/** Segundos sin gastar estamina antes de empezar a recuperar */
	UPROPERTY(EditAnywhere, Category = "Stamina")
	float StaminaRegenDelay = 1.0f;

	float LastStaminaUseTime = -100.0f;

	/** Life bar widget fill color */
	UPROPERTY(EditAnywhere, Category="Damage")
	FLinearColor LifeBarColor;

	/** Name of the pelvis bone, for damage ragdoll physics */
	UPROPERTY(EditAnywhere, Category="Damage")
	FName PelvisBoneName;

	/** Pointer to the life bar widget */
	UPROPERTY(EditAnywhere, Category="Damage")
	TObjectPtr<UCombatLifeBar> LifeBarWidget;

	/** Max amount of time that may elapse for a non-combo attack input to not be considered stale */
	UPROPERTY(EditAnywhere, Category="Melee Attack", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float AttackInputCacheTimeTolerance = 1.0f;

	/** Time at which an attack button was last pressed */
	float CachedAttackInputTime = 0.0f;

	/** If true, the character is currently playing an attack animation */
	bool bIsAttacking = false;

	/** Distance ahead of the character that melee attack sphere collision traces will extend */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units="cm"))
	float MeleeTraceDistance = 75.0f;

	/** Radius of the sphere trace for melee attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float MeleeTraceRadius = 75.0f;

	/** Distance ahead of the character that enemies will be notified of incoming attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units="cm"))
	float DangerTraceDistance = 300.0f;

	/** Radius of the sphere trace to notify enemies of incoming attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float DangerTraceRadius = 100.0f;

	/** Amount of damage a melee attack will deal */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MeleeDamage = 1.0f;

	/** Amount of knockback impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeKnockbackImpulse = 250.0f;

	/** Amount of upwards impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeLaunchImpulse = 300.0f;

	/** AnimMontage that will play for combo attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	UAnimMontage* ComboAttackMontage;

	/** Names of the AnimMontage sections that correspond to each stage of the combo attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	TArray<FName> ComboSectionNames;

	/** Max amount of time that may elapse for a combo attack input to not be considered stale */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float ComboInputCacheTimeTolerance = 0.45f;

	/** Index of the current stage of the melee attack combo */
	int32 ComboCount = 0;

	/** AttackCharged*/

	/** AnimMontage that will play for charged attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	UAnimMontage* ChargedAttackMontage;

	/** Name of the AnimMontage section that corresponds to the charge loop */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	FName ChargeLoopSection;

	/** Name of the AnimMontage section that corresponds to the attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	FName ChargeAttackSection;

	/** Minimum time (seconds) the button must be held for a fully charged attack */
	UPROPERTY(EditAnywhere, Category = "Melee Attack|Charged", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float MinChargeTime = 1.0f;

	/** Play rate multiplier applied when released before fully charged (attack plays faster) */
	UPROPERTY(EditAnywhere, Category = "Melee Attack|Charged", meta = (ClampMin = 1.0, ClampMax = 5.0))
	float WeakAttackPlayRateMultiplier = 2.0f;

	/** Distance the character lunges forward when committing to the charged attack swing */
	UPROPERTY(EditAnywhere, Category = "Melee Attack|Charged")
	float ChargedAttackLungeDistance = 300.0f;

	/** Duration (in seconds) over which the lunge distance is covered */
	UPROPERTY(EditAnywhere, Category = "Melee Attack|Charged")
	float ChargedAttackLungeDuration = 0.3f;

	UFUNCTION(BlueprintCallable, Category = "Melee Attack")
	void SetChargedAttackMontage(UAnimMontage* NewMontage);

	bool bIsLunging = false;

	/** Time remaining (seconds) on the current lunge */
	float LungeTimeRemaining = 0.0f;

	/** Cached forward velocity being applied while lunging */
	FVector LungeVelocity = FVector::ZeroVector;

	/** Dodge */
	/** AnimMontage played for the dodge/roll */
	UPROPERTY(EditAnywhere, Category = "Dodge")
	UAnimMontage* DodgeMontage;

	/** True while the character is currently dodging */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	bool bIsDodging = false;

	/** True while the character is invincible (e.g. during dodge i-frames) */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	bool bIsInvincible = false;

	/** Distance the character travels during the dodge roll */
	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = 0, ClampMax = 2000, Units = "cm"))
	float DodgeDistance = 400.0f;

	/** How long the dodge roll takes to complete, matching the animation length */
	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = 0.1, ClampMax = 3, Units = "s"))
	float DodgeDuration = 0.6f;

	/** Cached world-space direction of the last movement input, used to orient the dodge */
	FVector LastMovementInputVector = FVector::ZeroVector;

	/** Time at which the current charge began */
	float ChargeStartTime = 0.0f;

	/** True if the current/last charged attack was released before fully charging (for damage scaling later) */
	bool bIsWeakChargedAttack = false;

	/** Flag that determines if the player is currently holding the charged attack input */
	bool bIsChargingAttack = false;
	
	/** If true, the charged attack hold check has been tested at least once */
	bool bHasLoopedChargedAttack = false;

	/** If true, the character wants to release and resolve the charged attack. */
	bool bHasReleasedChargedAttack = false;

	/** Camera boom length while the character is dead */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float DeathCameraDistance = 400.0f;

	/** Camera boom length when the character respawns */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float DefaultCameraDistance = 100.0f;

	/** Time to wait before respawning the character */
	UPROPERTY(EditAnywhere, Category="Respawn", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float RespawnTime = 3.0f;

	/** Attack montage ended delegate */
	FOnMontageEnded OnAttackMontageEnded;

	/** Character respawn timer */
	FTimerHandle RespawnTimer;

	UPROPERTY(EditAnywhere, Category = "Damage", meta = (ClampMin = 0, ClampMax = 2, Units = "s"))
	float HitRecoveryTime = 0.4f;

	FTimerHandle HitRecoveryTimer;

	/** Quita el ragdoll parcial tras un golpe */
	void RecoverFromHit();

	/** Copy of the mesh's transform so we can reset it after ragdoll animations */
	FTransform MeshStartingTransform;

	/** Sprint Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* SprintAction;

	/** Velocidad al correr */
	UPROPERTY(EditAnywhere, Category = "Sprint")
	float SprintSpeed = 700.0f;

	/** Estamina que se gasta por segundo al correr */
	UPROPERTY(EditAnywhere, Category = "Sprint")
	float SprintStaminaCostPerSecond = 15.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sprint")
	bool bIsSprinting = false;

	float WalkSpeed = 0.0f;

	void SprintStarted();
	void SprintCompleted();

	/** Frío */
	UPROPERTY(EditAnywhere, Category = "Cold")
	bool bColdEnabled = true;

	/** Vida que quita cada tick de frío */
	UPROPERTY(EditAnywhere, Category = "Cold", meta = (ClampMin = 0))
	float ColdDamage = 0.25f;

	/** Segundos entre cada tick de frío */
	UPROPERTY(EditAnywhere, Category = "Cold", meta = (ClampMin = 0.1, Units = "s"))
	float ColdDamageInterval = 3.0f;

	/** Si es true, el frío no hace daño (lo usará el ítem especial) */
	UPROPERTY(BlueprintReadOnly, Category = "Cold")
	bool bColdProtected = false;

	/** Intensidad base del efecto visual de frío (0 a 1) */
	UPROPERTY(EditAnywhere, Category = "Cold|Visual", meta = (ClampMin = 0, ClampMax = 1))
	float ColdVisualIntensity = 0.5f;

	/** Color del tinte de pantalla */
	UPROPERTY(EditAnywhere, Category = "Cold|Visual")
	FLinearColor ColdTint = FLinearColor(0.7f, 0.85f, 1.0f);

	/** Qué tan rápido aparece y desaparece el efecto */
	UPROPERTY(EditAnywhere, Category = "Cold|Visual", meta = (ClampMin = 0.1))
	float ColdVisualBlendSpeed = 1.5f;

	float ColdVisualCurrent = 0.0f;
	float ColdPulse = 0.0f;

	FTimerHandle ColdTimer;

	void ApplyColdTick();

	/** Frascos de vida */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* HealAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heal", meta = (ClampMin = 1))
	int32 MaxHealCharges = 3;

	/** Frascos con los que empieza (y con los que reaparece) */
	UPROPERTY(EditAnywhere, Category = "Heal", meta = (ClampMin = 0))
	int32 StartingHealCharges = 2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heal")
	int32 HealCharges = 0;

	/** Vida que cura cada frasco (tu MaxHP es 5) */
	UPROPERTY(EditAnywhere, Category = "Heal", meta = (ClampMin = 0))
	float HealAmount = 2.0f;

	/** Montaje de beber */
	UPROPERTY(EditAnywhere, Category = "Heal")
	UAnimMontage* HealMontage;

	UPROPERTY(BlueprintReadOnly, Category = "Heal")
	bool bIsDrinking = false;

	void HealPressed();
	void HealMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void FinishHeal();
	void CancelHeal();
	void SetHealCharges(int32 NewValue);

	/** Avisa al HUD que cambió la cantidad de frascos */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnHealChargesChanged(int32 Charges, int32 MaxCharges);

public:
	
	/** Constructor */
	ACombatCharacter();

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for combo attack input */
	void ComboAttackPressed();

	/** Called for combo attack input pressed */
	void ChargedAttackPressed();

	/** Called for combo attack input released */
	void ChargedAttackReleased();

	/** Called for toggle camera side input */
	void ToggleCamera();

	/** BP hook to animate the camera side switch */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void BP_ToggleCamera();

	void DodgePressed();
	void DodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles combo attack pressed from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoComboAttackStart();

	/** Handles combo attack released from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoComboAttackEnd();

	/** Handles charged attack pressed from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoChargedAttackStart();

	/** Handles charged attack released from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoChargedAttackEnd();

	UFUNCTION(BlueprintCallable, Category = "Dodge")
	virtual void DoDodge();

	UFUNCTION(BlueprintCallable, Category = "Dodge")
	void SetInvincible(bool bNewInvincible);

	UFUNCTION(BlueprintCallable, Category = "Melee Attack|Damage")
	void SetMeleeDamage(float NewDamage) { MeleeDamage = NewDamage; }

	UFUNCTION(BlueprintCallable, Category = "Cold")
	void SetColdProtected(bool bProtected) { bColdProtected = bProtected; }

	UFUNCTION(BlueprintCallable, Category = "Heal")
	virtual void DoHeal();

	/** Sube el máximo de frascos y regala ese frasco (frasco ampliado) */
	UFUNCTION(BlueprintCallable, Category = "Heal")
	void IncreaseMaxHealCharges(int32 Amount = 1);

	/** Para el objeto recogible: true si se sumó el frasco, false si ya estaba lleno */
	UFUNCTION(BlueprintCallable, Category = "Heal")
	bool AddHealCharge(int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "Melee Attack")
	void SetComboAttackMontage(UAnimMontage* NewMontage);

protected:

	/** Resets the character's current HP to maximum */
	void ResetHP();

	/** Gasta estamina si alcanza; devuelve false si no */
	bool TryConsumeStamina(float Cost);

	/** Cambia la estamina y avisa al HUD */
	void SetStamina(float NewValue);

	/** Performs a combo attack */
	void ComboAttack();

	/** Performs a charged attack */
	void ChargedAttack();

	/** Called from a delegate when the attack montage ends */
	void AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	
public:

	// ~begin CombatAttacker interface

	/** Performs the collision check for an attack */
	virtual void DoAttackTrace(FName DamageSourceBone) override;

	/** Performs the combo string check */
	virtual void CheckCombo() override;

	/** Performs the charged attack hold check */
	virtual void CheckChargedAttack() override;

	/** Loops or resolves the charged attack animation */
	void LoopOrResolveChargedAttack();

	// ~end CombatAttacker interface

	// ~begin CombatDamageable interface

	/** Notifies nearby enemies that an attack is coming so they can react */
	void NotifyEnemiesOfIncomingAttack();

	/** Handles damage and knockback events */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Handles death events */
	virtual void HandleDeath() override;

	/** Handles healing events */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	/** Allows reaction to incoming attacks */
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	// ~end CombatDamageable interface

	/** Called from the respawn timer to destroy and re-create the character */
	void RespawnCharacter();

public:

	/** Overrides the default TakeDamage functionality */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Overrides landing to reset damage ragdoll physics */
	virtual void Landed(const FHitResult& Hit) override;

	/** Overrides tick to sustain the charged attack lunge velocity */
	virtual void Tick(float DeltaTime) override;

protected:

	/** Blueprint handler to play damage dealt effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void DealtDamage(float Damage, const FVector& ImpactPoint);

	/** Blueprint handler to play damage received effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void ReceivedDamage(float Damage, const FVector& ImpactPoint, const FVector& DamageDirection);

	/** Avisa al HUD que cambió la vida (0 a 1) */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnHealthChanged(float Percent);

	/** Avisa al HUD que cambió la estamina (0 a 1) */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnStaminaChanged(float Percent);

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Handles input bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Handles possessed initialization */
	virtual void NotifyControllerChanged() override;

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
