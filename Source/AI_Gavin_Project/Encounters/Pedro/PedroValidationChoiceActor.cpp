#include "Encounters/Pedro/PedroValidationChoiceActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Encounters/Pedro/PedroObservationSourceComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

APedroValidationChoiceActor::APedroValidationChoiceActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChoiceCube"));
    Mesh->SetupAttachment(SceneRoot);
    Mesh->SetRelativeScale3D(FVector(0.65f, 0.65f, 1.0f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Mesh->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        Mesh->SetStaticMesh(Cube.Object);
    }

    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ChoiceLabel"));
    Label->SetupAttachment(SceneRoot);
    Label->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
    Label->SetWorldSize(26.f);
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ObservationSource =
        CreateDefaultSubobject<UPedroObservationSourceComponent>(TEXT("ObservationSource"));
}

bool APedroValidationChoiceActor::IsPermittedWorld() const
{
    const UWorld* World = GetWorld();
    return World && World->WorldType == EWorldType::PIE &&
           World->GetMapName().EndsWith(TEXT("L_Pedro_ChoiceTest"));
}

void APedroValidationChoiceActor::ConfigureSource()
{
    if (!IsValid(ObservationSource)) return;

    const bool bValidAction = ActionKind == EPedroEvidenceKind::HelpedNPC ||
                              ActionKind == EPedroEvidenceKind::AttackedNPC;

    ObservationSource->bEvidenceEnabled =
        bValidAction && !OpportunityId.IsNone();
    ObservationSource->OpportunityId = OpportunityId;
    const TCHAR* Prefix = ActionKind == EPedroEvidenceKind::HelpedNPC
        ? TEXT("SimHelp") : TEXT("SimAttack");
    ObservationSource->SubjectId = FName(*FString::Printf(
        TEXT("%s_%s"), Prefix, *OpportunityId.ToString()));
}

void APedroValidationChoiceActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ConfigureSource();
    const bool bHelp = ActionKind == EPedroEvidenceKind::HelpedNPC;
    if (IsValid(Label))
    {
        Label->SetText(FText::FromString(FString::Printf(
            TEXT("SIM %s: %s"), bHelp ? TEXT("HELP") : TEXT("ATTACK"),
            *OpportunityId.ToString())));
        Label->SetTextRenderColor(bHelp ? FColor::Green : FColor::Red);
    }
}

void APedroValidationChoiceActor::BeginPlay()
{
    Super::BeginPlay();
    bAlreadyUsed = false;
    ConfigureSource();

    if (!IsPermittedWorld())
    {
        if (IsValid(ObservationSource))
        {
            ObservationSource->bEvidenceEnabled = false;
        }
        if (IsValid(Mesh))
        {
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        UE_LOG(LogTemp, Error,
            TEXT("PEDRO_CHOICE_TEST blocked=OutsideValidationMap actor=%s"),
            *GetName());
    }
}

void APedroValidationChoiceActor::Interact_Implementation(AActor* Interactor)
{
    const APawn* PlayerPawn = Cast<APawn>(Interactor);
    if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled() ||
        bAlreadyUsed || !IsPermittedWorld() ||
        !IsValid(ObservationSource) || !ObservationSource->bEvidenceEnabled)
    {
        return;
    }

    // Deliberate test fixture: these cube clicks SIMULATE successful actions;
    // they do not claim that an NPC was truly helped/attacked, nor seen by Pedro.
    const bool bExposed = ObservationSource->ReportOpportunityExposed();
    const bool bEmitted =
        bExposed && ObservationSource->ReportConfirmedAction(ActionKind);

    UE_LOG(LogTemp, Display,
        TEXT("PEDRO_CHOICE_TEST action=%s emitted=%s opportunity=%s actualLOS=no"),
        ActionKind == EPedroEvidenceKind::HelpedNPC
            ? TEXT("Gentle") : TEXT("Disruptive"),
        bEmitted ? TEXT("yes") : TEXT("no"),
        *OpportunityId.ToString());

    if (bEmitted)
    {
        bAlreadyUsed = true;
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
    }
}
