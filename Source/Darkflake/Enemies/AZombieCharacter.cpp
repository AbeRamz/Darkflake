// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemies/AZombieCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void AAZombieCharacter::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	// no llamamos a Super: no queremos ragdoll ni el Destroy a los 3 s

	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// quitar el ragdoll parcial que dejó el último golpe
	GetMesh()->SetAllBodiesSimulatePhysics(false);
	GetMesh()->SetPhysicsBlendWeight(0.0f);

	OnZombieDeath();
}
