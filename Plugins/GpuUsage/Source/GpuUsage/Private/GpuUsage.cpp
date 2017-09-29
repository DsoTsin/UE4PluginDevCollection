// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GpuUsage.h"

#ifndef DISABLE_NVPI
#include "nvapi.h"
#endif



//class NvVideoMemoryMonitor
//{
//public:
//    struct MemInfo
//    {
//        unsigned int DedicatedVideoMemoryInMB;
//        unsigned int AvailableDedicatedVideoMemoryInMB;
//        unsigned int CurrentAvailableDedicatedVideoMemoryInMB;
//    };
//
//    NvVideoMemoryMonitor()
//        : m_GpuHandle(0)
//    {
//    }
//
//    void Init()
//    {
//        NvAPI_Status Status = NvAPI_Initialize();
//        assert(Status == NVAPI_OK);
//
//        NvPhysicalGpuHandle NvGpuHandles[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
//        NvU32 NvGpuCount = 0;
//        Status = NvAPI_EnumPhysicalGPUs(NvGpuHandles, &NvGpuCount);
//        assert(Status == NVAPI_OK);
//        assert(NvGpuCount != 0);
//        m_GpuHandle = NvGpuHandles[0];
//    }
//
//    void GetVideoMemoryInfo(MemInfo* pInfo)
//    {
//        NV_DISPLAY_DRIVER_MEMORY_INFO_V2 MemInfo = { 0 };
//        MemInfo.version = NV_DISPLAY_DRIVER_MEMORY_INFO_VER_2;
//        NvAPI_Status Status = NvAPI_GPU_GetMemoryInfo(m_GpuHandle, &MemInfo);
//        assert(Status == NVAPI_OK);
//
//        pInfo->DedicatedVideoMemoryInMB = MemInfo.dedicatedVideoMemory / 1024;
//        pInfo->AvailableDedicatedVideoMemoryInMB = MemInfo.availableDedicatedVideoMemory / 1024;
//        pInfo->CurrentAvailableDedicatedVideoMemoryInMB = MemInfo.curAvailableDedicatedVideoMemory / 1024;
//    }
//
//private:
//    NvPhysicalGpuHandle m_GpuHandle;
//};

#define LOCTEXT_NAMESPACE "FGpuUsageModule"

class FGpuUsageModule : public IGpuUsageModule
{
public:

    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    int             QueryCurrentLoad() override;
    FGpuMemoryInfo  QueryVideoMemory(int index) override;

private:

#ifndef DISABLE_NVPI
    NV_GPU_DYNAMIC_PSTATES_INFO_EX  m_StateInfo;
    NvPhysicalGpuHandle             m_GpuHandle;
#endif
};

#if PLATFORM_WINDOWS 
#include "AllowWindowsPlatformTypes.h"
#include "HideWindowsPlatformTypes.h"
//
//#define NVAPI_MAX_PHYSICAL_GPUS   64
//#define NVAPI_MAX_USAGES_PER_GPU  34

HMODULE NvApi = NULL;
//
//typedef int *(*NvAPI_QueryInterface_t)(unsigned int offset);
//typedef int(*NvAPI_Initialize_t)();
//typedef int(*NvAPI_EnumPhysicalGPUs_t)(int **handles, int *count);
//typedef int(*NvAPI_GPU_GetUsages_t)(int *handle, unsigned int *usages);
//
//NvAPI_QueryInterface_t      NvAPI_QueryInterface = NULL;
//NvAPI_Initialize_t          NvAPI_Initialize = NULL;
//NvAPI_EnumPhysicalGPUs_t    NvAPI_EnumPhysicalGPUs = NULL;
//NvAPI_GPU_GetUsages_t       NvAPI_GPU_GetUsages = NULL;
//
//int                         NvGpuCount = 0;
//int*                        NvGpuHandles[NVAPI_MAX_PHYSICAL_GPUS] = { NULL };
//unsigned int                NvGpuUsages[NVAPI_MAX_USAGES_PER_GPU] = { 0 };
//float                       NvGpuUsagesPercents[NVAPI_MAX_USAGES_PER_GPU] = { 0 };

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
#endif

  NvAPI_Status Status = NvAPI_Initialize();
  if (Status != NVAPI_OK)
      return;

  NvPhysicalGpuHandle NvGpuHandles[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
  NvU32 NvGpuCount = 0;
  Status = NvAPI_EnumPhysicalGPUs(NvGpuHandles, &NvGpuCount);
  if (Status != NVAPI_OK || NvGpuCount == 0)
      return;
  m_GpuHandle = NvGpuHandles[0];
  m_StateInfo = {};
  m_StateInfo.version = NV_GPU_DYNAMIC_PSTATES_INFO_EX_VER;
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
    /*(*NvAPI_GPU_GetUsages)(NvGpuHandles[0], NvGpuUsages);
    return NvGpuUsages[3];*/
      NvAPI_GPU_GetDynamicPstatesInfoEx(m_GpuHandle, &m_StateInfo);
      return m_StateInfo.utilization[0].percentage;
  }
#endif
  return 0;
}

FGpuMemoryInfo FGpuUsageModule::QueryVideoMemory(int index)
{
    FGpuMemoryInfo Info;
    NV_DISPLAY_DRIVER_MEMORY_INFO MemInfo = { 0 };
    MemInfo.version = NV_DISPLAY_DRIVER_MEMORY_INFO_VER_3;
    NvAPI_Status Status = NvAPI_GPU_GetMemoryInfo(m_GpuHandle, &MemInfo);
    Info.DedicatedVideoMemoryInMB = MemInfo.dedicatedVideoMemory / 1024;
    Info.AvailableDedicatedVideoMemoryInMB = MemInfo.availableDedicatedVideoMemory / 1024;
    Info.CurrentAvailableDedicatedVideoMemoryInMB = MemInfo.curAvailableDedicatedVideoMemory / 1024;
    return Info;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGpuUsageModule, GpuUsage)