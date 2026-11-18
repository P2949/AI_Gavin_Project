#include "Encounters/Pedro/PedroObserverAIController.h"

#include "CollisionQueryParams.h"
#include "Encounters/Pedro/PedroDirectObservation.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

APedroObserverAIController::APedroObserverAIController()
{
    bAttachToPawn = true;
    bStartAILogicOnPossess = false;
    ObserverPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PedroPerception"));
    SetPerceptionComponent(*ObserverPerception);
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("PedroSight"));
    SightConfig->SightRadius = 1500.0f;
    SightConfig->LoseSightRadius = 1800.0f;
    SightConfig->PeripheralVisionAngleDegrees = 75.0f;
    SightConfig->AutoSuccessRangeFromLastSeenLocation = FAISystem::InvalidRange;
    SightConfig->PointOfViewBackwardOffset = 0.0f;
    SightConfig->NearClippingRadius = 0.0f;
    SightConfig->SetMaxAge(5.0f);
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    ObserverPerception->ConfigureSense(*SightConfig);
    ObserverPerception->SetDominantSense(SightConfig->GetSenseImplementation());
}

void APedroObserverAIController::GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const
{
    if (const auto* Observer = Cast<APedroObserverCharacter>(GetPawn()))
        Observer->GetActorEyesViewPoint(Location, Rotation);
    else
        Super::GetActorEyesViewPoint(Location, Rotation);
}

void APedroObserverAIController::GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const
{
    GetActorEyesViewPoint(Location, Rotation);
}

void APedroObserverAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
    // Awareness must not implicitly rotate the physical eye. Later attention
    // policy will own deliberate turning. Do not call Super's facing logic.
    static_cast<void>(DeltaTime);
    static_cast<void>(bUpdatePawn);
}

bool APedroObserverAIController::IsSupportedPlayer(const AActor* Candidate)
{
    const auto* Player = Cast<APawn>(Candidate);
    return IsValid(Player) && Player->IsPlayerControlled();
}

bool APedroObserverAIController::IsAwarenessRunning() const
{
    return bRunning && GetWorld() && GetWorld()->GetTimerManager().TimerExists(RefreshTimer) &&
        !GetWorld()->GetTimerManager().IsTimerPaused(RefreshTimer);
}

void APedroObserverAIController::OnPossess(APawn* InPawn)
{
    StopAwareness();
    Super::OnPossess(InPawn);
    if (!IsValid(Cast<APedroObserverCharacter>(GetPawn())) || !GetWorld()) return;
    ObserverPerception->ForgetAll();
    ObserverPerception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &APedroObserverAIController::HandleSightUpdate);
    bRunning = true;
    ObserverPerception->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
    ObserverPerception->RequestStimuliListenerUpdate();
    GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this,
        &APedroObserverAIController::RefreshAwareness, 0.1f, true);
}

void APedroObserverAIController::StopAwareness()
{
    bRunning = false;
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RefreshTimer);
    Awareness.Reset();
    if (IsValid(ObserverPerception))
    {
        ObserverPerception->OnTargetPerceptionUpdated.RemoveDynamic(this, &APedroObserverAIController::HandleSightUpdate);
        ObserverPerception->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
        ObserverPerception->ForgetAll();
    }
}

void APedroObserverAIController::OnUnPossess()
{
    StopAwareness();
    Super::OnUnPossess();
}

void APedroObserverAIController::EndPlay(const EEndPlayReason::Type Reason)
{
    StopAwareness();
    Super::EndPlay(Reason);
}

bool APedroObserverAIController::HasNativeSight(APawn* Target) const
{
    FVector Location; float Age;
    return GetNativeSightSample(Target,Location,Age);
}

bool APedroObserverAIController::GetNativeSightSample(APawn* Target, FVector& OutLocation, float& OutAge) const
{
    if (!IsValid(Target) || !IsValid(ObserverPerception) ||
        !ObserverPerception->IsSenseEnabled(UAISense_Sight::StaticClass())) return false;
    FActorPerceptionBlueprintInfo Info;
    if (!ObserverPerception->GetActorsPerception(Target, Info)) return false;
    for (const FAIStimulus& Stimulus : Info.LastSensedStimuli)
    {
        if (Stimulus.Type == SightConfig->GetSenseID() && Stimulus.WasSuccessfullySensed() &&
            !Stimulus.IsExpired() && !Stimulus.StimulusLocation.ContainsNaN() &&
            FAISystem::IsValidLocation(Stimulus.StimulusLocation) &&
            FMath::IsFinite(Stimulus.GetAge()) && Stimulus.GetAge() <= SightConfig->GetMaxAge())
        {
            OutLocation = Stimulus.StimulusLocation;
            OutAge = Stimulus.GetAge();
            return true;
        }
    }
    return false;
}

bool APedroObserverAIController::TryCurrentVisibleLocation(APawn* Target, FVector& OutLocation) const
{
    // This is a bounded sensory query, not a movement goal or a memory read.
    // A cached success may be older than the target's current transform.
    // Read the candidate endpoint ONLY inside this query and publish it only
    // if the fresh physical ray AND current view geometry both succeed.
    const auto* Observer = Cast<APedroObserverCharacter>(GetPawn());
    UWorld* World = GetWorld();
    if (!IsValid(Observer) || !IsValid(Target) || !World || Target->GetWorld() != World) return false;
    FVector Eye;
    FRotator Facing;
    Observer->GetActorEyesViewPoint(Eye, Facing);
    const FVector Candidate = Target->GetActorLocation();
    const float Range = Awareness.Status == EPedroAwarenessStatus::Perceived
        ? SightConfig->LoseSightRadius : SightConfig->SightRadius;
    if (!FAISystem::IsValidLocation(Candidate) ||
        PedroDirectObservation::CheckGeometry(Eye, Facing.Vector(), Candidate, Range,
            SightConfig->PeripheralVisionAngleDegrees) != EPedroDirectObservationStatus::Visible) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PedroAwarenessRefresh), false);
    Params.AddIgnoredActor(Observer);
    Params.AddIgnoredActor(this);
    FHitResult Hit;
    if (World->LineTraceSingleByChannel(Hit, Eye, Candidate, ECC_Visibility, Params) &&
        Hit.GetActor() != Target) return false;
    OutLocation = Candidate;
    return true;
}

void APedroObserverAIController::HandleSightUpdate(AActor* Actor, FAIStimulus Stimulus)
{
    if (bRunning && IsSupportedPlayer(Actor) && Stimulus.Type == SightConfig->GetSenseID())
        RefreshAwareness();
}

void APedroObserverAIController::RefreshAwareness()
{
    UWorld* World = GetWorld();
    if (!bRunning) return;
    if (!World || !IsValid(Cast<APedroObserverCharacter>(GetPawn()))) { StopAwareness(); return; }
    // Single-player encounter. Identity only; no transform read here.
    const APlayerController* PC = World->GetFirstPlayerController();
    APawn* Player = PC ? PC->GetPawn() : nullptr;
    if (!IsSupportedPlayer(Player) || Player->GetWorld() != World)
    {
        Awareness.Reset();
        return;
    }
    if (Awareness.bHasLastKnownPlayerLocation && Awareness.ObservedPlayer.Get() != Player)
        Awareness.Reset();
    FVector VisibleLocation;
    if (HasNativeSight(Player) && TryCurrentVisibleLocation(Player, VisibleLocation))
    {
        if (!Awareness.AcceptSighting(Player, VisibleLocation, World->GetTimeSeconds()))
            Awareness.LoseSight();
    }
    else Awareness.LoseSight();
}
