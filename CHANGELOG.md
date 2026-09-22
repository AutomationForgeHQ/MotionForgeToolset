# MotionForgeToolset

Every released version of MotionForgeToolset, newest first. A release publishes **one** section of this
file — the one whose heading matches its tag — as its release notes; for an `open` plugin those
notes are posted to Discord `#releases` automatically. Write for someone who installs the plugin,
not for the commit log.

Headings are `## <x.y.z> — <date>`. Use `Added` / `Changed` / `Fixed` / `Compatibility` /
`Known issues`, only the ones that apply.

## 0.3.0 — 2026-09-22

### Added
- Create Motion Definition, a draft without generating; Preview Motion Request, exactly what Generate would send, what it would cost and what would stop it.
- Set Motion Provider, List Pipeline Options and Set Motion Pipeline Option: each provider's own settings by their own names.
- Find Characters For Provider, Get Provider Setup Steps, and Open MotionForge Window, which shows a person Get started or the library.
- Choose And Import Take, Hide Take, Get Clip Users, Cancel Motion Definition, Get Motion Activity and Migrate Motion Definitions.

### Changed
- The version this toolset reports to an agent is now read from the plugin's own
  descriptor rather than repeated in C++, so it can no longer answer a number the
  installed package does not carry.
- Generate Motions stops at review and its description says what it costs per provider; Update Motion Definition reports the settings it refused and changes only the sampler settings you set, where setting one used to reset the others; Estimate Download Cost prices fetching only.
- A pipeline option's range is text, empty where there is no bound, instead of zero.
- The skill states each provider's facts for that provider - it had presented Uthana's billing as
  everyone's - and covers review, provider settings, poses and first-time setup.

### Compatibility
- Needs MotionForge 0.4.0 or later.

## 0.2.1 — 2026-09-08
- Packaging fix: the release now carries everything the register allows. `BuildPlugin`'s filter excludes `Config/` and every `public_extra` path, so earlier zips shipped without them.
- `GetToolsetVersion()` answers this plugin's real version; it had drifted from the descriptor.

## 0.2.0 — 2026-08-29
- Apache-2.0, and a release publishes its source
- A window for a definition, and one take by default
- The prompt timeline opens with its motion on it
- Readiness and provider state become tools
- Every plugin descriptor agrees with its release tag, and says who made it

## 0.1.0 — 2026-08-28
- Initial commit: Colony prototype, plugins and plans
- Free local motion generation, proven end to end
- Provenance: a clip can say what made it
- Document provenance, and put the menu work on hold
- A prompt is a timeline, so author it on one
- Write down what today changed, and keep beats where they belong
- Output roots move; the things they read from stay where they are
- What was done to a clip, and where it puts its own limbs
- Quality becomes its own add-on, beside the providers rather than inside core
