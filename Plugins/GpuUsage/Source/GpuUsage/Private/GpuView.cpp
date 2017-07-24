// Fill out your copyright notice in the Description page of Project Settings.

#include "GpuView.h"
#include "GpuUsage.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"

// Needed for VR Headset
#include "Engine.h"
#if HMD_MODULE_INCLUDED
#include "IHeadMountedDisplay.h"
#endif  // HMD_MODULE_INCLUDED


AGpuView::AGpuView()
{
  static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Engine/EngineFonts/RobotoDistanceField"));
  HUDFont = Font.Object;
}

void AGpuView::DrawHUD()
{
  Super::DrawHUD();

  bool bHMDDeviceActive = false;

  // We dont want the onscreen hud when using a HMD device	
#if HMD_MODULE_INCLUDED
  if (GEngine->HMDDevice.IsValid() == true)
  {
    bHMDDeviceActive = GEngine->HMDDevice->IsStereoEnabled();
  }
#endif // HMD_MODULE_INCLUDED
  if (bHMDDeviceActive == false)
  {
    auto Module = FModuleManager::GetModuleChecked<FGpuUsageModule>(FName("GpuUsage"));
    FVector2D ScaleVec(1.4f, 1.4f);
    FCanvasTextItem SpeedTextItem(FVector2D(20.f, 20.f), FText::AsNumber(Module.QueryCurrentLoad()), HUDFont, FLinearColor::White);
    SpeedTextItem.Scale = ScaleVec;
    Canvas->DrawItem(SpeedTextItem);
  }
}
