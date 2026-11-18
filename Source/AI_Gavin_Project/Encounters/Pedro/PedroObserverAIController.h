#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "TimerManager.h"
#include "Encounters/Pedro/PedroAwarenessState.h"
#include "Encounters/Pedro/PedroAttentionPolicy.h"
#include "Encounters/Pedro/PedroNavigationPolicy.h"
#include "PedroObserverAIController.generated.h"

class UCharacterMovementComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/** Opt-in peaceful observer. Spatial knowledge, opt-in attention and opt-in locomotion; no evidence. */
UCLASS()
class AI_GAVIN_PROJECT_API APedroObserverAIController : public AAIController
{
    GENERATED_BODY()
public:
    APedroObserverAIController();
    virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;
    virtual void GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;
    virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;

    // Runtime opt-in; requested enablement survives repossession, but the
    // captured anchor and decision never do. No rotation on repeated enable.
    void SetAttentionEnabled(bool bEnabled);
    bool IsAttentionRunning() const;
    const FPedroAttentionDecision& GetAttentionDecision() const { return Attention; }
    const FPedroAttentionSettings& GetAttentionSettings() const { return AttentionSettings; }

    virtual void Tick(float DeltaTime) override;
    void SetNavigationEnabled(bool bEnabled);
    bool IsNavigationRunning() const;
    const FPedroNavigationDecision& GetNavigationDecision() const { return Navigation; }
    const FPedroNavigationSettings& GetNavigationSettings() const { return NavigationSettings; }
    const FPedroNavIntent& GetNavigationIntent() const { return NavIntent; }
    FAIRequestID GetPedroMoveId() const { return OwnedMove; }
    FVector GetPedroMoveGoal() const { return MoveGoal; }
    bool HasValidPedroPath() const;
    double GetPedroPathLength() const;
    uint32 GetPedroMoveSubmissions() const { return MoveSubmissions; }
    const FPedroAwarenessState& GetAwarenessState() const { return Awareness; }
    const UAISenseConfig_Sight* GetSightSettings() const { return SightConfig; }
    bool IsAwarenessRunning() const;
    // Diagnostic: native cached sight, NOT a guarantee of fresh visibility.
    bool HasNativeSight(APawn* Target) const;
    bool GetNativeSightSample(APawn* Target, FVector& OutLocation, float& OutAge) const;
    // Read-only handle for verifying timer removal after destruction.
    FTimerHandle GetAwarenessRefreshTimerHandle() const { return RefreshTimer; }
    static bool IsSupportedPlayer(const AActor* Candidate);

protected:
    virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(VisibleAnywhere, Category="Pedro|Awareness")
    TObjectPtr<UAIPerceptionComponent> ObserverPerception;
    UPROPERTY(VisibleAnywhere, Category="Pedro|Awareness")
    TObjectPtr<UAISenseConfig_Sight> SightConfig;
    FPedroAwarenessState Awareness;
    FTimerHandle RefreshTimer;
    bool bRunning = false;
    bool bAttentionEnabled = false;
    TOptional<double> IdleYaw;
    FPedroAttentionSettings AttentionSettings;
    FPedroAttentionDecision Attention;
    bool bNavigationEnabled=false;
    bool bMovementConfigured=false;
    float SavedSpeed=0, SavedAccel=0, SavedBrake=0;
    bool bSavedAccelerationPaths=false;
    TWeakObjectPtr<UCharacterMovementComponent> ConfiguredMovement;
    FPedroNavigationSettings NavigationSettings;
    FPedroNavigationDecision Navigation;
    FPedroNavIntent NavIntent;
    FAIRequestID OwnedMove=FAIRequestID::InvalidRequest;
    FNavPathSharedPtr OwnedPath;
    FVector MoveGoal=FVector::ZeroVector, StallOrigin=FVector::ZeroVector;
    double MoveStarted=0, StallStarted=0;
    uint32 MoveSubmissions=0;
    TWeakObjectPtr<APawn> RoutePlayer, NavigationPlayer;
    void ConfigureNavigation();
    void ResetNavigation();
    void CancelPedroMove();
    void UpdateNavigation();
    void CaptureAttentionAnchor();
    void ResetAttention();
    void StopAwareness();
    void RefreshAwareness();
    UFUNCTION()
    void HandleSightUpdate(AActor* Actor, FAIStimulus Stimulus);
    bool TryCurrentVisibleLocation(APawn* Target, FVector& OutLocation) const;
};
