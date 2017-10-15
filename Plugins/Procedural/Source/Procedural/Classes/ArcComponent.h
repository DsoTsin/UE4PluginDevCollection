// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ShapeComponent.h"
#include "PhysicsEngine/ConvexElem.h"
#if WITH_EDITOR
#include "ComponentVisualizer.h"
#endif
#include "ArcComponent.generated.h"


UCLASS( ClassGroup=(Custom), hidecategories = (Object, LOD, Lighting, TextureStreaming, Activation, "Components|Activation"), editinlinenew, meta = (BlueprintSpawnableComponent), showcategories = (Mobility))
class PROCEDURAL_API UArcComponent : public UPrimitiveComponent, public IInterface_CollisionDataProvider
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UArcComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

    virtual void PostLoad() override;
    virtual void OnRegister() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Collision)
    float MinAngle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Collision)
    float MaxAngle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Collision, meta = (ClampMin = "0", UIMin = "0"))
    float Radius;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Collision, meta = (ClampMin = "0", UIMin = "0"))
    float Thickness;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Collision, meta = (ClampMin = "0", UIMin = "0"))
    float Height;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Collision, meta = (ClampMin = "0", UIMin = "0"))
    int32 AngleSlice;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
    bool bUseAsyncCooking;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
    bool bUseComplexAsSimple;

    virtual FPrimitiveSceneProxy*   CreateSceneProxy() override;

    //------------- Physics State ---------------------------//
    FBoxSphereBounds                CalcBounds(const FTransform& LocalToWorld) const override;
    virtual void                    UpdateBodySetup();
    virtual class UBodySetup*       GetBodySetup() override;
    //------------- End Physics -----------------------------//

    //-------------- Collision Data Provider -------------------//
    virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;

    /**	 Interface for checking if the implementing objects contains triangle mesh collision data
    *
    * @return true if the implementing object contains triangle mesh data, false otherwise
    */
    virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;

    /** Do we want to create a negative version of this mesh */
    virtual bool WantsNegXTriMesh() override;

    /** An optional string identifying the mesh data. */
    virtual void GetMeshId(FString& OutMeshId) override;
    //-------------- Collision Data Provider -------------------//
private:

    /** Only show this component if the actor is selected */
    UPROPERTY()
    uint8 bDrawOnlyIfSelected : 1;

    UPROPERTY()
    class UBodySetup* ArcBodySetup;

    /** Convex shapes used for simple collision */
    UPROPERTY(transient)
    TArray<FKConvexElem> CollisionConvexElems;

    /** Queue for async body setups that are being cooked */
    UPROPERTY(transient)
    TArray<UBodySetup*> AsyncBodySetupQueue;
    
    UBodySetup* CreateBodySetup();
    void UpdateCollision();
    void FinishPhysicsAsyncCook(UBodySetup* FinishedBodySetup);

    void TestPhysXLinking();

    friend class FDrawArcSceneProxy;
#if WITH_EDITOR
    friend class FArcCompVisualizer;
#endif
};

#if WITH_EDITOR
class FArcCompVisualizer : public FComponentVisualizer
{
public:
    virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};
#endif