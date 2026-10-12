// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Variant_Combat/CombatCharacter.h"
#include "AZombieCharacter.generated.h"

/**
 * 
 */
UCLASS()
class DARKFLAKE_API AAZombieCharacter : public ACombatCharacter
{
	GENERATED_BODY()

public:
	/** Muerte del zombi: sin ragdoll, avisa al Blueprint para reproducir la animación */
	virtual void HandleDeath() override;

protected:
	/** Se llama en Blueprint cuando el zombi muere */
	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie")
	void OnZombieDeath();

private:
	bool bIsDead = false;
	
};
