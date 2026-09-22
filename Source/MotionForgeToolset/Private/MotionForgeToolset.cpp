#include "MotionForgeToolset.h"

#include "MotionForgeAsyncResult.h"
#include "MotionForgeToolsetModule.h"
#include "MotionForgeSubsystem.h"
#include "MotionForgeSettings.h"
#include "IMotionProvider.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"
#include "Interfaces/IPluginManager.h"

namespace MotionForgeToolsetPrivate
{
	/** How often to check a running batch. Generation takes tens of seconds at best. */
	static constexpr float PollSeconds = 3.0f;

	/** Give up eventually rather than ticking forever on a batch that will never settle. */
	static constexpr double MaxWatchSeconds = 30.0 * 60.0;

	/**
	 * Fail the tool call.
	 *
	 * The registry watches for script exceptions while a tool runs and turns them into an error the
	 * agent sees, so this is how a tool says no. Say what went wrong and what to call instead -
	 * models are good at recovering from a mistake and bad at guessing they made one.
	 */
	static void Fail(const FString& Message)
	{
		UKismetSystemLibrary::RaiseScriptError(Message);
	}

	static UMotionForgeSubsystem* Subsystem()
	{
		UMotionForgeSubsystem* Forge = UMotionForgeSubsystem::Get();
		if (!Forge)
		{
			Fail(TEXT("MotionForge is not available. The plugin is disabled, or this is not an editor "
					  "session."));
		}
		return Forge;
	}
}

// -------------------------------------------------------------------------------------------------
// Discovery
// -------------------------------------------------------------------------------------------------

TArray<FString> UMotionForgeToolset::ListMotionDefinitions(const TArray<EMotionDefStatus>& StatusFilter)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->FindMotionDefs(StatusFilter) : TArray<FString>();
}

FMotionOutputPaths UMotionForgeToolset::GetMotionOutputPaths()
{
	return UMotionForgeSettings::Get()->GetOutputPaths();
}

FMotionOutputPaths UMotionForgeToolset::SetMotionOutputRoot(const FString& ContentPath)
{
	const FMotionOutputPaths Paths = UMotionForgeSettings::SetOutputRoot(ContentPath);

	if (!Paths.Problem.IsEmpty())
	{
		// A refused root is a mistake the agent can correct, so raise it rather than returning a
		// struct whose Problem field it may not read.
		MotionForgeToolsetPrivate::Fail(Paths.Problem);
	}

	return Paths;
}

TArray<FMotionProvenanceRow> UMotionForgeToolset::ReportMotionProvenance()
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->ReportProvenance() : TArray<FMotionProvenanceRow>();
}


TArray<FMotionDefinitionStatus> UMotionForgeToolset::GetMotionStatus(const TArray<FString>& AssetPaths)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->GetStatus(AssetPaths) : TArray<FMotionDefinitionStatus>();
}

FMotionReadiness UMotionForgeToolset::CheckMotionReadiness(const FString& AssetPath)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->CheckReadiness(AssetPath) : FMotionReadiness();
}

void UMotionForgeToolset::RefreshProviderState(FName ProviderId)
{
	if (UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem())
	{
		Forge->RefreshProviderState(ProviderId);
	}
}

FMotionBatchStatus UMotionForgeToolset::GetBatchStatus(const FString& BatchId)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->GetBatchStatus(BatchId) : FMotionBatchStatus();
}

TArray<FName> UMotionForgeToolset::ListProviders()
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->GetProviderIds() : TArray<FName>();
}

FMotionProviderCaps UMotionForgeToolset::GetProviderCapabilities(FName ProviderId)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return FMotionProviderCaps();
	}

	const FMotionProviderCaps Caps = Forge->GetProviderCaps(ProviderId);

	// An empty ProviderId on the way out means the one asked for does not exist. Saying so beats
	// handing back defaults that read like a real answer.
	if (Caps.ProviderId.IsNone())
	{
		MotionForgeToolsetPrivate::Fail(FString::Printf(
			TEXT("No provider named '%s'. Call List Providers for the valid names."),
			*ProviderId.ToString()));
	}

	return Caps;
}

FMotionCredentialInfo UMotionForgeToolset::GetCredentialStatus(FName ProviderId)
{
	FMotionCredentialInfo Info;

	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return Info;
	}

	// An empty name means "the one you would use anyway" - the project's default, resolved the one way
	// everything else resolves it, not the first provider alphabetically.
	const FName Provider = ProviderId;

	if (!Forge->GetCredentialInfo(Provider, Info))
	{
		MotionForgeToolsetPrivate::Fail(FString::Printf(
			TEXT("No provider named '%s'. Call List Providers for the valid names."),
			*Provider.ToString()));
	}

	return Info;
}

UToolCallAsyncResultString* UMotionForgeToolset::TestProviderConnection(FName ProviderId)
{
	UToolCallAsyncResultString* Result = NewObject<UToolCallAsyncResultString>();

	// Rooted for the same reason as the batch watchers: only the completion lambda refers to it, and
	// the collector cannot see that. The provider always invokes the callback, including on its own
	// synchronous failure paths, so every route through here unroots.
	Result->AddToRoot();

	UMotionForgeSubsystem* Forge = UMotionForgeSubsystem::Get();
	if (!Forge)
	{
		Result->SetError(TEXT("MotionForge is not available."));
		Result->RemoveFromRoot();
		return Result;
	}

	const FName Provider = ProviderId;

	TSharedPtr<IMotionProvider> Impl = Forge->FindProvider(Provider);
	if (!Impl.IsValid())
	{
		Result->SetError(FString::Printf(
			TEXT("No provider named '%s'. Call List Providers for the valid names."), *Provider.ToString()));
		Result->RemoveFromRoot();
		return Result;
	}

	UE_LOG(LogMotionForgeToolset, Log, TEXT("Tool: TestProviderConnection on '%s'"), *Provider.ToString());

	Impl->TestConnection(
		[Result](bool bSuccess, const FString& Message)
		{
			if (bSuccess)
			{
				Result->SetValue(Message);
			}
			else
			{
				Result->SetError(Message);
			}
			Result->RemoveFromRoot();
		});

	return Result;
}

// -------------------------------------------------------------------------------------------------
// Characters
// -------------------------------------------------------------------------------------------------

UToolCallAsyncResultRemoteCharacters* UMotionForgeToolset::ListProviderCharacters(FName ProviderId)
{
	UToolCallAsyncResultRemoteCharacters* Result = NewObject<UToolCallAsyncResultRemoteCharacters>();
	Result->AddToRoot();

	UMotionForgeSubsystem* Forge = UMotionForgeSubsystem::Get();
	if (!Forge)
	{
		Result->SetError(TEXT("MotionForge is not available."));
		Result->RemoveFromRoot();
		return Result;
	}

	Forge->ListProviderCharacters(ProviderId,
		[Result](bool bSuccess, const TArray<FMotionRemoteCharacter>& Characters, const FString& Error)
		{
			if (bSuccess)
			{
				Result->SetValue(Characters);
			}
			else
			{
				Result->SetError(Error);
			}
			Result->RemoveFromRoot();
		});

	return Result;
}

UToolCallAsyncResultCharacterUpload* UMotionForgeToolset::UploadCharacterToProvider(
	const FString& CharacterAssetPath,
	const FMotionCharacterUploadOptions& Options,
	bool Force)
{
	UToolCallAsyncResultCharacterUpload* Result = NewObject<UToolCallAsyncResultCharacterUpload>();
	Result->AddToRoot();

	UMotionForgeSubsystem* Forge = UMotionForgeSubsystem::Get();
	if (!Forge)
	{
		Result->SetError(TEXT("MotionForge is not available."));
		Result->RemoveFromRoot();
		return Result;
	}

	UE_LOG(LogMotionForgeToolset, Log, TEXT("Tool: UploadCharacterToProvider on '%s'%s"),
		*CharacterAssetPath, Force ? TEXT(" (forced)") : TEXT(""));

	Forge->UploadCharacter(CharacterAssetPath, Options, Force,
		[Result](bool bSuccess, const FMotionCharacterUpload& Upload, const FString& Error)
		{
			if (bSuccess)
			{
				Result->SetValue(Upload);
			}
			else
			{
				Result->SetError(Error);
			}
			Result->RemoveFromRoot();
		});

	return Result;
}

UToolCallAsyncResultString* UMotionForgeToolset::ImportProviderCharacterRig(const FString& CharacterAssetPath)
{
	UToolCallAsyncResultString* Result = NewObject<UToolCallAsyncResultString>();
	Result->AddToRoot();

	UMotionForgeSubsystem* Forge = UMotionForgeSubsystem::Get();
	if (!Forge)
	{
		Result->SetError(TEXT("MotionForge is not available."));
		Result->RemoveFromRoot();
		return Result;
	}

	UE_LOG(LogMotionForgeToolset, Log, TEXT("Tool: ImportProviderCharacterRig on '%s'"), *CharacterAssetPath);

	Forge->ImportProviderCharacter(CharacterAssetPath,
		[Result](bool bSuccess, const FString& MeshPath, const FString& Error)
		{
			if (bSuccess)
			{
				Result->SetValue(MeshPath);
			}
			else
			{
				Result->SetError(Error);
			}
			Result->RemoveFromRoot();
		});

	return Result;
}

// -------------------------------------------------------------------------------------------------
// Cost
// -------------------------------------------------------------------------------------------------

FMotionCostEstimate UMotionForgeToolset::EstimateDownloadCost(
	const TArray<FString>& AssetPaths, bool SelectedOnly)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->EstimateCost(AssetPaths, SelectedOnly) : FMotionCostEstimate();
}

FMotionCostEstimate UMotionForgeToolset::EstimateGenerationCost(const TArray<FMotionDefSpec>& Definitions)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return FMotionCostEstimate();
	}

	if (Definitions.Num() == 0)
	{
		MotionForgeToolsetPrivate::Fail(
			TEXT("No definitions were given, so there is nothing to price. Pass the same payload you "
				 "intend to give Create And Generate Motions."));
		return FMotionCostEstimate();
	}

	return Forge->EstimateGenerationCost(Definitions);
}

// -------------------------------------------------------------------------------------------------
// Authoring
// -------------------------------------------------------------------------------------------------

FMotionBatchSubmission UMotionForgeToolset::CreateAndGenerateMotions(
	const TArray<FMotionDefSpec>& Definitions, EMotionPipelineMode Mode)
{
	FMotionBatchSubmission Result;

	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return Result;
	}

	if (Definitions.Num() == 0)
	{
		MotionForgeToolsetPrivate::Fail(TEXT("No definitions were given. Each one needs at least a prompt."));
		return Result;
	}

	for (const FMotionDefSpec& Spec : Definitions)
	{
		if (Spec.Prompt.IsEmpty())
		{
			MotionForgeToolsetPrivate::Fail(FString::Printf(
				TEXT("Definition '%s' has no prompt. A prompt is the only field with no useful default."),
				*Spec.AssetName));
			return Result;
		}
	}

	UE_LOG(LogMotionForgeToolset, Log, TEXT("Tool: CreateAndGenerateMotions on %d definition(s)"),
		Definitions.Num());

	Result = Forge->SubmitBatch(Definitions, Mode);

	if (Result.AssetPaths.Num() == 0)
	{
		MotionForgeToolsetPrivate::Fail(TEXT("No definitions could be created. Check the Output Log for "
										 "the asset names that were rejected."));
	}

	return Result;
}

void UMotionForgeToolset::UpdateMotionDefinition(const FString& AssetPath, const FMotionDefSpec& Definition)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return;
	}

	TArray<FString> Problems;
	if (!Forge->UpdateMotionDefChecked(AssetPath, Definition, Problems))
	{
		MotionForgeToolsetPrivate::Fail(FString::Printf(
			TEXT("No motion definition at '%s'. Call List Motion Definitions for the valid paths."),
			*AssetPath));
		return;
	}

	// Everything else in the spec was applied; say exactly what was not, so it can be corrected.
	if (Problems.Num() > 0)
	{
		MotionForgeToolsetPrivate::Fail(FString::Printf(
			TEXT("Updated, except: %s Call List Pipeline Options for the valid keys and values."),
			*FString::Join(Problems, TEXT(" "))));
	}
}

FString UMotionForgeToolset::CreateMotionDefinition(const FMotionDefSpec& Definition)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return FString();
	}

	TArray<FString> Problems;
	const FString Path = Forge->CreateMotionDefChecked(Definition, Problems);

	if (Path.IsEmpty())
	{
		MotionForgeToolsetPrivate::Fail(TEXT("The definition could not be created. The Output Log says why."));
		return Path;
	}

	if (Problems.Num() > 0)
	{
		MotionForgeToolsetPrivate::Fail(FString::Printf(
			TEXT("Created %s, except: %s Call List Pipeline Options for the valid keys and values."),
			*Path, *FString::Join(Problems, TEXT(" "))));
	}

	return Path;
}

FMotionResolvedRequest UMotionForgeToolset::PreviewMotionRequest(const FString& AssetPath)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->ResolveRequest(AssetPath) : FMotionResolvedRequest();
}

FString UMotionForgeToolset::SetMotionProvider(const FString& AssetPath, FName ProviderId)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return FString();
	}

	if (!ProviderId.IsNone() && !Forge->FindProvider(ProviderId).IsValid())
	{
		MotionForgeToolsetPrivate::Fail(FString::Printf(
			TEXT("No provider named '%s'. Call List Providers for the valid names."), *ProviderId.ToString()));
		return FString();
	}

	return Forge->SetDefinitionProvider(AssetPath, ProviderId);
}

TArray<FMotionPipelineOption> UMotionForgeToolset::ListPipelineOptions(const FString& AssetPath, FName ProviderId)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->GetPipelineOptions(AssetPath, ProviderId) : TArray<FMotionPipelineOption>();
}

void UMotionForgeToolset::SetMotionPipelineOption(const FString& AssetPath, const FString& Key, const FString& Value)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return;
	}

	FString Error;
	if (!Forge->SetPipelineOption(AssetPath, Key, Value, Error))
	{
		MotionForgeToolsetPrivate::Fail(Error);
	}
}

TArray<FString> UMotionForgeToolset::FindCharactersForProvider(FName ProviderId)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->FindCharactersFor(ProviderId) : TArray<FString>();
}

TArray<FMotionSetupStepInfo> UMotionForgeToolset::GetProviderSetupSteps(FName ProviderId)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->GetSetupSteps(ProviderId) : TArray<FMotionSetupStepInfo>();
}

void UMotionForgeToolset::OpenMotionForgeWindow(bool GetStarted)
{
	// Through the editor module's own console command, so this module needs no editor-UI dependency
	// and the page is chosen the one way the menu chooses it.
	const TCHAR* Command = GetStarted ? TEXT("MotionForge.GetStarted") : TEXT("MotionForge.Library");

	if (!IConsoleManager::Get().FindConsoleObject(Command) || !GEngine)
	{
		MotionForgeToolsetPrivate::Fail(TEXT("The MotionForge window is not available: MotionForgeEditor is not loaded. "
			"This only works in the editor."));
		return;
	}

	GEngine->Exec(nullptr, Command);
}

int32 UMotionForgeToolset::MigrateMotionDefinitions()
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->MigrateDefinitions() : 0;
}

// -------------------------------------------------------------------------------------------------
// The prompt on a timeline
// -------------------------------------------------------------------------------------------------

FString UMotionForgeToolset::CreatePromptSequence(
	const FString& AssetPath, const FString& SequenceAssetPath)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return FString();
	}

	FString Error;
	const FString Created = Forge->CreatePromptSequence(AssetPath, SequenceAssetPath, Error);

	if (Created.IsEmpty())
	{
		MotionForgeToolsetPrivate::Fail(Error.IsEmpty()
			? FString::Printf(TEXT("Could not build a prompt sequence for '%s'."), *AssetPath)
			: Error);
	}

	return Created;
}

void UMotionForgeToolset::RefreshPromptSequenceTake(const FString& AssetPath)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return;
	}

	FString Error;
	if (!Forge->RefreshPromptSequenceTake(AssetPath, Error))
	{
		MotionForgeToolsetPrivate::Fail(Error);
	}
}

FMotionPromptRead UMotionForgeToolset::ReadPromptBeats(const FString& AssetPath)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return FMotionPromptRead();
	}

	FString Error;
	const FMotionPromptRead Read = Forge->ReadPromptSequence(AssetPath, Error);

	// A definition with no sequence is a success with FromSequence false, so an error here means the
	// definition is missing or its track will not read - both worth stopping on rather than reporting
	// an empty beat list that reads like a prompt nobody wrote.
	if (!Error.IsEmpty())
	{
		MotionForgeToolsetPrivate::Fail(Error);
	}

	return Read;
}

void UMotionForgeToolset::BakePromptBeatsIntoDefinition(const FString& AssetPath)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return;
	}

	FString Error;
	if (!Forge->BakePromptSequence(AssetPath, Error))
	{
		MotionForgeToolsetPrivate::Fail(Error.IsEmpty()
			? FString::Printf(TEXT("Could not bake the prompt sequence on '%s'."), *AssetPath)
			: Error);
	}
}

// -------------------------------------------------------------------------------------------------
// Pipeline
// -------------------------------------------------------------------------------------------------

UToolCallAsyncResultMotionStatus* UMotionForgeToolset::WatchBatch(
	const FString& BatchId,
	const TArray<FString>& DefinitionPaths,
	const FString& FailureMessage)
{
	UToolCallAsyncResultMotionStatus* Result = NewObject<UToolCallAsyncResultMotionStatus>();

	// The result outlives this call and is only referenced by the ticker lambda, which the garbage
	// collector cannot see. Root it now, unroot on every exit path below.
	Result->AddToRoot();

	if (BatchId.IsEmpty())
	{
		Result->SetError(FailureMessage);
		Result->RemoveFromRoot();
		return Result;
	}

	const double Deadline = FPlatformTime::Seconds() + MotionForgeToolsetPrivate::MaxWatchSeconds;
	TArray<FString> Paths = DefinitionPaths;

	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[Result, BatchId, Paths, Deadline](float) -> bool
			{
				UMotionForgeSubsystem* Forge = UMotionForgeSubsystem::Get();
				if (!Forge)
				{
					Result->SetError(TEXT("MotionForge became unavailable while the batch was running."));
					Result->RemoveFromRoot();
					return false;
				}

				if (Forge->GetBatchStatus(BatchId).bFinished)
				{
					// Report per-definition outcomes rather than the batch summary. "Finished" only
					// says the jobs stopped; the caller needs to know which produced animations.
					Result->SetValue(Forge->GetStatus(Paths));
					Result->RemoveFromRoot();
					return false;
				}

				if (FPlatformTime::Seconds() > Deadline)
				{
					Result->SetError(FString::Printf(
						TEXT("Timed out watching batch %s. Jobs may still be running - check Get Motion Status."),
						*BatchId));
					Result->RemoveFromRoot();
					return false;
				}

				return true;
			}),
		MotionForgeToolsetPrivate::PollSeconds);

	return Result;
}

UToolCallAsyncResultMotionStatus* UMotionForgeToolset::GenerateMotions(const TArray<FString>& AssetPaths)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return WatchBatch(FString(), AssetPaths, TEXT("MotionForge is not available."));
	}

	UE_LOG(LogMotionForgeToolset, Log, TEXT("Tool: GenerateMotions on %d definition(s)"), AssetPaths.Num());

	const FString BatchId = Forge->Generate(AssetPaths);
	return WatchBatch(BatchId, AssetPaths,
		TEXT("Nothing was eligible to generate. Definitions may already be running, or may be "
			 "missing a prompt, a character, or an API key - check Get Motion Status and "
			 "Get Credential Status."));
}

void UMotionForgeToolset::SelectTake(const FString& AssetPath, const FString& MotionId)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return;
	}

	if (!Forge->SelectCandidate(AssetPath, MotionId))
	{
		MotionForgeToolsetPrivate::Fail(FString::Printf(
			TEXT("'%s' has no take with motion id '%s', or there is no definition at that path. "
				 "Call Get Motion Status for the takes each definition actually has."),
			*AssetPath, *MotionId));
	}
}

UToolCallAsyncResultMotionStatus* UMotionForgeToolset::DownloadAndImportSelected(
	const TArray<FString>& AssetPaths)
{
	UToolCallAsyncResultMotionStatus* Result = NewObject<UToolCallAsyncResultMotionStatus>();
	Result->AddToRoot();

	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		Result->SetError(TEXT("MotionForge is not available."));
		Result->RemoveFromRoot();
		return Result;
	}

	UE_LOG(LogMotionForgeToolset, Log, TEXT("Tool: DownloadAndImportSelected on %d definition(s)"),
		AssetPaths.Num());

	Forge->DownloadSelected(AssetPaths);

	// Downloading is not batch tracked - each definition walks its own download, normalise and import
	// chain. Watch the definitions themselves settle rather than a batch id.
	const double Deadline = FPlatformTime::Seconds() + MotionForgeToolsetPrivate::MaxWatchSeconds;
	TArray<FString> Paths = AssetPaths;

	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[Result, Paths, Deadline](float) -> bool
			{
				UMotionForgeSubsystem* Live = UMotionForgeSubsystem::Get();
				if (!Live)
				{
					Result->SetError(TEXT("MotionForge became unavailable during download."));
					Result->RemoveFromRoot();
					return false;
				}

				const TArray<FMotionDefinitionStatus> Status = Live->GetStatus(Paths);

				bool bAllSettled = true;
				for (const FMotionDefinitionStatus& Definition : Status)
				{
					if (Definition.Status == EMotionDefStatus::Downloading
						|| Definition.Status == EMotionDefStatus::Processing)
					{
						bAllSettled = false;
						break;
					}
				}

				if (bAllSettled)
				{
					Result->SetValue(Status);
					Result->RemoveFromRoot();
					return false;
				}

				if (FPlatformTime::Seconds() > Deadline)
				{
					Result->SetError(TEXT("Timed out waiting for downloads. Check Get Motion Status."));
					Result->RemoveFromRoot();
					return false;
				}

				return true;
			}),
		MotionForgeToolsetPrivate::PollSeconds);

	return Result;
}

UToolCallAsyncResultMotionStatus* UMotionForgeToolset::RunFullPipeline(const TArray<FString>& AssetPaths)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return WatchBatch(FString(), AssetPaths, TEXT("MotionForge is not available."));
	}

	UE_LOG(LogMotionForgeToolset, Warning,
		TEXT("Tool: RunFullPipeline on %d definition(s) - this will download unattended."),
		AssetPaths.Num());

	const FString BatchId = Forge->RunFullPipeline(AssetPaths);
	return WatchBatch(BatchId, AssetPaths, TEXT("Nothing was eligible to run."));
}

void UMotionForgeToolset::CancelBatch(const FString& BatchId)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return;
	}

	if (!Forge->CancelBatch(BatchId))
	{
		MotionForgeToolsetPrivate::Fail(FString::Printf(
			TEXT("Batch '%s' is not being tracked, so there is nothing to stop waiting on."), *BatchId));
	}
}

void UMotionForgeToolset::CancelMotionDefinition(const FString& AssetPath)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (Forge && !Forge->CancelDefinition(AssetPath))
	{
		MotionForgeToolsetPrivate::Fail(FString::Printf(
			TEXT("'%s' is not generating, so there is nothing to cancel. Downloading and importing cannot be "
				 "interrupted part way; Get Motion Activity shows what is running."), *AssetPath));
	}
}

UToolCallAsyncResultMotionStatus* UMotionForgeToolset::ChooseAndImportTake(const FString& AssetPath, const FString& MotionId)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (!Forge)
	{
		return WatchBatch(FString(), { AssetPath }, TEXT("MotionForge is not available."));
	}

	if (!Forge->SelectCandidate(AssetPath, MotionId))
	{
		return WatchBatch(FString(), { AssetPath }, FString::Printf(
			TEXT("'%s' has no take '%s'. Call Get Motion Status for the takes it has."), *AssetPath, *MotionId));
	}

	return DownloadAndImportSelected({ AssetPath });
}

void UMotionForgeToolset::HideTake(const FString& AssetPath, const FString& MotionId, bool Hidden)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	if (Forge && !Forge->HideTake(AssetPath, MotionId, Hidden))
	{
		MotionForgeToolsetPrivate::Fail(FString::Printf(
			TEXT("'%s' has no take '%s'. Call Get Motion Status for the takes it has."), *AssetPath, *MotionId));
	}
}

TArray<FString> UMotionForgeToolset::GetClipUsers(const FString& AssetPath)
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->GetClipUsers(AssetPath) : TArray<FString>();
}

TArray<FMotionActivity> UMotionForgeToolset::GetMotionActivity()
{
	UMotionForgeSubsystem* Forge = MotionForgeToolsetPrivate::Subsystem();
	return Forge ? Forge->GetActivities() : TArray<FMotionActivity>();
}

FString UMotionForgeToolset::GetToolsetVersion() const
{
	// The descriptor is the version. Reading it here rather than repeating it means there is no
	// second copy to keep true - and no window, between a bump and a fix, where an agent is told
	// a number the package does not carry.
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT(UE_PLUGIN_NAME));
	return Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : FString();
}
