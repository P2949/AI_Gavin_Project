#include "Encounters/Pedro/PedroPlayableIntegrationActor.h"

#include "Combat/Attack/MeleeAttackComponent.h"
#include "Encounters/Pedro/PedroEncounterDirector.h"
#include "Encounters/Pedro/PedroObserverAIController.h"
#include "Encounters/Pedro/PedroObserverCharacter.h"
#include "Encounters/Pedro/PedroObserverWatchPoint.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

APedroPlayableIntegrationActor::APedroPlayableIntegrationActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void APedroPlayableIntegrationActor::LogBlocked(const TCHAR* Reason) const
{
    UE_LOG(LogTemp, Warning, TEXT("PEDRO_PLAYABLE status=BLOCKED reason=%s actor=%s"),
        Reason, *GetName());
}

bool APedroPlayableIntegrationActor::ValidateBindings(FString& Failure) const
{
    const UWorld* World = GetWorld();
    if (!World || !IsValid(Observer) || !IsValid(Director))
    {
        Failure = TEXT("MissingWorldOrRequiredActor");
        return false;
    }
    if (Observer->GetWorld() != World || Director->GetWorld() != World ||
        Observer->GetLevel() != GetLevel() || Director->GetLevel() != GetLevel())
    {
        Failure = TEXT("WorldOrLevelMismatch");
        return false;
    }
    if (!Director->bUseDirectObservationAcquisition ||
        Director->bUsePrototypeAssumedAcquisition ||
        Director->DirectObservationActor.Get() != Observer)
    {
        Failure = TEXT("DirectorNotBoundToPhysicalDirectObserver");
        return false;
    }
    if (!Observer->AIControllerClass ||
        !Observer->AIControllerClass->IsChildOf(APedroObserverAIController::StaticClass()) ||
        Observer->AutoPossessAI == EAutoPossessAI::Disabled)
    {
        Failure = TEXT("WrongControllerClassOrAutoPossessDisabled");
        return false;
    }
    if (WatchPoints.Num() > 32)
    {
        Failure = TEXT("TooManyWatchPoints");
        return false;
    }
    TSet<FName> Seen;
    for (const APedroObserverWatchPoint* Point : WatchPoints)
    {
        if (!IsValid(Point) || Point->GetWorld() != World || Point->GetLevel() != GetLevel() ||
            !Point->IsUsable() || Seen.Contains(Point->WatchPointId))
        {
            Failure = TEXT("InvalidOrDuplicateWatchPoint");
            return false;
        }
        Seen.Add(Point->WatchPointId);
    }
    return true;
}

#if WITH_EDITOR
EDataValidationResult APedroPlayableIntegrationActor::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    FString Failure;
    if (!ValidateBindings(Failure))
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("Pedro playable integration invalid: %s"), *Failure)));
        Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
    }
    return Result;
}
#endif

void APedroPlayableIntegrationActor::BeginPlay()
{
    Super::BeginPlay();
    FString Failure;
    if (!ValidateBindings(Failure))
    {
        LogBlocked(*Failure);
        return;
    }
    Observer->ReceiveControllerChangedDelegate.AddUniqueDynamic(
        this, &APedroPlayableIntegrationActor::OnObserverControllerChanged);
    BindPlayer();
    if (!Activate())
        ScheduleStartupRetry();
}

void APedroPlayableIntegrationActor::ScheduleStartupRetry()
{
    if (!GetWorld() || StartupRetries >= MaxStartupRetries || StartupRetryHandle.IsValid())
        return;
    ++StartupRetries;
    StartupRetryHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
        this, &APedroPlayableIntegrationActor::RetryStartup);
}

void APedroPlayableIntegrationActor::RetryStartup()
{
    StartupRetryHandle.Invalidate();
    if (!Activate()) ScheduleStartupRetry();
}

bool APedroPlayableIntegrationActor::Activate()
{
    FString Failure;
    if (!ValidateBindings(Failure))
    {
        LogBlocked(*Failure);
        return false;
    }
    if (Director->HasCommittedContextResolution() || Director->IsEncounterProgressResolved())
    {
        Deactivate();
        LogBlocked(TEXT("EncounterAlreadyResolved"));
        return true; // An intentional inactive terminal state, not a startup race.
    }
    APedroObserverAIController* Controller = Cast<APedroObserverAIController>(Observer->GetController());
    if (!IsValid(Controller) || Controller->GetPawn() != Observer)
    {
        LogBlocked(TEXT("ControllerNotReady"));
        return false;
    }
    if (bActivated && BoundController.Get() == Controller &&
        Controller->IsContextualObservationRunning())
        return true;

    Deactivate();
    BindPlayer();
    if (!Controller->IsAwarenessRunning())
    {
        LogBlocked(TEXT("AwarenessNotReady"));
        return false;
    }
    Controller->SetAttentionEnabled(true);
    if (!Controller->IsAttentionRunning())
    {
        Controller->SetAttentionEnabled(false);
        LogBlocked(TEXT("AttentionUnavailable"));
        return false;
    }
    Controller->SetNavigationEnabled(true);
    if (!Controller->IsNavigationRunning())
    {
        Controller->SetNavigationEnabled(false);
        Controller->SetAttentionEnabled(false);
        LogBlocked(TEXT("NavigationUnavailable"));
        return false;
    }
    TArray<APedroObserverWatchPoint*> Points;
    Points.Reserve(WatchPoints.Num());
    for (APedroObserverWatchPoint* Point : WatchPoints) Points.Add(Point);
    if (!Controller->EnableContextualObservation(Director, Points) ||
        !Controller->IsContextualObservationRunning())
    {
        Controller->DisableContextualObservation();
        Controller->SetNavigationEnabled(false);
        Controller->SetAttentionEnabled(false);
        LogBlocked(TEXT("ContextBindingRejected"));
        return false;
    }
    BoundController = Controller;
    bActivated = true;
    StartupRetries = 0;
    UE_LOG(LogTemp, Display,
        TEXT("PEDRO_PLAYABLE status=ACTIVE observer=%s controller=%s watchpoints=%d"),
        *GetNameSafe(Observer), *GetNameSafe(Controller), WatchPoints.Num());
    return true;
}

void APedroPlayableIntegrationActor::Deactivate()
{
    if (APedroObserverAIController* Controller = BoundController.Get())
    {
        Controller->DisableContextualObservation();
        Controller->SetNavigationEnabled(false);
        Controller->SetAttentionEnabled(false);
    }
    BoundController.Reset();
    bActivated = false;
}

void APedroPlayableIntegrationActor::OnObserverControllerChanged(
    APawn* Pawn, AController* Previous, AController* Current)
{
    if (Pawn != Observer) return;
    if (Previous == Current && bActivated && BoundController.Get() == Current)
        return; // A redundant notification must not erase contextual memory.
    // Pawn changes can fire before the controller has completed OnPossess.
    Deactivate();
    StartupRetries = 0;
    if (Current) ScheduleStartupRetry();
}

void APedroPlayableIntegrationActor::UnbindPlayer()
{
    if (UMeleeAttackComponent* Melee = BoundMelee.Get())
        Melee->OnAttackHit.RemoveDynamic(this, &APedroPlayableIntegrationActor::OnPlayerMeleeHit);
    BoundMelee.Reset();
    BoundPlayer.Reset();
}

void APedroPlayableIntegrationActor::BindPlayer()
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (BoundPlayerController.Get() != PC)
    {
        if (APlayerController* Old = BoundPlayerController.Get())
            Old->OnPossessedPawnChanged.RemoveDynamic(
                this, &APedroPlayableIntegrationActor::OnPlayerPawnChanged);
        BoundPlayerController = PC;
        if (IsValid(PC))
            PC->OnPossessedPawnChanged.AddUniqueDynamic(
                this, &APedroPlayableIntegrationActor::OnPlayerPawnChanged);
    }
    APawn* Player = PC ? PC->GetPawn() : nullptr;
    if (!IsValid(Player) || !Player->IsPlayerControlled() || Player->GetWorld() != World)
        Player = nullptr;
    if (BoundPlayer.Get() == Player &&
        (!Player || BoundMelee.IsValid())) return;
    UnbindPlayer();
    if (!Player) return;
    BoundPlayer = Player;
    UMeleeAttackComponent* Melee = Player->FindComponentByClass<UMeleeAttackComponent>();
    if (IsValid(Melee))
    {
        Melee->OnAttackHit.AddUniqueDynamic(
            this, &APedroPlayableIntegrationActor::OnPlayerMeleeHit);
        BoundMelee = Melee;
    }
    else LogBlocked(TEXT("PlayerHasNoMeleeComponent"));
}

void APedroPlayableIntegrationActor::OnPlayerPawnChanged(APawn* Previous, APawn* Current)
{
    BindPlayer();
}

void APedroPlayableIntegrationActor::OnPlayerMeleeHit(
    AActor* HitActor, const FHitResult& Hit, float AppliedDamage)
{
    // A real successful melee sweep has already occurred in UMeleeAttackComponent.
    // Damage may legitimately be zero for Pedro, so it is never the proof of contact.
    static_cast<void>(AppliedDamage);
    if (!bActivated || !IsValid(Director) || !IsValid(Observer) || HitActor != Observer ||
        !Hit.bBlockingHit || Hit.GetActor() != Observer ||
        Director->HasCommittedContextResolution() || Director->IsEncounterProgressResolved()) return;
    const UWorld* World = GetWorld();
    const APlayerController* PC = BoundPlayerController.Get();
    APawn* Player = BoundPlayer.Get();
    if (!World || !IsValid(PC) || !IsValid(Player) || PC->GetPawn() != Player ||
        !Player->IsPlayerControlled() || Player->GetWorld() != World ||
        !BoundMelee.IsValid() || BoundMelee->GetOwner() != Player ||
        Observer->GetWorld() != World) return;
    Director->ReceiveVerifiedObserverMeleeHit(Player, Observer, Hit);
}

void APedroPlayableIntegrationActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(StartupRetryHandle);
    if (IsValid(Observer))
        Observer->ReceiveControllerChangedDelegate.RemoveDynamic(
            this, &APedroPlayableIntegrationActor::OnObserverControllerChanged);
    if (APlayerController* PC = BoundPlayerController.Get())
        PC->OnPossessedPawnChanged.RemoveDynamic(
            this, &APedroPlayableIntegrationActor::OnPlayerPawnChanged);
    UnbindPlayer();
    BoundPlayerController.Reset();
    Deactivate();
    Super::EndPlay(Reason);
}
