// Typed promise for the tools that have to wait on a provider.

#pragma once

#include "CoreMinimal.h"
#include "MotionForgeTypes.h"
#include "ToolsetRegistry/ToolCallAsyncResult.h"
#include "MotionForgeAsyncResult.generated.h"

/**
 * An async tool call that completes with per-definition motion status.
 *
 * Generation and download take tens of seconds at best, so the tools that drive them return a
 * promise rather than blocking the editor. The registry finds the schema by reflecting over the
 * property literally named Value, which is why this exists at all instead of reusing the string
 * result: a caller waiting minutes for a batch should get back the same typed status Get Motion
 * Status returns, not a string it has to parse.
 */
UCLASS(BlueprintType)
class MOTIONFORGETOOLSET_API UToolCallAsyncResultMotionStatus : public UToolCallAsyncResult
{
	GENERATED_BODY()

public:

	/** Complete the call successfully. Safe to call from any thread. */
	UFUNCTION(BlueprintCallable, Category = "MotionForge")
	bool SetValue(const TArray<FMotionDefinitionStatus>& InValue)
	{
		return MaybeBroadcastSuccessfulCompletion(TArray<FMotionDefinitionStatus>(InValue), Value);
	}

	/** One entry per definition the call was watching. */
	UPROPERTY(BlueprintReadOnly, Category = "MotionForge")
	TArray<FMotionDefinitionStatus> Value;
};

/** An async tool call that completes with the characters a provider holds. */
UCLASS(BlueprintType)
class MOTIONFORGETOOLSET_API UToolCallAsyncResultRemoteCharacters : public UToolCallAsyncResult
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "MotionForge")
	bool SetValue(const TArray<FMotionRemoteCharacter>& InValue)
	{
		return MaybeBroadcastSuccessfulCompletion(TArray<FMotionRemoteCharacter>(InValue), Value);
	}

	UPROPERTY(BlueprintReadOnly, Category = "MotionForge")
	TArray<FMotionRemoteCharacter> Value;
};

/** An async tool call that completes with the outcome of pairing a character with a provider. */
UCLASS(BlueprintType)
class MOTIONFORGETOOLSET_API UToolCallAsyncResultCharacterUpload : public UToolCallAsyncResult
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "MotionForge")
	bool SetValue(const FMotionCharacterUpload& InValue)
	{
		return MaybeBroadcastSuccessfulCompletion(FMotionCharacterUpload(InValue), Value);
	}

	UPROPERTY(BlueprintReadOnly, Category = "MotionForge")
	FMotionCharacterUpload Value;
};
