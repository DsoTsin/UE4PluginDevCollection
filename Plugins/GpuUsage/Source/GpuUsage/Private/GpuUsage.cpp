// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GpuUsage.h"

#define LOCTEXT_NAMESPACE "FGpuUsageModule"

#if PLATFORM_WINDOWS 
#include "AllowWindowsPlatformTypes.h"
#include "HideWindowsPlatformTypes.h"

#define NVAPI_MAX_PHYSICAL_GPUS   64
#define NVAPI_MAX_USAGES_PER_GPU  34

HMODULE NvApi = NULL;

typedef int *(*NvAPI_QueryInterface_t)(unsigned int offset);
typedef int(*NvAPI_Initialize_t)();
typedef int(*NvAPI_EnumPhysicalGPUs_t)(int **handles, int *count);
typedef int(*NvAPI_GPU_GetUsages_t)(int *handle, unsigned int *usages);

NvAPI_QueryInterface_t      NvAPI_QueryInterface = NULL;
NvAPI_Initialize_t          NvAPI_Initialize = NULL;
NvAPI_EnumPhysicalGPUs_t    NvAPI_EnumPhysicalGPUs = NULL;
NvAPI_GPU_GetUsages_t       NvAPI_GPU_GetUsages = NULL;

int                         NvGpuCount = 0;
int*                        NvGpuHandles[NVAPI_MAX_PHYSICAL_GPUS] = { NULL };
unsigned int                NvGpuUsages[NVAPI_MAX_USAGES_PER_GPU] = { 0 };
float                       NvGpuUsagesPercents[NVAPI_MAX_USAGES_PER_GPU] = { 0 };

#endif

void FGpuUsageModule::StartupModule()
{
#if PLATFORM_WINDOWS

#if PLATFORM_64BITS
  NvApi = LoadLibraryW(L"nvapi64.dll");
#else
  NvApi = LoadLibraryW(L"nvapi.dll");
#endif
  if (NvApi == NULL)
  {
    return;
  }
  NvAPI_QueryInterface = (NvAPI_QueryInterface_t)GetProcAddress(NvApi, "nvapi_QueryInterface");
  NvAPI_Initialize = (NvAPI_Initialize_t)(*NvAPI_QueryInterface)(0x0150E828);
  NvAPI_EnumPhysicalGPUs = (NvAPI_EnumPhysicalGPUs_t)(*NvAPI_QueryInterface)(0xE5AC921F);
  NvAPI_GPU_GetUsages = (NvAPI_GPU_GetUsages_t)(*NvAPI_QueryInterface)(0x189A1FDF);
  if (NvAPI_Initialize == NULL || NvAPI_EnumPhysicalGPUs == NULL ||
    NvAPI_EnumPhysicalGPUs == NULL || NvAPI_GPU_GetUsages == NULL)
  {
    return;
  }
  (*NvAPI_Initialize)();
  NvGpuUsages[0] = (NVAPI_MAX_USAGES_PER_GPU * 4) | 0x10000;
  (*NvAPI_EnumPhysicalGPUs)(NvGpuHandles, &NvGpuCount);
#endif
}

void FGpuUsageModule::ShutdownModule()
{
#if PLATFORM_WINDOWS 
  if (NvApi)
  {
    FreeLibrary(NvApi);
  }
#endif
}

int FGpuUsageModule::QueryCurrentLoad()
{
#if PLATFORM_WINDOWS 
  if (NvApi)
  {
    (*NvAPI_GPU_GetUsages)(NvGpuHandles[0], NvGpuUsages);
    return NvGpuUsages[3];
  }
#endif
  return 0;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGpuUsageModule, GpuUsage)