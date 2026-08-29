# MotionForge Toolset

Exposes [MotionForge](../MotionForge/README.md) as native Model Context Protocol tools, so an agent
can author motion definitions, generate, review, download and import without a human driving the
editor.

**Status: 0.3 — verified live, whole surface exercised.** The toolset registers as
`MotionForgeToolset.MotionForgeToolset` and `UMotionForgeSkill` is listed by
`AgentSkillToolset.ListSkills`. Connection test, character upload and pairing, generation, polling,
download and import have all been driven through these tools against a live Uthana account, producing
an animation now in use in game.

The retarget path is the one thing still unexercised — it engages only for a character you did not
upload, and is not needed when you did.

---

## Why this is a separate plugin

MotionForge must drop into any UE 5.8 project. `ToolsetRegistry` and `ModelContextProtocol` are
**Experimental** engine plugins, and folding this in would make MotionForge refuse to load anywhere
they are turned off.

So the split is the usual one: MotionForge is the capability, this is an adapter onto a surface.
Delete this plugin and MotionForge behaves identically.

This module holds **no pipeline logic**. Every tool forwards to `UMotionForgeSubsystem` and adds
nothing but a doc comment and an error message.

---

## The three parts

Epic's model for extending the Unreal MCP has three pieces, and this plugin ships all three.

### 1. Toolset — `UMotionForgeToolset`

A `UToolsetDefinition` subclass whose static `UFUNCTION`s are marked `meta = (AICallable)`. The
registry reflects over them: **the signature is the schema**. Parameter names, types, defaults and
doc comments all become the JSON schema an agent reads before calling anything.

That is why nothing here takes or returns a JSON string. A tool typed `FString Json` publishes a
schema that says only "a string", and the agent has to guess the field names from prose. A tool
typed `TArray<FMotionDefSpec>` publishes every field, its type and its tooltip.

```cpp
UFUNCTION(meta = (AICallable), Category = "MotionForge|Authoring")
static FMotionBatchSubmission CreateAndGenerateMotions(
    const TArray<FMotionDefSpec>& Definitions,
    EMotionPipelineMode Mode);
```

### 2. Skill — `UMotionForgeSkill`

A native `UAgentSkill`. It carries what no signature can express: **which way round the billing goes**,
because pay-as-you-go bills generated seconds while a subscription bills downloads, and getting it
backwards costs real money — and that these models pad an underspecified prompt out to their minimum
clip length. Short and deliberately free of tool and property names, which rot.

### 3. Examples

None yet. The motion libraries in `PROTOTYPE_CONTENT_PIPELINE.md` §3.8 — the interaction verbs and
the conversational gestures — become the examples once they exist, and definitions are already
readable through `GetMotionStatus`, so discovery needs no new tool.

---

## Tools

| Tool | Returns | |
|---|---|---|
| `ListMotionDefinitions` | `TArray<FString>` | Filter by any set of statuses |
| `GetMotionStatus` | `TArray<FMotionDefinitionStatus>` | Status, takes and errors per definition |
| `GetBatchStatus` | `FMotionBatchStatus` | Progress of a running batch |
| `ListProviders` | `TArray<FName>` | Providers compiled into this project |
| `GetCredentialStatus` | `FMotionCredentialInfo` | Whether a key exists — never its value |
| `TestProviderConnection` | **async** string | One cheap authenticated call; costs nothing |
| `ListProviderCharacters` | **async** `TArray<FMotionRemoteCharacter>` | What the account already holds |
| `UploadCharacterToProvider` | **async** `FMotionCharacterUpload` | Export + upload + pair, in one call |
| `EstimateDownloadCost` | `FMotionCostEstimate` | Seconds that would be fetched |
| `CreateAndGenerateMotions` | `FMotionBatchSubmission` | Author a library and start it |
| `UpdateMotionDefinition` | — | Reword a prompt without losing takes |
| `GenerateMotions` | **async** status | Generate, wait, stop at review |
| `SelectTake` | — | Choose a take by motion id |
| `DownloadAndImportSelected` | **async** status | Fetch, normalise, import |
| `RunFullPipeline` | **async** status | Unattended end to end |
| `CancelBatch` | — | Stop waiting; jobs keep running |

### Errors are raised, not returned

Failures call `UKismetSystemLibrary::RaiseScriptError`. The registry watches for script exceptions
while a tool runs and turns them into a tool error the agent sees. That keeps success types clean —
no `ok` field to check on every call — and it is the standard path.

Every message says what to call next. `SelectTake` with a bad id points at `GetMotionStatus`;
`GetCredentialStatus` with a bad provider points at `ListProviders`. Models are good at recovering
from a mistake and bad at noticing they made one.

### Async tools return promises, not poll handles

`ToolsetRegistry` ships `UToolCallAsyncResult`, so long operations return a result that *completes*
when the work finishes rather than making the caller poll. `UToolCallAsyncResultMotionStatus`
extends it to complete with `TArray<FMotionDefinitionStatus>` — the registry finds the schema by
reflecting over the property literally named `Value`, so a caller that waited three minutes gets the
same typed status back as a caller that asked directly.

They complete with **per-definition status**, not a batch summary. "Finished" only says the jobs
stopped; the caller needs to know which ones produced animations.

Watched operations time out after 30 minutes and say so, pointing at `GetMotionStatus` — jobs may
still be running provider-side, and nothing paid for is lost.

---

## Two deliberate omissions

**No tool sets an API key.** An agent that can write secrets into the OS credential vault is a
liability with no matching benefit. Signing in is a human action, done once, in Project Settings.
Agents can ask whether a key exists; they can neither read nor set one.

**No tool deletes anything.** Candidates are never pruned by MotionForge because providers without a
seed cannot reproduce a take — the motion id is the only route back to it. Nothing here should be
able to throw that away.

---

## Registration

`StartupModule` calls `UToolsetRegistry::RegisterToolsetClass`, guarded by
`UToolsetRegistry::IsAvailable()` — in a cooked build, a commandlet, or an editor with the
experimental plugins off, the module loads and quietly does nothing. An adapter should never be the
reason a project fails to start.

The skill needs no registration. `UAgentSkillToolset::ListSkills` walks every class derived from
`UAgentSkill`, native ones included.

The toolset appears to clients as **`MotionForgeToolset.MotionForgeToolset`** — the registry builds the
name from the module and the class.

---

## Guidance lives in doc comments and the skill

Tool descriptions are the doc comments in `MotionForgeToolset.h`. Treat them as user-facing copy,
not internal notes. Split the two kinds of guidance:

- **How to call this tool** → the doc comment. Argument meanings, what an empty array does, what to
  call first.
- **How to work with the pipeline** → `MotionForgeSkill.h`. Ordering, economics, how to write a
  prompt. Anything that would have to be repeated on four tools belongs there instead.

---

## Layout

```
MotionForgeToolset.uplugin    editor-only; depends on MotionForge, ToolsetRegistry, ModelContextProtocol
Source/MotionForgeToolset/
  MotionForgeToolsetModule.*  module; conditional registration
  MotionForgeToolset.*        the tools
  MotionForgeSkill.h          the skill
  MotionForgeAsyncResult.h    typed promise for the tools that wait
```

Filter the Output Log on **`LogMotionForgeToolset`** for registration and tool calls.
