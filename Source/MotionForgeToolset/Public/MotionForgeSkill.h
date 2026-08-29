// What an agent needs to know about MotionForge that the tool signatures cannot say.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/AgentSkill.h"
#include "MotionForgeSkill.generated.h"

/**
 * How to drive the MotionForge pipeline without wasting the user's download quota.
 *
 * Deliberately short and free of tool signatures. The tools describe themselves; what belongs here
 * is the ordering and the economics, which no signature can express and no model can guess.
 */
UCLASS()
class MOTIONFORGETOOLSET_API UMotionForgeSkill : public UAgentSkill
{
	GENERATED_BODY()

public:

	UMotionForgeSkill()
	{
		Description = TEXT(
			"Generate character animation from text prompts through MotionForge: authoring motion "
			"definitions, reviewing generated takes, and importing the chosen one as an animation.");

		Instructions = TEXT(
			"MotionForge turns a written prompt into an imported animation sequence. Two things "
			"shape how you should use it.\n"
			"\n"
			"First, the money - and check which way round it is before doing anything, because the "
			"two plans invert the correct workflow. Ask the cost tools; they know which model the "
			"project is configured for and report Billed Seconds and an Estimated Cost accordingly. "
			"Never assume.\n"
			"\n"
			"On pay-as-you-go every generated second bills at submission, kept or discarded, and "
			"downloads are free. Variants are the spend. Ask for two or three, not eight, price the "
			"batch before submitting, and tell the user the number and get their agreement. Once a "
			"take exists it is paid for, so download all of them - reviewing first saves nothing.\n"
			"\n"
			"On a subscription it is the other way: generation is free and downloads come out of a "
			"quota. Then generate generously, review in the web viewer, and fetch only the keeper.\n"
			"\n"
			"Either way, a failed or ugly take on pay-as-you-go is money gone. That makes the prompt "
			"worth more care than a retry.\n"
			"\n"
			"Second, the prompt is the work. These models have a minimum clip length and will spend "
			"the whole duration whether or not you have told them how, so an underspecified prompt "
			"comes back padded and lifeless. Write it as beats, give each beat its own tempo word, "
			"and end in a neutral standing idle so the "
			"clip blends out cleanly. Put weight in the body rather than in adjectives - 'braces, "
			"shifts his weight back, lifts with the legs' beats 'lifts a heavy crate'. Name the "
			"limbs that matter. Do not describe the camera, the scene, or anything the character "
			"cannot do with their own body.\n"
			"\n"
			"SOME providers cut a prompt into beats and generate each separately, and on those the "
			"advice above applies per beat rather than per clip - which is a much smaller thing to "
			"underspecify. Whether yours does is a capability, not an assumption: ask Get Provider "
			"Capabilities and read the provider's own guidance, because a provider that does not "
			"segment ignores beats entirely and one that does has rules about them that no signature "
			"here can state.\n"
			"\n"
			"The one worth knowing wherever it applies: on a segmenting provider a beat inherits the "
			"body from the beat before it but **not the words**, so every beat must name the pose it "
			"acts on in full. 'The person holds the arm perfectly still' fails, because standing "
			"with the arms down is perfectly still and that beat cannot see the sentence which "
			"raised the arm.\n"
			"\n"
			"Because of the minimum length, ask for a longer clip than you want and trim it back to "
			"the part you asked for.\n"
			"\n"
			"The normal order is: check the provider has a key and the character is paired, price the "
			"batch, author and generate, wait, read the takes back, offer the user the viewer links "
			"so they can watch candidates for free, select one, then download and import. Retrying "
			"is safe at every step - definitions already in flight are skipped rather than "
			"resubmitted, so a call that timed out can simply be made again without paying twice.\n"
			"\n"
			"Third, a clip that imported successfully can still be wrong in a way nothing reports. "
			"Run Report Motion Provenance after importing and read bLooksMisRated first. It is true "
			"when a clip's own frames and duration disagree with its provider's frame rate, which "
			"means it holds more motion than its length allows and plays too fast - and that failure "
			"passes bone counts, curve counts, a clean log, and a duration exactly as requested. "
			"Three clips shipped that way for a week. The same report is how you tell which provider "
			"made a clip, since 60fps is correct from one and a defect from another, and the asset is "
			"the only reliable place to ask: definitions get renamed, regenerated and deleted, and "
			"two takes can share a name in different folders.\n"
			"\n"
			"Run it again after anything changes underneath the pipeline - a provider, a runner "
			"image, or the machine doing the importing. That last one is not paranoia: whether the "
			"Blender normalisation step ran, which rotates a clip ninety degrees, once depended on "
			"nothing more than whether that computer happened to have Blender installed.\n"
			"\n"
			"A character must be paired with the provider before anything can generate. That is one "
			"upload, done once, and it costs nothing. Check what the account already holds before "
			"uploading another.\n"
			"\n"
			"Takes are never deleted, and on providers without a seed a discarded take can never be "
			"regenerated. Treat the motion id as the only route back to a take.\n"
			"\n"
			"If a provider has no key, say so and point at Project Settings. Signing in is the "
			"user's job, not yours - there is no tool for it and that is on purpose.\n"
			"\n"
			"Finally, a prompt can be authored on a timeline instead of in a sentence. Create Prompt "
			"Sequence lays a definition out in a Level Sequence, one section per beat, where a human "
			"can drag a boundary to retime one - and from then on the sequence IS the prompt: "
			"generation reads the track, and the definition's own Prompt and Beat Seconds are "
			"ignored while it is set. Read Prompt Beats reports what a definition will actually "
			"generate from either way, and Bake Prompt Beats Into Definition copies the sequence "
			"back onto the asset so deleting it later costs nothing.\n"
			"\n"
			"It earns its keep on a provider that segments and cares about per-beat durations, and "
			"on one that does not it is simply a tidier place to write a prompt. Either way it is a "
			"human's surface - the timeline also carries a Control Rig for posing constraints, where "
			"the provider supports them. Leave the judging to them: whether a beat feels long "
			"enough, or a pose looks right, is not something you can measure.");
	}
};
