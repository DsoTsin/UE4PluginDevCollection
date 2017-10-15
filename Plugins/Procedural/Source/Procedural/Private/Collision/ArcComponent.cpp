// Fill out your copyright notice in the Description page of Project Settings.

#include "Classes/ArcComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysXIncludes.h"
#include "Private/PhysicsEngine/PhysXSupport.h"

class FDrawArcSceneProxy : public FPrimitiveSceneProxy
{
private:
    const uint32	bDrawOnlyIfSelected : 1;
    const float		Radius;
    const float     MinAngle;
    const float		MaxAngle;

public:
    UBodySetup*     BodySetup;
    FDrawArcSceneProxy(UArcComponent* InComponent)
        : FPrimitiveSceneProxy(InComponent)
        , bDrawOnlyIfSelected(InComponent->bDrawOnlyIfSelected)
        , Radius(InComponent->Radius)
        , MinAngle(InComponent->MinAngle)
        , MaxAngle(InComponent->MaxAngle)
        , BodySetup(InComponent->GetBodySetup())
    {
        bWillEverBeLit = false;
    }

    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
    {
        QUICK_SCOPE_CYCLE_COUNTER(STAT_GetDynamicMeshElements_DrawDynamicElements);

        const FMatrix& LocalToWorld = GetLocalToWorld();
        for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
        {
            if (VisibilityMap & (1 << ViewIndex))
            {
                const FSceneView* View = Views[ViewIndex];
                const FLinearColor DrawArcColor = GetViewSelectionColor(FColor(157, 149, 223, 255), *View, IsSelected(), IsHovered(), false, IsIndividuallySelected());

                FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
                ::DrawArc(PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetScaledAxis(EAxis::X), LocalToWorld.GetScaledAxis(EAxis::Y), MinAngle, MaxAngle, Radius, 20, FColor(157, 149, 223, 255), SDPG_World);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
                //if (ViewFamily.EngineShowFlags.Collision && IsCollisionEnabled() && BodySetup->GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseComplexAsSimple)
                FTransform GeomTransform(GetLocalToWorld());
                BodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(FColor(157, 149, 223, 255), IsSelected(), IsHovered()).ToFColor(true), NULL, false, false, UseEditorDepthTest(), ViewIndex, Collector);

                // Render bounds
                RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#endif
            }
        }
    }

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
    {
        const bool bProxyVisible = !bDrawOnlyIfSelected || IsSelected();

        // Should we draw this because collision drawing is enabled, and we have collision
        const bool bShowForCollision = View->Family->EngineShowFlags.Collision && IsCollisionEnabled();

        FPrimitiveViewRelevance Result;
        Result.bDrawRelevance = (IsShown(View) && bProxyVisible) || bShowForCollision;
        Result.bDynamicRelevance = true;
        Result.bShadowRelevance = IsShadowCast(View);
        Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
        return Result;
    }
    virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
    uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

};
// Sets default values for this component's properties
UArcComponent::UArcComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
    ArcBodySetup = nullptr;

    static const FName CollisionProfileName(TEXT("OverlapAllDynamic"));
    BodyInstance.SetCollisionProfileName(CollisionProfileName);
    BodyInstance.ResponseToChannels_DEPRECATED.SetAllChannels(ECR_Block);
    BodyInstance.bAutoWeld = true;	//UShapeComponent by default has auto welding

    bHiddenInGame = true;
    bCastDynamicShadow = false;
}


// Called when the game starts
void UArcComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UArcComponent::PostLoad()
{
    Super::PostLoad();
    if (ArcBodySetup && IsTemplate())
    {
        ArcBodySetup->SetFlags(RF_Public);
    }
}

void UArcComponent::OnRegister()
{
    Super::OnRegister();
    UpdateBodySetup();
}

// Called every frame
void UArcComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

FPrimitiveSceneProxy * UArcComponent::CreateSceneProxy()
{
    return new FDrawArcSceneProxy(this);
}

FBoxSphereBounds 
UArcComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FVector BoxPoint = FVector(Radius, Radius, Radius);
    return FBoxSphereBounds(FVector::ZeroVector, BoxPoint, Radius).TransformBy(LocalToWorld);
}

void UArcComponent::UpdateBodySetup()
{
    if (AngleSlice == 0 || Height < 0.0001f || Thickness < 0.0001f )
        return;

    CollisionConvexElems.Reset();

    float AngleStep = (MaxAngle - MinAngle) / ((float)(AngleSlice));
    float CurrentAngle = MinAngle;
    auto X = GetComponentToWorld().GetScaledAxis(EAxis::X);
    auto Y = GetComponentToWorld().GetScaledAxis(EAxis::Y);
    auto Z = GetComponentToWorld().GetScaledAxis(EAxis::Z);

    auto RadVec = Radius * (FMath::Cos(CurrentAngle * (PI / 180.0f)) * X + FMath::Sin(CurrentAngle * (PI / 180.0f)) * Y);
    auto InnerVec = (Radius - Thickness) * (FMath::Cos(CurrentAngle * (PI / 180.0f)) * X + FMath::Sin(CurrentAngle * (PI / 180.0f)) * Y);

    FVector LastVertex = RadVec;
    FVector LastBottomVertex = FVector(0, 0, -Height) + RadVec;
    FVector LastInnerVertex = InnerVec;
    FVector LastBottomInnerVertex = FVector(0, 0, -Height) + InnerVec;

    CurrentAngle += AngleStep;

    for (int32 i = 0; i < AngleSlice; i++)
    {
        FKConvexElem Elem;
        auto NewRadVec = Radius * (FMath::Cos(CurrentAngle * (PI / 180.0f)) * X + FMath::Sin(CurrentAngle * (PI / 180.0f)) * Y);
        auto NewInnerVec = (Radius - Thickness) * (FMath::Cos(CurrentAngle * (PI / 180.0f)) * X + FMath::Sin(CurrentAngle * (PI / 180.0f)) * Y);

        FVector ThisVertex = NewRadVec;
        FVector BottomVertex = FVector(0, 0, -Height) + NewRadVec;
        FVector InnerVertex = NewInnerVec;
        FVector BottomInnerVertex = FVector(0, 0, -Height) + NewInnerVec;
        
        Elem.VertexData.Add(ThisVertex);
        Elem.VertexData.Add(InnerVertex);
        Elem.VertexData.Add(LastVertex);
        Elem.VertexData.Add(LastInnerVertex);
        Elem.VertexData.Add(BottomVertex);
        Elem.VertexData.Add(BottomInnerVertex);
        Elem.VertexData.Add(LastBottomVertex);
        Elem.VertexData.Add(LastBottomInnerVertex);

        Elem.ElemBox = FBox(Elem.VertexData);

        LastVertex = ThisVertex;
        LastInnerVertex = InnerVertex;
        LastBottomVertex = BottomVertex;
        LastBottomInnerVertex = BottomInnerVertex;
        
        CurrentAngle += AngleStep;

        CollisionConvexElems.Add(Elem);
    }
    UpdateCollision();
}

UBodySetup* 
UArcComponent::GetBodySetup()
{
    if (ArcBodySetup == nullptr)
    {
        ArcBodySetup = CreateBodySetup();
    }
    return ArcBodySetup;
}

bool UArcComponent::GetPhysicsTriMeshData(FTriMeshCollisionData * CollisionData, bool InUseAllTriData)
{
    if (AngleSlice == 0)
        return false;

    int32 VertexBase = 0; // Base vertex index for current section
    bool bCopyUVs = UPhysicsSettings::Get()->bSupportUVFromHitResults;
    if (bCopyUVs)
    {
        CollisionData->UVs.AddZeroed(1); // only one UV channel
    }
    
    float AngleStep = (MaxAngle - MinAngle) / ((float)(AngleSlice));
    float CurrentAngle = MinAngle;
    auto X = GetComponentToWorld().GetScaledAxis(EAxis::X);
    auto Y = GetComponentToWorld().GetScaledAxis(EAxis::Y);
    auto Z = GetComponentToWorld().GetScaledAxis(EAxis::Z);

    auto RadVec = Radius * (FMath::Cos(CurrentAngle * (PI / 180.0f)) * X + FMath::Sin(CurrentAngle * (PI / 180.0f)) * Y);
    auto InnerVec = (Radius - Thickness) * (FMath::Cos(CurrentAngle * (PI / 180.0f)) * X + FMath::Sin(CurrentAngle * (PI / 180.0f)) * Y);

    FVector LastVertex = RadVec;
    FVector LastBottomVertex = FVector(0, 0, -Height) + RadVec;
    FVector LastInnerVertex = InnerVec;
    FVector LastBottomInnerVertex = FVector(0, 0, -Height) + InnerVec;

    CollisionData->Vertices.Add(LastVertex);
    CollisionData->Vertices.Add(LastBottomVertex);
    CollisionData->Vertices.Add(LastInnerVertex);
    CollisionData->Vertices.Add(LastBottomInnerVertex);

    FTriIndices Indices;
    Indices.v0 = 0;
    Indices.v1 = 1;
    Indices.v2 = 2;
    CollisionData->Indices.Add(Indices);

    Indices.v0 = 1;
    Indices.v1 = 3;
    Indices.v2 = 2;
    CollisionData->Indices.Add(Indices);

    CurrentAngle += AngleStep;

    for (int32 i = 0; i < AngleSlice; i++)
    {
        FKConvexElem Elem;
        auto NewRadVec = Radius * (FMath::Cos(CurrentAngle * (PI / 180.0f)) * X + FMath::Sin(CurrentAngle * (PI / 180.0f)) * Y);
        auto NewInnerVec = (Radius - Thickness) * (FMath::Cos(CurrentAngle * (PI / 180.0f)) * X + FMath::Sin(CurrentAngle * (PI / 180.0f)) * Y);

        FVector ThisVertex = NewRadVec;
        FVector BottomVertex = FVector(0, 0, -Height) + NewRadVec;
        FVector InnerVertex = NewInnerVec;
        FVector BottomInnerVertex = FVector(0, 0, -Height) + NewInnerVec;

        CollisionData->Vertices.Add(ThisVertex);
        CollisionData->Vertices.Add(BottomVertex);
        CollisionData->Vertices.Add(InnerVertex);
        CollisionData->Vertices.Add(BottomInnerVertex);

        // Top
        Indices.v0 = i * 4;
        Indices.v1 = i * 4 + 6;
        Indices.v2 = i * 4 + 4;
        CollisionData->Indices.Add(Indices);
        Indices.v0 = i * 4;
        Indices.v1 = i * 4 + 2;
        Indices.v2 = i * 4 + 6;
        CollisionData->Indices.Add(Indices);

        // Bottom
        Indices.v0 = i * 4 + 1;
        Indices.v1 = i * 4 + 5;
        Indices.v2 = i * 4 + 7;
        CollisionData->Indices.Add(Indices);
        Indices.v0 = i * 4 + 1;
        Indices.v1 = i * 4 + 7;
        Indices.v2 = i * 4 + 3;
        CollisionData->Indices.Add(Indices);

        // Inner
        Indices.v0 = i * 4 + 2;
        Indices.v1 = i * 4 + 3;
        Indices.v2 = i * 4 + 7;
        CollisionData->Indices.Add(Indices);
        Indices.v0 = i * 4 + 2;
        Indices.v1 = i * 4 + 7;
        Indices.v2 = i * 4 + 6;
        CollisionData->Indices.Add(Indices);

        // Outter
        Indices.v0 = i * 4 + 0;
        Indices.v1 = i * 4 + 4;
        Indices.v2 = i * 4 + 5;
        CollisionData->Indices.Add(Indices);
        Indices.v0 = i * 4 + 0;
        Indices.v1 = i * 4 + 5;
        Indices.v2 = i * 4 + 1;
        CollisionData->Indices.Add(Indices);

        if (i == AngleSlice - 1)
        {
            Indices.v0 = i * 4 + 0;
            Indices.v1 = i * 4 + 2;
            Indices.v2 = i * 4 + 1;
            CollisionData->Indices.Add(Indices);
            Indices.v0 = i * 4 + 2;
            Indices.v1 = i * 4 + 3;
            Indices.v2 = i * 4 + 1;
            CollisionData->Indices.Add(Indices);
        }
    }
    
    CollisionData->bFlipNormals = true;
    CollisionData->bDeformableMesh = true;
    CollisionData->bFastCook = true;

    return true;
}

bool UArcComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
    return true;
}

bool UArcComponent::WantsNegXTriMesh()
{
    return false;
}

void UArcComponent::GetMeshId(FString& OutMeshId)
{
    OutMeshId = FString::Printf(TEXT("Arc_%d_%d_%d_%d_%d_%d"), (int32)MinAngle, (int32)MaxAngle, (int32)Radius, (int32)Thickness, (int32)Height, (int32)AngleSlice);
}

UBodySetup* UArcComponent::CreateBodySetup()
{   
    auto BodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public : RF_NoFlags));
    BodySetup->BodySetupGuid = FGuid::NewGuid();
    BodySetup->bGenerateMirroredCollision = false;
    BodySetup->bDoubleSidedGeometry = true;
    BodySetup->CollisionTraceFlag = bUseComplexAsSimple? CTF_UseComplexAsSimple : CTF_UseDefault;
    return BodySetup;
}

void UArcComponent::UpdateCollision()
{
    UWorld* World = GetWorld();
    const bool bUseAsyncCook = World && World->IsGameWorld() && bUseAsyncCooking;

    if (bUseAsyncCook)
    {
        AsyncBodySetupQueue.Add(CreateBodySetup());
    }
    else
    {
        AsyncBodySetupQueue.Empty();	//If for some reason we modified the async at runtime, just clear any pending async body setups
        if (ArcBodySetup == nullptr)
        {
            ArcBodySetup = CreateBodySetup();
        }
    }

    UBodySetup* UseBodySetup = bUseAsyncCook ? AsyncBodySetupQueue.Last() : ArcBodySetup;

    // Fill in simple collision convex elements
    UseBodySetup->AggGeom.ConvexElems = CollisionConvexElems;

    // Set trace flag
    UseBodySetup->CollisionTraceFlag = bUseComplexAsSimple ? CTF_UseComplexAsSimple : CTF_UseDefault;

    if (bUseAsyncCook)
    {
        UseBodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &UArcComponent::FinishPhysicsAsyncCook, UseBodySetup));
    }
    else
    {
        // New GUID as collision has changed
        UseBodySetup->BodySetupGuid = FGuid::NewGuid();
        // Also we want cooked data for this
        UseBodySetup->bHasCookedCollisionData = true;
        UseBodySetup->InvalidatePhysicsData();
        UseBodySetup->CreatePhysicsMeshes();
        RecreatePhysicsState();
    }

}

void UArcComponent::FinishPhysicsAsyncCook(UBodySetup * FinishedBodySetup)
{
    TArray<UBodySetup*> NewQueue;
    NewQueue.Reserve(AsyncBodySetupQueue.Num());

    int32 FoundIdx;
    if (AsyncBodySetupQueue.Find(FinishedBodySetup, FoundIdx))
    {
        //The new body was found in the array meaning it's newer so use it
        ArcBodySetup = FinishedBodySetup;
        RecreatePhysicsState();

        //remove any async body setups that were requested before this one
        for (int32 AsyncIdx = FoundIdx + 1; AsyncIdx < AsyncBodySetupQueue.Num(); ++AsyncIdx)
        {
            NewQueue.Add(AsyncBodySetupQueue[AsyncIdx]);
        }

        AsyncBodySetupQueue = NewQueue;
    }
}

void UArcComponent::TestPhysXLinking()
{
    physx::PxCooking* PhysXCooking;

    PxTolerancesScale PScale;
    PScale.length = 1.0;
    PScale.speed = 1.0;

    PxCookingParams PCookingParams(PScale);
    PhysXCooking = PxCreateCooking(PX_PHYSICS_VERSION, *GPhysXFoundation, PCookingParams);
}

#if WITH_EDITOR
void UArcComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    if (!IsTemplate())
    {
        UpdateBodySetup();
    }
    Super::PostEditChangeProperty(PropertyChangedEvent);
}

void FArcCompVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
    const UArcComponent* ArcComp = Cast<const UArcComponent>(Component);
    if (ArcComp != NULL)
    {
        for (auto &Elem : ArcComp->CollisionConvexElems)
        {
            Elem.DrawElemWire(PDI, ArcComp->GetComponentToWorld(), 1.0, FColor::Cyan);
        }
    }
}
#endif