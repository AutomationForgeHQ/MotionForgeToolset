// What an agent needs to know about MotionForge that the tool signatures cannot say.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/AgentSkill.h"
#include "MotionForgeSkill.generated.h"

/**
 * How to drive MotionForge without spending the user's money or their judgement.
 *
 * Deliberately short and free of tool signatures. The tools describe themselves; what belongs here
 * is the ordering, the economics and who decides what - which no signature can express and no
 * model can guess. **Every fact here is stated for the provider it is true of**: a plugin with
 * several providers is exactly where one provider's rule, stated as universal, misleads.
 */
UCLASS()
class MOTIONFORGETOOLSET_API UMotionForgeSkill : public UAgentSkill
{
	GENERATED_BODY()

public:

	UMotionForgeSkill()
	{
		Description = TEXT(
			"Generate character animation from text prompts through MotionForge: setting up a provider "
			"and a character, authoring motion definitions, directing takes, and importing the chosen one.");

		Instructions = TEXT(
			"MotionForge turns a written prompt into an imported animation sequence, through one of "
			"several providers chosen per motion. Almost everything that matters - price, length "
			"limits, whether seeds or poses exist - belongs to the provider, so read it rather than "
			"remembering it: the provider's capabilities, and Preview Motion Request for a definition "
			"that exists. That preview is built by the same code that submits, so it is the truth about "
			"what Generate would send and cost.\n"
			"\n"
			"Money, per provider. Kimodo on the user's own machine is free: ask for several takes and "
			"let them pick. Kimodo on a rented GPU bills by the hour while the pod is up, whether or not "
			"anything generates - never leave one running, and say so when you start one. Uthana on pay "
			"as you go bills every generated second at submission, kept or discarded, so takes multiply "
			"the bill exactly; fetching a take to watch is free. Uthana on a subscription is the other "
			"way round: generating is free and importing uses download quota. Before anything that "
			"bills, tell the user the cost summary and get their agreement on the number.\n"
			"\n"
			"The person chooses. Generating stops once takes exist; the definition's window plays each "
			"take on the character before it is chosen, which is where a human judges motion. Do not "
			"choose a take for them unless they asked you to. Choosing imports it over the definition's "
			"one clip, which montages and dialogue may already use - ask which assets use the clip and "
			"say what will change first. Takes are hidden, never deleted: without a seed a take cannot "
			"be made again.\n"
			"\n"
			"A first-time user needs setup more than prompts. Get the provider's setup steps and relay "
			"what they say; many are the person's to do - installing Docker, accepting a model licence, "
			"storing a key - and this toolset deliberately has no tool that stores a credential: point "
			"the person at the Keys page. Kimodo's text "
			"encoder is a gated Llama 3 model: a Hugging Face token without that grant installs fine and "
			"then fails every generation, so check the access row, not just the token. When someone asks "
			"how to begin, open the MotionForge window on Get started rather than reciting the steps.\n"
			"\n"
			"A character must be prepared for the provider before it can generate for it, and one "
			"prepared for one provider does not suit another. Ask which characters suit a provider. On "
			"Uthana, preparing means uploading the mesh once, which is free; on Kimodo a character works "
			"straight onto its skeleton, and a Kimodo rig is an optional improvement for the feet.\n"
			"\n"
			"Each provider has its own settings on a definition, listed by its own names. The seed is "
			"the useful one where it exists: leave it random to explore - each take records the seed it "
			"used - and fix it to change one word or one setting and see only that difference.\n"
			"\n"
			"The prompt is the work. One person, one or two actions, how it ends. Put weight in the body "
			"rather than in adjectives - 'braces, shifts weight back, lifts with the legs' beats 'lifts a "
			"heavy crate' - name the limbs that matter, and end in a neutral stance so the clip blends "
			"out. Some providers have a minimum length and pad a short action to fill it; ask for the "
			"length the action needs and trim. Some cut the prompt into beats at every full stop and "
			"generate each in turn - the capabilities say which - and there a beat inherits the body "
			"from the beat before but not its words, so every beat must name the pose it acts on: 'holds "
			"the arm still, raised', not 'holds still'.\n"
			"\n"
			"Poses, where the provider takes them (Kimodo does, Uthana does not). Pin the body at the "
			"moments that matter and leave the rest to the model: full body at the start and end, a hand "
			"or foot for a contact in the middle. Keep it sparse. Read the payload that would be sent "
			"before generating - a pose silently dropped looks exactly like a model that ignored it - "
			"and judge the result by measuring the clip, not by description.\n"
			"\n"
			"After importing, report the clip's provenance and read whether it looks mis-rated: a clip "
			"whose frames and duration disagree with its provider's rate plays too fast while passing "
			"every other check. Do it again after anything under the pipeline changes - a provider, a "
			"runner image, the machine doing the importing.\n"
			"\n"
			"Retrying is safe at every step: a definition already generating is skipped rather than "
			"submitted twice, so a call that timed out can be made again without paying twice.\n"
			"\n"
			"A prompt can also be authored on a timeline - one section per beat, with a Control Rig for "
			"posing where the provider takes poses. While a definition has one, the timeline is the "
			"prompt. It is a human's surface: whether a beat feels long enough or a pose looks right is "
			"theirs to judge.");
	}
};
