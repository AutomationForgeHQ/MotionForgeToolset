// MotionForge, as tools an agent can call.

#pragma once

#include "CoreMinimal.h"
#include "MotionForgeTypes.h"
#include "MotionPromptSequence.h"
#include "MotionTakeProvenance.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "MotionForgeToolset.generated.h"

class UToolCallAsyncResultMotionStatus;
class UToolCallAsyncResultString;
class UToolCallAsyncResultRemoteCharacters;
class UToolCallAsyncResultCharacterUpload;

/**
 * The MotionForge pipeline exposed as Model Context Protocol tools.
 *
 * Every function forwards to UMotionForgeSubsystem and adds nothing. All logic, state and safety
 * live in the capability plugin; this is a surface, and deleting it changes nothing about how
 * MotionForge behaves.
 *
 * Signatures are the schema. Parameters and return values are USTRUCTs and enums rather than JSON
 * strings, so the registry publishes a typed schema an agent can fill in correctly without guessing
 * field names, and doc comments here become the tool descriptions it reads. They are written for
 * that reader, not for us.
 *
 * Failures raise a script error through UKismetSystemLibrary::RaiseScriptError rather than returning
 * an ok/error envelope. The registry turns those into tool errors the agent can act on, which is
 * both the standard path and the one that keeps success types clean.
 *
 * One thing is deliberately absent: there is no tool for setting an API key. An agent that can write
 * secrets into the OS credential vault is a liability with no matching benefit, and signing in is a
 * human action performed once in Project Settings. Agents can ask whether a key exists, never set or
 * read one.
 */
UCLASS(BlueprintType)
class MOTIONFORGETOOLSET_API UMotionForgeToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.2"); }

	// ---------------------------------------------------------------------------------------------
	// Discovery
	// ---------------------------------------------------------------------------------------------

	/**
	 * List the content paths of motion definitions in the project.
	 *
	 * @param StatusFilter Statuses to keep. Leave empty for every definition.
	 * @return Asset paths, which every other tool here takes as its handle.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Discovery")
	static TArray<FString> ListMotionDefinitions(const TArray<EMotionDefStatus>& StatusFilter);

	// ---------------------------------------------------------------------------------------------
	// Output
	// ---------------------------------------------------------------------------------------------

	/**
	 * Where the pipeline writes: the configured root, and every folder derived from it.
	 *
	 * Worth reading before generating anything into a project you did not set up, because it is the
	 * only way to know where results will land without waiting to see where they landed.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Output")
	static FMotionOutputPaths GetMotionOutputPaths();

	/**
	 * Point the pipeline at a different content root.
	 *
	 * Only the root is settable. The folders beneath it - Definitions, Takes, Characters, Rigs,
	 * Sequences, Montages - are derived and stay as they are, because the sort by kind is how the
	 * output stays readable after the second run rather than a preference to be overridden.
	 *
	 * **Moves nothing.** Assets already written stay where they are and keep working from there; this
	 * decides where the next ones are created. Anything the pipeline *reads* by path - a provider rig,
	 * a curve preset - must still be findable where it actually is, so relocating output does not
	 * relocate inputs and does not need to.
	 *
	 * The change is written to the project's DefaultEditor.ini and persists across restarts.
	 *
	 * @param ContentPath A content folder such as `/Game/_EP1/Motion`. Must be under a mounted root -
	 *        content written somewhere unmounted is created in memory, never saved, and reported as a
	 *        success.
	 * @return The resolved structure. On refusal, `Problem` says why and every path describes what is
	 *         still configured rather than what was asked for.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Output")
	static FMotionOutputPaths SetMotionOutputRoot(const FString& ContentPath);

	/**
	 * What every imported clip says about itself: which provider and model made it, on which runner,
	 * how many frames it has, and whether it was normalised or retargeted.
	 *
	 * **Read `bLooksMisRated` first.** It is true when a clip's own frames and duration disagree with
	 * its provider's frame rate, which means it holds more motion than its length allows and plays too
	 * fast. That failure passes bone counts, curve counts, a clean log, and a duration exactly as
	 * requested - three clips shipped that way for a week before anyone divided one number by another.
	 *
	 * Worth running after changing a provider, a runner image, or the machine doing the importing.
	 * Clips made before provenance existed report an empty provider rather than a guess.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Discovery")
	static TArray<FMotionProvenanceRow> ReportMotionProvenance();


	/**
	 * Report status, generated takes and errors for motion definitions.
	 *
	 * @param AssetPaths Definitions to report on. Leave empty for every definition.
	 * @return One entry per definition, including the take ids Select Take accepts.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Discovery")
	static TArray<FMotionDefinitionStatus> GetMotionStatus(const TArray<FString>& AssetPaths);

	/**
	 * Report how far a generation batch has got, given the id returned when it was started.
	 *
	 * A batch stops being tracked once its jobs settle, so an unknown id comes back with Tracked
	 * false and Finished true rather than as an error. Read per-definition status after that.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Discovery")
	static FMotionBatchStatus GetBatchStatus(const FString& BatchId);

	/** List the motion providers this project has compiled in. */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Discovery")
	static TArray<FName> ListProviders();

	/**
	 * Report what a provider can actually do: frame rate, clip length limits, whether it charges,
	 * whether it can reproduce a generation from a seed, and whether it needs an API key.
	 *
	 * **Read this rather than assuming.** Providers differ in ways that change the right workflow
	 * completely. One bills every generated second and cannot reproduce a take, so variants are a
	 * spending decision and a discarded clip is gone forever. Another runs locally, costs nothing,
	 * and seeds - so ask for as many variants as you like and treat the files as disposable.
	 *
	 * Setup Hint is the field to act on when it is not empty: it says what a human has to do before
	 * this provider will work at all, and that is never something you can do for them.
	 *
	 * @param ProviderId A name from List Providers. Leave empty for the default provider.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Discovery")
	static FMotionProviderCaps GetProviderCapabilities(FName ProviderId);

	/**
	 * Report whether a provider has a usable API key, and where it is read from.
	 *
	 * Never returns the key itself, and providers cannot be signed in to from here. When Configured
	 * is false, tell the user to add the key in Project Settings rather than trying to set it.
	 *
	 * @param ProviderId A name from List Providers. Leave empty for the default provider.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Discovery")
	static FMotionCredentialInfo GetCredentialStatus(FName ProviderId);

	/**
	 * Make one cheap authenticated call to a provider and report what came back.
	 *
	 * Costs nothing and generates nothing. Worth doing once before a first batch, and it is the
	 * fastest way to tell a wrong key apart from a wrong prompt when generation fails.
	 *
	 * @param ProviderId A name from List Providers. Leave empty for the default provider.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Discovery")
	static UToolCallAsyncResultString* TestProviderConnection(FName ProviderId);

	// ---------------------------------------------------------------------------------------------
	// Characters
	// ---------------------------------------------------------------------------------------------

	/**
	 * List characters the provider already holds.
	 *
	 * Check here before uploading. A character only has to be paired once, and every take ever
	 * generated is tied to the id it was generated against.
	 *
	 * @param ProviderId A name from List Providers. Leave empty for the default provider.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Characters")
	static UToolCallAsyncResultRemoteCharacters* ListProviderCharacters(FName ProviderId);

	/**
	 * Export a motion character's mesh and upload it to its provider, pairing the two.
	 *
	 * This is the one thing that must happen before any motion can be generated: the provider
	 * retargets server-side against a character it holds, and the returned id is written into the
	 * asset automatically.
	 *
	 * Costs nothing and generates nothing, but it is **not** idempotent - each call creates another
	 * character on the provider. A character that already has an id is refused unless Force is set,
	 * because repointing it would orphan every take generated against the old id, and on a provider
	 * without seeds those takes cannot be reproduced.
	 *
	 * The character needs a Preview Mesh on its Target Skeleton. Leave the options alone for a mesh
	 * exported from this project: it is already rigged, and re-rigging would replace the bone names
	 * that imported animation has to bind to.
	 *
	 * @param CharacterAssetPath Content path of the Motion Character asset.
	 * @param Force Upload even though the character is already paired. Ask the user first.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Characters")
	static UToolCallAsyncResultCharacterUpload* UploadCharacterToProvider(
		const FString& CharacterAssetPath,
		const FMotionCharacterUploadOptions& Options,
		bool Force);

	/**
	 * Fetch a paired character back from its provider and import its rig into the project.
	 *
	 * Do this once per character, straight after Upload Character To Provider. Providers normalise a
	 * rig on ingest and generate against the normalised one, so their copy - not the file that was
	 * uploaded - is the skeleton generated clips actually fit. Importing it is what lets clips land
	 * with an exact bone match instead of being reconstructed and twisted.
	 *
	 * Costs nothing. Afterwards a human authors an IK Retargeter from the imported Provider Mesh to
	 * the character's Preview Mesh and sets it on the character; from then on every clip is
	 * retargeted onto the game's skeleton automatically.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Characters")
	static UToolCallAsyncResultString* ImportProviderCharacterRig(const FString& CharacterAssetPath);

	// ---------------------------------------------------------------------------------------------
	// Cost
	// ---------------------------------------------------------------------------------------------

	/**
	 * Price what generating a set of definitions would cost, before any of them exist.
	 *
	 * **Call this before Create And Generate Motions and tell the user the number.** On a
	 * pay-per-generated-second plan this is where all the money goes, it is spent the instant the
	 * job is submitted, and nothing refunds a take that turns out badly. Read Billed Seconds and
	 * Estimated Cost rather than assuming which of the two second-counts applies.
	 *
	 * Takes the same definitions Create And Generate Motions takes, so the two can be called with
	 * one payload.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Cost")
	static FMotionCostEstimate EstimateGenerationCost(const TArray<FMotionDefSpec>& Definitions);

	/**
	 * Report what these definitions would cost to fetch, and what regenerating them would cost.
	 *
	 * Which number bills depends on the plan: Billed Seconds and Estimated Cost already account for
	 * it, so use those rather than picking one yourself. On a subscription this is the call to make
	 * before downloading; on pay-as-you-go downloading is free and Estimate Generation Cost is the
	 * one that matters.
	 *
	 * @param AssetPaths Definitions to price. Leave empty for every definition.
	 * @param SelectedOnly True counts only chosen takes; false counts every usable take.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Cost")
	static FMotionCostEstimate EstimateDownloadCost(const TArray<FString>& AssetPaths, bool SelectedOnly);

	// ---------------------------------------------------------------------------------------------
	// Authoring
	// ---------------------------------------------------------------------------------------------

	/**
	 * Create or update motion definitions, and start generating takes for them.
	 *
	 * Writing the prompt is most of the job. Models with a minimum clip length spend the whole
	 * duration whether or not the prompt says how, so a vague prompt comes back padded and lifeless.
	 * Give each beat its own tempo word, state holds explicitly, put weight in the torso rather than
	 * calling an object heavy, and end in a neutral standing idle so the clip blends out cleanly.
	 *
	 * A definition whose asset name already exists is updated rather than duplicated, and keeps the
	 * takes it already has.
	 *
	 * **Every variant may cost money.** On a pay-per-generated-second plan each one bills the moment
	 * it is submitted, whether or not it is ever downloaded or kept, so Variants is a spending
	 * decision and not a free knob. Call Estimate Generation Cost first and get the user's agreement
	 * on the number. On a subscription the reverse holds and variants are free.
	 *
	 * @param Definitions What to author. Prompt and AssetName are the only fields worth setting by
	 *        hand; the rest fall back to the plugin's configured defaults when left empty.
	 * @param Mode Human In The Loop stops after generating so takes can be reviewed. Automatic runs
	 *        through to an imported animation and spends download quota unattended.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Authoring")
	static FMotionBatchSubmission CreateAndGenerateMotions(
		const TArray<FMotionDefSpec>& Definitions,
		EMotionPipelineMode Mode);

	/**
	 * Rewrite a definition's authoring fields without touching its takes or pipeline state.
	 *
	 * Use this to iterate on a prompt after reading the generated takes back. Generate Motions
	 * afterwards to produce takes from the new wording.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Authoring")
	static void UpdateMotionDefinition(const FString& AssetPath, const FMotionDefSpec& Definition);

	// ---------------------------------------------------------------------------------------------
	// The prompt on a timeline
	// ---------------------------------------------------------------------------------------------

	/**
	 * Lay a definition's prompt out in a Level Sequence, one section per beat, and point the
	 * definition at it.
	 *
	 * **From then on the sequence is the prompt.** Its sections carry the beat texts and their lengths
	 * are the beat durations, so a human can drag a boundary to retime a beat. The definition's own
	 * prompt and beat durations are ignored while it is set, and left untouched underneath.
	 *
	 * Offer this when a prompt has more than one beat and the user cares how long each takes. A
	 * segmenting provider divides a prompt at every full stop and gives each piece a duration whether
	 * or not anybody chose them, and the symptom of getting that wrong is a beat too short to perform
	 * rather than an error - which is what a timeline makes visible before generating instead of after.
	 *
	 * The same sequence also carries constraint poses, so a character on the definition's skeleton is
	 * bound into it ready to pose. Costs nothing, generates nothing, and needs no provider running.
	 *
	 * Refused when the definition has no prompt to lay out, or no skeleton to build around.
	 *
	 * @param SequenceAssetPath Where to put it. Leave empty for the plugin's own sequences folder.
	 * @return Content path of the Level Sequence.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Prompt")
	static FString CreatePromptSequence(const FString& AssetPath, const FString& SequenceAssetPath);

	/**
	 * Report what a motion definition will actually be generated from: its beats, how long each one
	 * lasts, and anything wrong with them.
	 *
	 * Answers for a definition with a prompt sequence and one without, so this is the reliable way to
	 * find out what a definition currently asks for. `FromSequence` says which it was.
	 *
	 * **Read `Problems` out to the user before generating.** Nothing in it blocks a generation and
	 * every entry changes what comes back in a way the timeline does not show - a beat that overlaps
	 * the next one, a first beat that does not start at zero, or a full stop inside a beat's text,
	 * which divides that beat in two at the far end and pairs every duration with the wrong sentence.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Prompt")
	static FMotionPromptRead ReadPromptBeats(const FString& AssetPath);

	/**
	 * Copy a definition's prompt sequence back onto the definition itself, as its prompt, beat
	 * durations and length.
	 *
	 * Insurance, not a step in the workflow. The sequence keeps winning afterwards and the reference
	 * is left in place - what this buys is that deleting the sequence later costs nothing, because the
	 * definition already says the same thing in the form it understood before the timeline existed.
	 *
	 * Worth doing once a prompt has settled. Refused on a definition that has no prompt sequence,
	 * because there its own fields are already the truth.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Prompt")
	static void BakePromptBeatsIntoDefinition(const FString& AssetPath);

	// ---------------------------------------------------------------------------------------------
	// Pipeline
	// ---------------------------------------------------------------------------------------------

	/**
	 * Generate takes for existing motion definitions and wait for them to finish.
	 *
	 * Stops once takes exist, leaving each definition awaiting review so a human can choose. Nothing
	 * is downloaded and nothing is charged for by this call.
	 *
	 * Definitions already generating are skipped, so calling this again after a timeout is safe.
	 *
	 * @return Per-definition status once the batch settles, including the take ids to choose from.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Pipeline")
	static UToolCallAsyncResultMotionStatus* GenerateMotions(const TArray<FString>& AssetPaths);

	/**
	 * Choose which generated take a motion definition should use, by its motion id.
	 *
	 * Take ids come from Get Motion Status. Each has a viewer URL a human can watch for free, so
	 * offer those rather than picking blind when the choice matters.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Pipeline")
	static void SelectTake(const FString& AssetPath, const FString& MotionId);

	/**
	 * Download the chosen take for each definition, normalise it and import it as an animation.
	 *
	 * This spends download quota. Check Estimate Download Cost first and confirm with the user.
	 * Takes already on disk are not fetched again.
	 *
	 * @return Per-definition status once every download settles, including the imported animation.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Pipeline")
	static UToolCallAsyncResultMotionStatus* DownloadAndImportSelected(const TArray<FString>& AssetPaths);

	/**
	 * Generate, automatically take the first usable variant, download it and import it, without
	 * stopping for review.
	 *
	 * This spends download quota unattended. Prefer Generate Motions followed by human review unless
	 * the user has explicitly asked for an unattended run.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Pipeline")
	static UToolCallAsyncResultMotionStatus* RunFullPipeline(const TArray<FString>& AssetPaths);

	/**
	 * Stop waiting on a batch.
	 *
	 * Jobs already submitted keep running and their takes remain retrievable, so nothing paid for is
	 * lost - only the waiting stops.
	 */
	UFUNCTION(meta = (AICallable), Category = "MotionForge|Pipeline")
	static void CancelBatch(const FString& BatchId);

private:

	/**
	 * Poll a batch to completion, then complete the async result with per-definition status.
	 *
	 * Definition paths are captured up front rather than read back from the batch: a finished batch
	 * is dropped from tracking, so by the time it reports done it no longer knows what it contained.
	 *
	 * Plain C++ rather than a UFUNCTION: the registry publishes every AICallable UFUNCTION on this
	 * class, and a helper is not a tool.
	 */
	static UToolCallAsyncResultMotionStatus* WatchBatch(
		const FString& BatchId,
		const TArray<FString>& DefinitionPaths,
		const FString& FailureMessage);
};
