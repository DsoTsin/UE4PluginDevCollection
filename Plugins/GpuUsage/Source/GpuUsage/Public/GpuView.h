// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GpuView.generated.h"

/**
 * 
 */
UCLASS()
class GPUUSAGE_API AGpuView : public AHUD
{
	GENERATED_BODY()
	
public:
  AGpuView();

  /** Font used to render the vehicle info */
  UPROPERTY()
  UFont* HUDFont;

  // Begin AHUD interface
  virtual void DrawHUD() override;
	
};
