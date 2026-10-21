// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "CombatLifeBar.h"
#include "Engine/DamageEvents.h"
#include "TimerManager.h"
#include "Engine/LocalPlayer.h"
#include "CombatPlayerController.h"

ACombatCharacter::ACombatCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// bind the attack montage ended delegate
	OnAttackMontageEnded.BindUObject(this, &ACombatCharacter::AttackMontageEnded);

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	// Configure character movement
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;

	// create the camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);

	CameraBoom->TargetArmLength = DefaultCameraDistance;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = true;

	// create the orbiting camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// create the life bar widget component
	LifeBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("LifeBar"));
	LifeBar->SetupAttachment(RootComponent);

	// set the player tag
	Tags.Add(FName("Player"));
}

void ACombatCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ACombatCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ACombatCharacter::ComboAttackPressed()
{
	// route the input
	DoComboAttackStart();
}

void ACombatCharacter::ChargedAttackPressed()
{
	// route the input
	DoChargedAttackStart();
}

void ACombatCharacter::ChargedAttackReleased()
{
	// route the input
	DoChargedAttackEnd();
}

void ACombatCharacter::ToggleCamera()
{
	// call the BP hook
	BP_ToggleCamera();
}

void ACombatCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// Cache this for the dodge to use, since it's more reliable than GetLastInputVector()
		LastMovementInputVector = (ForwardDirection * Forward) + (RightDirection * Right);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ACombatCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ACombatCharacter::DoComboAttackStart()
{
	if (bIsDrinking) { return; }

	// are we already playing an attack animation?
	if (bIsAttacking)
	{
		// cache the input time so we can check it later
		CachedAttackInputTime = GetWorld()->GetTimeSeconds();

		return;
	}

	// perform a combo attack
	ComboAttack();
}

void ACombatCharacter::DoComboAttackEnd()
{
	// stub
}

void ACombatCharacter::DoChargedAttackStart()
{
	if (bIsDrinking) { return; }

	// raise the charging attack flag
	bIsChargingAttack = true;

	if (bIsAttacking)
	{
		// do not attack if the charge animation hasn't looped at least once
		if (!bHasLoopedChargedAttack)
		{
			bHasReleasedChargedAttack = false;
			ChargeStartTime = GetWorld()->GetTimeSeconds();
			bIsWeakChargedAttack = false;
		}

		// cache the input time so we can check it later
		CachedAttackInputTime = GetWorld()->GetTimeSeconds();

		return;
	}

	ChargedAttack();
}

void ACombatCharacter::DoChargedAttackEnd()
{
	// lower the charging attack flag
	bIsChargingAttack = false;

	// have we done the charge loop at least once and haven't released the button yet?
	if (bHasLoopedChargedAttack && !bHasReleasedChargedAttack)
	{
		// release the charge and resolve the attack
		bHasReleasedChargedAttack = true;

		LoopOrResolveChargedAttack();
	}
}

void ACombatCharacter::ResetHP()
{
	// reset the current HP total
	CurrentHP = MaxHP;

	// update the life bar
	LifeBarWidget->SetLifePercentage(1.0f);
	OnHealthChanged(1.0f);
}

void ACombatCharacter::SetStamina(float NewValue)
{
	CurrentStamina = FMath::Clamp(NewValue, 0.0f, MaxStamina);
	OnStaminaChanged(CurrentStamina / MaxStamina);
}

bool ACombatCharacter::TryConsumeStamina(float Cost)
{
	if (CurrentStamina < Cost)
	{
		return false;
	}

	SetStamina(CurrentStamina - Cost);
	LastStaminaUseTime = GetWorld()->GetTimeSeconds();
	return true;
}

void ACombatCharacter::ComboAttack()
{
	if (!TryConsumeStamina(AttackStaminaCost))
	{
		return;
	}

	// raise the attacking flag
	bIsAttacking = true;

	// reset the combo count
	ComboCount = 0;

	// notify enemies they are about to be attacked
	NotifyEnemiesOfIncomingAttack();

	// play the attack montage
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(ComboAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

		// subscribe to montage completed and interrupted events
		if (MontageLength > 0.0f)
		{
			// set the end delegate for the montage
			AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ComboAttackMontage);
		}
	}

}

void ACombatCharacter::ChargedAttack()
{
	if (!TryConsumeStamina(AttackStaminaCost))
	{
		return;
	}

	// raise the attacking flag
	bIsAttacking = true;

	// reset the charge loop flag
	bHasLoopedChargedAttack = false;

	// reset the charge release flag
	bHasReleasedChargedAttack = false;

	// notify enemies they are about to be attacked
	NotifyEnemiesOfIncomingAttack();

	// play the charged attack montage
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(ChargedAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

		// subscribe to montage completed and interrupted events
		if (MontageLength > 0.0f)
		{
			// set the end delegate for the montage
			AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ChargedAttackMontage);
		}
	}
}

void ACombatCharacter::AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// reset the attacking flag
	bIsAttacking = false;

	// check if we have a non-stale cached input
	if (GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= AttackInputCacheTimeTolerance)
	{
		// are we holding the charged attack button?
		if (bIsChargingAttack)
		{
			// do a charged attack
			ChargedAttack();
		}
		else
		{
			// do a regular attack
			ComboAttack();
		}
	}
}

void ACombatCharacter::DoAttackTrace(FName DamageSourceBone)
{
	// sweep for objects in front of the character to be hit by the attack
	TArray<FHitResult> OutHits;

	// start at the provided socket location, sweep forward
	const FVector TraceStart = GetMesh()->GetSocketLocation(DamageSourceBone);
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * MeleeTraceDistance);

	// check for pawn and world dynamic collision object types
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// use a sphere shape for the sweep
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(MeleeTraceRadius);

	// ignore self
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		// iterate over each object hit
		for (const FHitResult& CurrentHit : OutHits)
		{
			// check if we've hit a damageable actor
			ICombatDamageable* Damageable = Cast<ICombatDamageable>(CurrentHit.GetActor());

			if (Damageable)
			{
				// knock upwards and away from the impact normal
				const FVector Impulse = (CurrentHit.ImpactNormal * -MeleeKnockbackImpulse) + (FVector::UpVector * MeleeLaunchImpulse);

				// pass the damage event to the actor
				Damageable->ApplyDamage(MeleeDamage, this, CurrentHit.ImpactPoint, Impulse);

				// call the BP handler to play effects, etc.
				DealtDamage(MeleeDamage, CurrentHit.ImpactPoint);
			}
		}
	}
}

void ACombatCharacter::CheckCombo()
{
	// are we playing a non-charge attack animation?
	if (bIsAttacking && !bIsChargingAttack)
	{
		// is the last attack input not stale?
		if (GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= ComboInputCacheTimeTolerance)
		{
			// consume the attack input so we don't accidentally trigger it twice
			CachedAttackInputTime = 0.0f;

			// increase the combo counter
			++ComboCount;

			// do we still have a combo section to play?
			if (ComboCount < ComboSectionNames.Num())
			{
				if (!TryConsumeStamina(AttackStaminaCost))
				{
					--ComboCount;
					return;
				}

				// notify enemies they are about to be attacked
				NotifyEnemiesOfIncomingAttack();

				// jump to the next combo section
				if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
				{
					AnimInstance->Montage_JumpToSection(ComboSectionNames[ComboCount], ComboAttackMontage);
				}
			}
		}
	}
}

void ACombatCharacter::CheckChargedAttack()
{
	// raise the looped charged attack flag
	bHasLoopedChargedAttack = true;

	// set the input release flag from the input. This will determine if we loop or resolve
	bHasReleasedChargedAttack = !bIsChargingAttack;

	// resolve the charge loop
	LoopOrResolveChargedAttack();
}

void ACombatCharacter::LoopOrResolveChargedAttack()
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (bHasReleasedChargedAttack)
		{
			const float ChargeDuration = GetWorld()->GetTimeSeconds() - ChargeStartTime;
			bIsWeakChargedAttack = ChargeDuration < MinChargeTime;

			AnimInstance->Montage_SetPlayRate(ChargedAttackMontage, bIsWeakChargedAttack ? WeakAttackPlayRateMultiplier : 1.0f);

			// keep pushing forward for the whole swing duration instead of one instant burst
			LungeVelocity = GetActorForwardVector() * (ChargedAttackLungeDistance / ChargedAttackLungeDuration);
			LungeTimeRemaining = ChargedAttackLungeDuration;
			bIsLunging = true;
		}

		AnimInstance->Montage_JumpToSection(bHasReleasedChargedAttack ? ChargeAttackSection : ChargeLoopSection, ChargedAttackMontage);
	}
}

void ACombatCharacter::NotifyEnemiesOfIncomingAttack()
{
	// sweep for objects in front of the character to be hit by the attack
	TArray<FHitResult> OutHits;

	// start at the actor location, sweep forward
	const FVector TraceStart = GetActorLocation();
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * DangerTraceDistance);

	// check for pawn object types only
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	// use a sphere shape for the sweep
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(DangerTraceRadius);

	// ignore self
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		// iterate over each object hit
		for (const FHitResult& CurrentHit : OutHits)
		{
			// check if we've hit a damageable actor
			ICombatDamageable* Damageable = Cast<ICombatDamageable>(CurrentHit.GetActor());

			if (Damageable)
			{
				// notify the enemy
				Damageable->NotifyDanger(GetActorLocation(), this);
			}
		}
	}
}

void ACombatCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	// ignore damage entirely while invincible (e.g. mid-dodge i-frames)
	if (bIsInvincible)
	{
		return;
	}

	// pass the damage event to the actor
	FDamageEvent DamageEvent;
	const float ActualDamage = TakeDamage(Damage, DamageEvent, nullptr, DamageCauser);

	// only process knockback and effects if we received nonzero damage
	if (ActualDamage > 0.0f)
	{
		// apply the knockback impulse
		GetCharacterMovement()->AddImpulse(DamageImpulse, true);

		// is the character ragdolling?
		if (GetMesh()->IsSimulatingPhysics())
		{
			// apply an impulse to the ragdoll
			GetMesh()->AddImpulseAtLocation(DamageImpulse * GetMesh()->GetMass(), DamageLocation);
		}

		// pass control to BP to play effects, etc.
		ReceivedDamage(ActualDamage, DamageLocation, DamageImpulse.GetSafeNormal());
	}

}

void ACombatCharacter::HandleDeath()
{
	// disable movement while we're dead
	GetCharacterMovement()->DisableMovement();

	// enable full ragdoll physics
	GetMesh()->SetSimulatePhysics(true);

	// hide the life bar
	LifeBar->SetHiddenInGame(true);

	// pull back the camera
	GetCameraBoom()->TargetArmLength = DeathCameraDistance;

	// schedule respawning
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &ACombatCharacter::RespawnCharacter, RespawnTime, false);
}

void ACombatCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	if (CurrentHP <= 0.0f)
	{
		return;
	}

	CurrentHP = FMath::Clamp(CurrentHP + Healing, 0.0f, MaxHP);

	if (LifeBarWidget)
	{
		LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);
	}
	OnHealthChanged(CurrentHP / MaxHP);
}

void ACombatCharacter::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// stub
}

void ACombatCharacter::RespawnCharacter()
{
	// destroy the character and let it be respawned by the Player Controller
	Destroy();
}

float ACombatCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// only process damage if the character is still alive
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// reduce the current HP
	CurrentHP -= Damage;
	CancelHeal();

	// have we run out of HP?
	if (CurrentHP <= 0.0f)
	{
		// die
		HandleDeath();
		CancelHeal();
	}
	else
	{
		// update the life bar
		LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);

		// enable partial ragdoll physics, but keep the pelvis vertical
		GetMesh()->SetPhysicsBlendWeight(0.5f);
		GetMesh()->SetBodySimulatePhysics(PelvisBoneName, false);

		// recuperar el cuerpo aunque no llegue a haber un Landed
		GetWorld()->GetTimerManager().SetTimer(HitRecoveryTimer, this, &ACombatCharacter::RecoverFromHit, HitRecoveryTime, false);
	}

	// tell the HUD
	OnHealthChanged(FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f));

	// return the received damage amount
	return Damage;
}

void ACombatCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// is the character still alive?
	if (CurrentHP >= 0.0f)
	{
		// disable ragdoll physics
		GetMesh()->SetPhysicsBlendWeight(0.0f);
	}
}

void ACombatCharacter::BeginPlay()
{
	Super::BeginPlay();

	// get the life bar from the widget component
	LifeBarWidget = Cast<UCombatLifeBar>(LifeBar->GetUserWidgetObject());
	check(LifeBarWidget);

	// initialize the camera
	GetCameraBoom()->TargetArmLength = DefaultCameraDistance;

	// save the relative transform for the mesh so we can reset the ragdoll later
	MeshStartingTransform = GetMesh()->GetRelativeTransform();

	// set the life bar color
	LifeBarWidget->SetBarColor(LifeBarColor);

	// reset HP to maximum
	ResetHP();

	// start with full stamina
	SetStamina(MaxStamina);

	SetHealCharges(StartingHealCharges);

	WalkSpeed = GetCharacterMovement()->MaxWalkSpeed;

	GetWorldTimerManager().SetTimer(ColdTimer, this, &ACombatCharacter::ApplyColdTick, ColdDamageInterval, true);

	// configurar el post process de frío (empieza apagado)
	FPostProcessSettings& PP = FollowCamera->PostProcessSettings;
	PP.bOverride_SceneColorTint = true;
	PP.SceneColorTint = ColdTint;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 1.0f;
	FollowCamera->PostProcessBlendWeight = 0.0f;
}

void ACombatCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
	GetWorldTimerManager().ClearTimer(ColdTimer);
}

void ACombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Look);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Look);

		// Combo Attack
		EnhancedInputComponent->BindAction(ComboAttackAction, ETriggerEvent::Started, this, &ACombatCharacter::ComboAttackPressed);

		// Charged Attack
		EnhancedInputComponent->BindAction(ChargedAttackAction, ETriggerEvent::Started, this, &ACombatCharacter::ChargedAttackPressed);
		EnhancedInputComponent->BindAction(ChargedAttackAction, ETriggerEvent::Completed, this, &ACombatCharacter::ChargedAttackReleased);

		// Camera Side Toggle
		EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Triggered, this, &ACombatCharacter::ToggleCamera);

		// Sprint
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &ACombatCharacter::SprintStarted);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &ACombatCharacter::SprintCompleted);

		// Dodge
		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &ACombatCharacter::DodgePressed);

		// Heal
		if (HealAction)
		{
			EnhancedInputComponent->BindAction(HealAction, ETriggerEvent::Started, this, &ACombatCharacter::HealPressed);
		}
	}
}

void ACombatCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// update the respawn transform on the Player Controller
	if (ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetController()))
	{
		PC->SetRespawnTransform(GetActorTransform());
	}
}

void ACombatCharacter::DodgePressed()
{
	DoDodge();
}

void ACombatCharacter::DoDodge()
{
	if (bIsDodging || bIsAttacking || bIsDrinking)

	if (!TryConsumeStamina(DodgeStaminaCost))
	{
		return;
	}

	bIsDodging = true;
	SprintCompleted();

	bUseControllerRotationYaw = false;

	FVector RollDirection = GetActorForwardVector();
	if (!LastMovementInputVector.IsNearlyZero())
	{
		const FRotator FacingRotation(0.0f, LastMovementInputVector.Rotation().Yaw, 0.0f);
		SetActorRotation(FacingRotation);
		RollDirection = LastMovementInputVector.GetSafeNormal();
	}

	const FVector LaunchVelocity = RollDirection * (DodgeDistance / DodgeDuration);
	LaunchCharacter(FVector(LaunchVelocity.X, LaunchVelocity.Y, 0.0f), true, true);

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(DodgeMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

		if (MontageLength > 0.0f)
		{
			FOnMontageEnded DodgeEndDelegate;
			DodgeEndDelegate.BindUObject(this, &ACombatCharacter::DodgeMontageEnded);
			AnimInstance->Montage_SetEndDelegate(DodgeEndDelegate, DodgeMontage);
		}
	}
}

void ACombatCharacter::DodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	bIsDodging = false;
	bIsInvincible = false;

	// Give rotation control back to the camera now that the dodge is done
	bUseControllerRotationYaw = true;

	GetMesh()->SetRelativeRotation(MeshStartingTransform.Rotator());
}

void ACombatCharacter::SetInvincible(bool bNewInvincible)
{
	bIsInvincible = bNewInvincible;
}

void ACombatCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// regenerate stamina after a short pause without spending it
	if (CurrentStamina < MaxStamina && !bIsDodging && !bIsAttacking && !bIsSprinting
		&& GetWorld()->GetTimeSeconds() - LastStaminaUseTime >= StaminaRegenDelay)
	{
		SetStamina(CurrentStamina + StaminaRegenRate * DeltaTime);
	}

	// gastar estamina mientras corre (solo si realmente se está moviendo)
	if (bIsSprinting)
	{
		if (GetVelocity().Size2D() > 10.0f)
		{
			SetStamina(CurrentStamina - SprintStaminaCostPerSecond * DeltaTime);
			LastStaminaUseTime = GetWorld()->GetTimeSeconds();
		}

		if (CurrentStamina <= 0.0f)
		{
			SprintCompleted();
		}
	}

	// efecto visual de frío
	const bool bColdActive = bColdEnabled && !bColdProtected && CurrentHP > 0.0f;
	ColdVisualCurrent = FMath::FInterpTo(ColdVisualCurrent, bColdActive ? 1.0f : 0.0f, DeltaTime, ColdVisualBlendSpeed);
	ColdPulse = FMath::FInterpTo(ColdPulse, 0.0f, DeltaTime, 3.0f);

	FollowCamera->PostProcessBlendWeight = ColdVisualCurrent * FMath::Clamp(ColdVisualIntensity + ColdPulse * 0.3f, 0.0f, 1.0f);
}

//New Montage
void ACombatCharacter::SetComboAttackMontage(UAnimMontage* NewMontage)
{
	if (NewMontage)
	{
		ComboAttackMontage = NewMontage;
	}
}

void ACombatCharacter::SetChargedAttackMontage(UAnimMontage* NewMontage)
{
	if (NewMontage)
	{
		ChargedAttackMontage = NewMontage;
	}
}

void ACombatCharacter::RecoverFromHit()
{
	if (CurrentHP > 0.0f)
	{
		GetMesh()->SetPhysicsBlendWeight(0.0f);
	}
}

//Sprint
void ACombatCharacter::SprintStarted()
{
	if (bIsDodging || bIsDrinking || CurrentStamina <= 0.0f)

	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ACombatCharacter::SprintCompleted()
{
	if (!bIsSprinting)
	{
		return;
	}

	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

//Daño por Frío
void ACombatCharacter::ApplyColdTick()
{
	if (!bColdEnabled || bColdProtected || CurrentHP <= 0.0f)
	{
		return;
	}

	CurrentHP -= ColdDamage;
	ColdPulse = 1.0f;   // pulso visual con cada tick

	if (CurrentHP <= 0.0f)
	{
		HandleDeath();
	}

	OnHealthChanged(FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f));
}

//Frascos de vida
void ACombatCharacter::HealPressed()
{
	DoHeal();
}

void ACombatCharacter::DoHeal()
{
	if (bIsDrinking || bIsAttacking || bIsDodging)
	{
		return;
	}

	// sin frascos, muerto o con la vida llena: no se bebe
	if (HealCharges <= 0 || CurrentHP <= 0.0f || CurrentHP >= MaxHP)
	{
		return;
	}

	SprintCompleted();

	if (HealMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			const float MontageLength = AnimInstance->Montage_Play(HealMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

			if (MontageLength > 0.0f)
			{
				bIsDrinking = true;

				FOnMontageEnded HealEndDelegate;
				HealEndDelegate.BindUObject(this, &ACombatCharacter::HealMontageEnded);
				AnimInstance->Montage_SetEndDelegate(HealEndDelegate, HealMontage);
				return;
			}
		}
	}

	// sin montaje todavía: cura al instante (útil para probar)
	FinishHeal();
}

void ACombatCharacter::HealMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	bIsDrinking = false;

	// si lo interrumpieron (golpe o muerte) no se cura ni se gasta el frasco
	if (!bInterrupted)
	{
		FinishHeal();
	}
}

void ACombatCharacter::FinishHeal()
{
	SetHealCharges(HealCharges - 1);
	ApplyHealing(HealAmount, this);
}

void ACombatCharacter::CancelHeal()
{
	if (!bIsDrinking)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.1f, HealMontage);
	}
}

void ACombatCharacter::SetHealCharges(int32 NewValue)
{
	HealCharges = FMath::Clamp(NewValue, 0, MaxHealCharges);
	OnHealChargesChanged(HealCharges, MaxHealCharges);
}

bool ACombatCharacter::AddHealCharge(int32 Amount)
{
	if (HealCharges >= MaxHealCharges)
	{
		return false;
	}

	SetHealCharges(HealCharges + Amount);
	return true;
}

void ACombatCharacter::IncreaseMaxHealCharges(int32 Amount)
{
	MaxHealCharges += Amount;
	SetHealCharges(HealCharges + Amount);
}