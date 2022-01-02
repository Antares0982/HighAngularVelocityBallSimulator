// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyActor.generated.h"

#define _radius 50.f // the default radius of material

UCLASS()
class TEST27CPP_API AMyActor : public AActor {
    GENERATED_BODY()

private:
    UPROPERTY(EditAnywhere)
    float yboundMin = -1474.0f;

    UPROPERTY(EditAnywhere)
    float yboundMax = 1475.0f;

    UPROPERTY(EditAnywhere)
    float xboundMin = -1874.0f;

    UPROPERTY(EditAnywhere)
    float xboundMax = 1075.0f;

    UPROPERTY(EditAnywhere)
    float zbound = 130.f;


    UPROPERTY(EditAnywhere)
    AActor *CameraOne;

    UPROPERTY(EditAnywhere)
    AActor *CameraTwo;


    UPROPERTY()
    float TimeToNextCameraChange;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector angularVelocity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector velocity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool canstart;


private:
    enum TOUCHTYPE {
        NOTOUCH,
        TOUCHGROUND,
        TOUCHWALLX,
        TOUCHWALLY
    };


private:
    bool canTouchWallX(const float &, float &);
    bool canTouchWallY(const float &, float &);
    bool canTouchGround(const float &deltatime, float &happentime);

    void touchGround(FVector &position, const float &happentime);
    void touchWallX(FVector &position, const float &happentime);
    void touchWallY(FVector &position, const float &happentime);

    void recursiveRenewPosition(float &DeltaTime);

    void freedown(FVector &Location, const float &DeltaTime) {
        if (DeltaTime <= 0) return;
        Location += DeltaTime * velocity;
        velocity += DeltaTime * FVector(0, 0, -980.f);
    }

    bool isOnGround() { return velocity.IsZero() && GetActorLocation().Z <= zbound + getradius(); }

    void roll(FVector &, float &DeltaTime, bool pureroll = false);

    float getradius() { return GetActorScale().X * _radius; }

    FVector getTouchVelocityOfAngular(const FVector &toCentralVec) {
        auto ans = FVector::CrossProduct(angularVelocity, toCentralVec);
        // GEngine->AddOnScreenDebugMessage(0, 10.0f, FColor::Red, "1, angular v line: " + ans.ToCompactString(), false);
        return ans;
    }

    FVector getTouchVelocity(const FVector &toCentralVec) {
        auto ans = velocity;
        auto normal = toCentralVec.GetUnsafeNormal();
        ans -= FVector::DotProduct(ans, normal) * normal;

        return ans + getTouchVelocityOfAngular(toCentralVec);
    }

public:
    // Sets default values for this actor's properties
    AMyActor();
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    UStaticMeshComponent *VisualMesh;

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;
};
