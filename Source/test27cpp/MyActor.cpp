// Fill out your copyright notice in the Description page of Project Settings.
#include "MyActor.h"
#include "Kismet/GameplayStatics.h"
#include <String>
#include <cmath>
// length unit in ue: cm
#define _collisiontime 0.0005f // seconds

// assume that the ball has radius 22.5cm, i.e. mass will be greater 100 times (normal ping-pong ball has radius 2.25cm)

#define _fric_mu 0.5f
#define _inertial 63300.f // g*cm^2, see: https://patents.google.com/patent/CN102494844A
#define _mass 245.f       // g
#define _velocity_ratio_after_collision 0.9f
#define _gravity_accel 980.f

// Sets default values
AMyActor::AMyActor() : angularVelocity(0.0f), velocity(0.0f), canstart(false) {
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    VisualMesh->SetupAttachment(RootComponent);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeVisualAsset(TEXT("/Game/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));

    if (CubeVisualAsset.Succeeded()) {
        VisualMesh->SetStaticMesh(CubeVisualAsset.Object);
        VisualMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f));
    }
}

bool AMyActor::canTouchWallX(const float &deltatime, float &happentime) {
    auto locx = GetActorLocation().X;
    auto r = getradius();

    if (locx <= xboundMin + r || locx >= xboundMax - r) {
        happentime = 0;
        return true;
    }
    const auto &vx = velocity.X;
    auto newlocx = locx + vx * deltatime;

    if (newlocx > xboundMin + r && newlocx < xboundMax - r) return false;
    if (newlocx <= xboundMin + r)
        happentime = (xboundMin + r - locx) / vx; // note: vx is negative here
    else
        happentime = (xboundMax - r - locx) / vx;

    return true;
}

bool AMyActor::canTouchWallY(const float &deltatime, float &happentime) {
    auto locy = GetActorLocation().Y;
    auto r = getradius();

    if (locy <= yboundMin + r || locy >= yboundMax - r) {
        happentime = 0;
        return true;
    }
    const auto &vy = velocity.Y;
    auto newlocy = locy + vy * deltatime;

    if (newlocy > yboundMin + r && newlocy < yboundMax - r) return false;
    if (newlocy <= yboundMin + r)
        happentime = (yboundMin + r - locy) / vy; // note: vy is negative here
    else
        happentime = (yboundMax - r - locy) / vy;

    return true;
}

bool AMyActor::canTouchGround(const float &deltatime, float &happentime) {
    auto locz = GetActorLocation().Z;
    auto r = getradius();
    if (locz <= zbound) {
        happentime = 0;
        return true;
    }

    const auto &vz = velocity.Z;
    if (vz >= 0) return false;


    auto newlocz = locz + vz * deltatime;
    if (newlocz > zbound) return false;
    happentime = (zbound - locz) / vz; // note: vz is negative here
    return true;
}


void AMyActor::touchGround(FVector &position, const float &happentime) {
    float r = getradius();                   // Assume scales from all directions are the same
    if (velocity.Z * velocity.Z < 12100.f) { // velocity on z axis too small, stop
        velocity.Z = 0.f;
        velocity *= _velocity_ratio_after_collision;
        position.Z = zbound;
        angularVelocity *= _velocity_ratio_after_collision;
        GEngine->AddOnScreenDebugMessage(0, 10.0f, FColor::Red, "0, Velocity: " + velocity.ToCompactString());
        //GEngine->AddOnScreenDebugMessage(0, 1.0f, FColor::Red, "Position: " + position.ToCompactString());
        return;
    }

    // modify position first, then velocity
    position += happentime * velocity;
    if (position.Z < zbound) position.Z = zbound;

    // if velocity is positive, do nothing
    if (velocity.Z > 0) return;

    // modify velocity and angular velocity
    // first modify z direction to positive
    velocity.Z = -velocity.Z;
    // calculate the relative velocity to hit point
    auto touch_velocity = getTouchVelocity({0, 0, r});
    auto touch_velocity_direction = touch_velocity.GetSafeNormal();
    if (touch_velocity_direction.IsNearlyZero()) { // no friction
        velocity *= _velocity_ratio_after_collision;
        angularVelocity *= _velocity_ratio_after_collision;
        return;
    }

    float touch_v_norm = std::sqrt(FVector::DotProduct(touch_velocity, touch_velocity));
    // define v_center_coef = 2* mu * vz
    float v_center_coef = 2 * _fric_mu * velocity.Z;

    // Note: the friction does not always exist!
    // Check if the friction always exists during hit. Renew v_center_coef
    v_center_coef = std::min(v_center_coef, touch_v_norm * _inertial / (_inertial + r * r * _mass)); // friction does not exist during hit, renew it

    // friction is of inverse direction to touch_velocity
    // Delta v = 2 * mu * v_z
    velocity -= v_center_coef * touch_velocity_direction;

    // Delta w = 2 * mu * r * vz * (mass / inertia) = v_center_coef * r * (mass / inertia)
    // direction: fric cross normal, i.e. normal cross touch_velocity_direction
    // Note: in UE4 the axis is left-handed, cross product is the other direction of right-handed case
    auto dir = -FVector::CrossProduct({0, 0, 1.f}, touch_velocity_direction);
    angularVelocity += v_center_coef * r * (_mass / _inertial) * dir;

    // velocity decreases after collision
    velocity *= _velocity_ratio_after_collision;
    angularVelocity *= _velocity_ratio_after_collision;

    GEngine->AddOnScreenDebugMessage(0, 10.0f, FColor::Red, "total touch v: " + getTouchVelocity({0, 0, r}).ToCompactString());
    // GEngine->AddOnScreenDebugMessage(0, 10.0f, FColor::Red, "2, Velocity: " + velocity.ToCompactString(), false);
    //GEngine->AddOnScreenDebugMessage(0, 1.0f, FColor::Red, "Position: " + position.ToCompactString());
}

void AMyActor::touchWallX(FVector &position, const float &happentime) {
    // first determine which direction to hit
    position += velocity * happentime;
    float r = getradius(); // Assume scales from all directions are the same
    bool hitmax = std::abs(xboundMax - r - position.X) < std::abs(xboundMin + r - position.X);
    position.X = hitmax ? xboundMax - r : xboundMin + r;
    if (velocity.X * velocity.X < 25.f) { // velocity on z axis too small, stop
        velocity.X = 0.f;
        velocity *= _velocity_ratio_after_collision;
        angularVelocity *= _velocity_ratio_after_collision;
        return;
    }

    // if velocity is not the direction, do nothing
    if ((hitmax && velocity.X < 0) || (!hitmax && velocity.X > 0)) return;

    // modify velocity and angular velocity
    // first modify z direction
    velocity.X = -velocity.X;

    auto touch_velocity = getTouchVelocity({hitmax ? -r : r, 0, 0});
    auto touch_velocity_direction = touch_velocity.GetSafeNormal();
    if (touch_velocity_direction.IsNearlyZero()) {
        velocity *= _velocity_ratio_after_collision;
        angularVelocity *= _velocity_ratio_after_collision;
        return;
    }

    float touch_v_norm = std::sqrt(FVector::DotProduct(touch_velocity, touch_velocity));
    // define v_center_coef = 2* mu * vz
    float v_center_coef = 2 * _fric_mu * std::abs(velocity.X);

    // Note: the friction does not always exist!
    // Check if the friction always exists during hit. Renew v_center_coef
    v_center_coef = std::min(v_center_coef, touch_v_norm); // friction does not exist during hit, renew it

    // friction is inverse to touch_velocity
    // Delta v = 2 * mu * v_z
    velocity -= v_center_coef * touch_velocity_direction;

    // Delta w = 2 * mu * r * vz * (mass / inertia)
    // direction: fric cross normal, i.e. normal cross touch_velocity_direction
    angularVelocity += -v_center_coef * r * (_mass / _inertial) * FVector::CrossProduct({hitmax ? -1.f : 1.f, 0, 0}, touch_velocity_direction);

    // velocity decreases after collision
    velocity *= _velocity_ratio_after_collision;
    angularVelocity *= _velocity_ratio_after_collision;
}

void AMyActor::touchWallY(FVector &position, const float &happentime) {
    // first determine which direction to hit
    position += velocity * happentime;
    float r = getradius(); // Assume scales from all directions are the same
    bool hitmax = std::abs(yboundMax - r - position.Y) < std::abs(yboundMin + r - position.Y);
    position.Y = hitmax ? yboundMax - r : yboundMin + r;

    //if (velocity.Y * velocity.Y < 25.f) { // velocity on z axis too small, stop
    //    velocity.Y = 0.f;
    //    velocity *= _velocity_ratio_after_collision;
    //    angularVelocity *= _velocity_ratio_after_collision;
    //    return;
    //}

    // if velocity is not the direction, do nothing
    if ((hitmax && velocity.Y < 0) || (!hitmax && velocity.Y > 0)) return;

    // modify velocity and angular velocity
    // first modify z direction
    velocity.Y = -velocity.Y;

    auto touch_velocity = getTouchVelocity({0, hitmax ? -r : r, 0});
    auto touch_velocity_direction = touch_velocity.GetSafeNormal();
    if (touch_velocity_direction.IsNearlyZero()) {
        velocity *= _velocity_ratio_after_collision;
        angularVelocity *= _velocity_ratio_after_collision;
        return;
    }

    float touch_v_norm = std::sqrt(FVector::DotProduct(touch_velocity, touch_velocity));
    // define v_center_coef = 2* mu * vz
    float v_center_coef = 2 * _fric_mu * std::abs(velocity.Y);

    // Note: the friction does not always exist!
    // Check if the friction always exists during hit. Renew v_center_coef
    v_center_coef = std::min(v_center_coef, touch_v_norm); // friction does not exist during hit, renew it

    // friction is inverse to touch_velocity
    // Delta v = 2 * mu * v_z
    velocity -= v_center_coef * touch_velocity_direction;

    // Delta w = 2 * mu * r * vz * (mass / inertia)
    // direction: fric cross normal, i.e. normal cross touch_velocity_direction
    angularVelocity += -v_center_coef * r * (_mass / _inertial) * FVector::CrossProduct({0, hitmax ? -1.f : 1.f, 0}, touch_velocity_direction);

    // velocity decreases after collision
    velocity *= _velocity_ratio_after_collision;
    angularVelocity *= _velocity_ratio_after_collision;
}

void AMyActor::recursiveRenewPosition(float &DeltaTime) {
    if (DeltaTime <= 0) return;
    if (isOnGround()) {
        if (velocity.IsNearlyZero() && angularVelocity.IsNearlyZero()) {
            canstart = false;
            return;
        }
        roll(DeltaTime);
        return;
    }

    // consider hit situation

    TOUCHTYPE flag = NOTOUCH;
    float happentime = DeltaTime;
    float dum;

    if (canTouchGround(DeltaTime, dum)) {
        flag = TOUCHGROUND;
        happentime = dum;
    }
    if (canTouchWallX(DeltaTime, dum) && dum < happentime) {
        flag = TOUCHWALLX;
        happentime = dum;
    }
    if (canTouchWallY(DeltaTime, dum) && dum < happentime) {
        flag = TOUCHWALLY;
        happentime = dum;
    }

    FVector NewLocation = GetActorLocation();
    if (flag == NOTOUCH) {
        // 抛物线
        freedown(NewLocation, DeltaTime);
        SetActorLocation(NewLocation);
        return;
    }

    if (flag == TOUCHGROUND) {
        GEngine->AddOnScreenDebugMessage(0, 1.0f, FColor::Red, TEXT("HIT GROUND"));
        touchGround(NewLocation, happentime);
        freedown(NewLocation, DeltaTime - happentime);
        SetActorLocation(NewLocation);
        return;
    }

    else if (flag == TOUCHWALLX) {
        GEngine->AddOnScreenDebugMessage(0, 1.0f, FColor::Red, TEXT("HIT X"));

        touchWallX(NewLocation, happentime);
        freedown(NewLocation, DeltaTime - happentime);
        SetActorLocation(NewLocation);
        return;
    }

    else if (flag == TOUCHWALLY) {
        GEngine->AddOnScreenDebugMessage(0, 1.0f, FColor::Red, TEXT("HIT Y"));

        touchWallY(NewLocation, happentime);
        freedown(NewLocation, DeltaTime - happentime);
        SetActorLocation(NewLocation);
        return;
    }
}


void AMyActor::roll(float &DeltaTime, bool pureroll) {
    velocity.Z = 0.f;

    auto r = getradius();
    auto vline = getTouchVelocity({0, 0, r});
    if (pureroll || vline.IsNearlyZero(0.01f)) {
        
        auto diff = -r * _fric_mu * _gravity_accel * (_mass / _inertial) * DeltaTime * FVector::CrossProduct({0, 0, 1.f}, vline).GetSafeNormal();
        ;
        if (FVector::DotProduct(angularVelocity, angularVelocity + diff) < 0)
            angularVelocity = FVector(0.f);
        else
            angularVelocity += diff;
        velocity = -getTouchVelocityOfAngular({0, 0, r});
        return;
    }
    auto deltav = _gravity_accel * _fric_mu * DeltaTime;
    auto normalsquarevline = FVector::DotProduct(vline, vline);
    if (normalsquarevline < deltav * deltav) {
        auto newdelta = normalsquarevline * DeltaTime / (deltav * deltav);
        DeltaTime -= newdelta;
        angularVelocity += -r * _fric_mu * _gravity_accel * (_mass / _inertial) * newdelta * FVector::CrossProduct({0, 0, 1.f}, vline).GetSafeNormal();
        velocity = -getTouchVelocityOfAngular({0, 0, r});
        roll(DeltaTime, true);
        return;
    }
    angularVelocity += -r * _fric_mu * _gravity_accel * (_mass / _inertial) * DeltaTime * FVector::CrossProduct({0, 0, 1.f}, vline).GetSafeNormal();
    velocity -= _fric_mu * _gravity_accel * DeltaTime * vline.GetSafeNormal();
}


// Called when the game starts or when spawned
void AMyActor::BeginPlay() {
    Super::BeginPlay();
}


// Called every frame
void AMyActor::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    const float TimeBetweenCameraChanges = 2.0f;
    const float SmoothBlendTime = 0.75f;

    TimeToNextCameraChange -= DeltaTime;

    if (TimeToNextCameraChange <= 0.0f) {
        TimeToNextCameraChange += TimeBetweenCameraChanges;

        // 查找处理本地玩家控制的actor。
        APlayerController *OurPlayerController = UGameplayStatics::GetPlayerController(this, 0);
        if (OurPlayerController) {
            //if ((OurPlayerController->GetViewTarget() != CameraOne) && (CameraOne != nullptr)) {
            //    // 立即切换到摄像机1。
            //    OurPlayerController->SetViewTarget(CameraOne);
            //} else
            if ((OurPlayerController->GetViewTarget() != CameraTwo) && (CameraTwo != nullptr)) {
                // 平滑地混合到摄像机2。
                OurPlayerController->SetViewTargetWithBlend(CameraTwo, SmoothBlendTime);
            }
        }
    }
    if (!canstart) return;
    recursiveRenewPosition(DeltaTime);
    //FRotator NewRotation = GetActorRotation();
    //float RunningTime = GetGameTimeSinceCreation();
    //float DeltaHeight = (FMath::Sin(RunningTime + DeltaTime) - FMath::Sin(RunningTime));
    //NewLocation.Z += DeltaHeight * 20.0f;    //Scale our height by a factor of 20
    //float DeltaRotation = DeltaTime * 20.0f; //Rotate by 20 degrees per second
    //NewRotation.Yaw += DeltaRotation;
    //SetActorLocationAndRotation(NewLocation, NewRotation);
}
