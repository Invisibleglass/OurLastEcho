# Our Last Echo – Game Design Document

Game Design Document · first written 2026-09-23, updated 2026-10-21 to match the prototype (Milestones 1–8) · Andrew

**Status markers** (what the greybox prototype on `main` does today):
✅ built · 🔶 partly built, or built differently (see the note) · ⏳ planned

## Overview

Our Last Echo is an online two-player co-op adventure about a brother and his late sister, reunited in a Mongolian canyon where the living present and an ancient afterlife overlap.

**Logline:** A grieving brother walks into a canyon where he can hear his dead sister again. Together, across two eras of the same landscape, they uncover how she died.

| Item | Detail |
| --- | --- |
| Genre | Story-driven co-op action-adventure / puzzle-platformer |
| Players | 2, online co-op only (each player has a fixed character) |
| Camera | Third-person 3D |
| Engine | Unreal Engine 5.8 (the prototype is on 5.8.4; see Technical plan) |
| Platforms | PC, PlayStation, Xbox (the prototype runs on Windows PC only) |
| Length | 8–12 hours |
| Tone | Emotional and dramatic |
| Comparable titles | It Takes Two, Brothers: A Tale of Two Sons, Journey, Spiritfarer |

**What makes it different**

- Grief is the core theme, not a backdrop: the co-op itself is the siblings' last time together.
- Asymmetric vision: each player sees platforms, enemies and paths the other cannot.
- Cross-time cause and effect: what the sister does in the ancient past reshapes the brother's present.
- A setting rarely seen in games: Mongolian steppe, canyons, shamanic belief and nomadic culture.

## Story and characters

The story is a mystery told through grief: the siblings slowly piece together how the sister died, and the truth decides whether they can let each other go.

### Characters (names are placeholders)

| | Bat (brother) | Saraa (sister) |
| --- | --- | --- |
| Player | Player 1 (the host) | Player 2 |
| World | The canyon today (the living present) | The canyon's ancient past (the afterlife) |
| State | Alive, grieving, searching for answers | Recently died, newly arrived in the afterlife |
| Weapon | Spirit bow | Sword whip |
| Emotional arc | From guilt and denial to acceptance | From confusion and fear to peace |

**Bat** travels to the canyon after his sister's death. He hears her voice on the wind, then sees her ghost moving through the rock. He is the one who has to keep living.

**Saraa** wakes in the same canyon centuries earlier, among ancestral spirits. She does not remember how she died. Her missing memories return in fragments as the pair progress.

### Story arc

1. **Arrival.** Bat enters the canyon. Saraa wakes in the afterlife. They find they can hear and help each other.
2. **Hope.** They believe they may be able to bring her back, or at least stay together here.
3. **Unravelling.** Memory fragments reveal the days before her death. Tension grows between them.
4. **The truth.** The full cause of her death is revealed at the canyon's heart.
5. **Farewell.** The siblings face the choice to let each other go.

⏳ No story content is in the prototype yet (no cutscenes, dialogue or memory fragments).

### The mystery of her death

The cause of Saraa's death is still undecided and will be revealed across the game. Design rules for the reveal:

- Each chapter ends with a memory fragment, playable or shown as a cutscene.
- Early fragments should support more than one reading, so players form theories.
- Both players should hold different pieces, so they have to talk to solve it.

## The two worlds

Both players explore the same canyon, but centuries apart: Saraa's afterlife is the canyon as it was in ancient times, and Bat's world is the canyon today.

| | Present canyon (Bat) | Ancient canyon (Saraa) |
| --- | --- | --- |
| Look | Eroded, dry, abandoned; ruins, rusted fences, old Soviet-era structures | Lush, whole and sacred; rivers, standing stones, ovoo shrines, spirit camps |
| Inhabitants | Wildlife, weather, hostile spirits drawn to his grief (prototype: the Dust Wraith) | Ancestral spirits, lost souls, guardian beasts (prototype: the Shade, the Stone Guardian) |
| Light and sound | Harsh daylight, wind, silence | Dreamlike light, throat singing, drums |
| Feeling | Loneliness and loss | Wonder, with an undercurrent of danger |

✅ Both eras exist in the prototype as greybox: the same canyon shapes with each era's own colours, lighting, sky, fog and ambient sound, and each player sees only their own era. ⏳ The cultural props and structures in the table above aren't built yet.

### How the worlds interact

- 🔶 **Past changes present.** A tree Saraa plants becomes a climbable trunk for Bat. A dam she breaks carves a new path through his canyon. *(Built: the sapling. Saraa steps on the marked spot and a tree with climbable branches stands in Bat's present. The dam and other changes are planned.)*
- ⏳ **Present echoes into past.** Sounds Bat makes (horn calls, bells) can wake or distract spirits in Saraa's world.
- ✅ **Ghost overlap.** Each player sees the other as a translucent figure moving through their world.
- ⏳ **Thin places.** At ovoo shrines the worlds briefly align, allowing items or light to pass between them. These are also the natural spots for story scenes.

## Core co-op mechanics

Every puzzle and fight is built so neither sibling can succeed alone: each sees half the world and must trust the other for the rest.

### Asymmetric vision

- ✅ Some platforms, paths and enemies exist only in one world.
- ✅ Saraa may walk on spirit platforms Bat cannot see. From his view, she seems to walk on air, and he must follow her route by trusting her.
- ✅ **Echo platforms** (added in the prototype): Bat sees a dormant platform as a flickering outline and shoots it awake, and only then can Saraa stand on it (some stay awake only for a few seconds).
- ✅ Enemies only one sibling can see, touch and fight (the Dust Wraith for Bat, the Shade for Saraa; see Combat).
- ⏳ Bat sees hazards in the present (collapsing rock, predators) that Saraa cannot, and must warn her.

### Cross-time puzzles

- 🔶 **Cause and effect:** Saraa changes something in the past; Bat uses the result in the present. *(Built: the sapling.)*
- ⏳ **Relay puzzles:** Bat reads a clue in the ruins today; Saraa acts on it in the past.
- ⏳ **Synchronised actions:** both players trigger linked objects at the same moment across time.
- ✅ **Help across the divide** (added in the prototype): a switch only Saraa can press raises a bridge for Bat, and Bat's arrows give Saraa her swing points and platforms.

### Communication

- ✅ The siblings can talk freely; in-game voice chat is framed as their conversation. *(Epic voice chat, open mic or push-to-talk, with a speaking indicator.)*
- ⏳ A ping system (point and mark) supports players who don't use voice.
- ⏳ Characters also speak scripted and reactive lines, so the story carries even for quiet players. *(The subtitle settings exist; there are no lines yet.)*

### Failure and respawn

- 🔶 If one player falls, they reappear beside the other's ghost, so the pair always stays together. *(Built for combat: a player knocked down in a fight gets back up beside their partner after a few seconds, and if both are down the fight restarts from its checkpoint. A fall off the level still returns the player to their last checkpoint.)*
- ✅ Checkpoints are frequent, to keep emotional scenes from being broken by repeated failure. Some checkpoints apply to only one sibling, so each has a respawn on their own route.

## Weapons and abilities

Saraa's sword whip handles close-range traversal and combat, while Bat's spirit bow reaches across distance and across worlds. Both siblings always hold their weapon: Bat carries the bow in his left hand and raises it to aim, and Saraa holds the sword in her right hand.

| | Sword whip (Saraa) | Spirit bow (Bat) |
| --- | --- | --- |
| Range | Close to mid | Long |
| Traversal | Swing from anchor points, pull levers, grapple across gaps | Shoot distant targets to raise or reveal platforms |
| Combat | Sweeping strikes, disarm, pull enemies closer | Shoot enemies only Saraa can see, from afar |
| Cultural root | Herder's whip and the uurga lasso pole | Mongolian composite horse bow |

### Sword whip

- ✅ **Swing:** latch onto spirit anchors and swing across chasms, chaining from one anchor to the next. The whip holds from the moment it latches, so she never drops into slack rope.
- ✅ **Lash:** fast combo attacks; the blade segments extend on a charged hit. *(Built: a 3-lash combo whose third lash sweeps a wide arc, and a charged lash, held then released, that reaches much further and hits harder.)*
- ⏳ **Pull:** drag objects, switches or smaller enemies toward her.
- ✅ **Focus** (added in the prototype): holding the right mouse button or left trigger moves her camera in over her shoulder, a gentler version of Bat's aim.

### Spirit bow

- ✅ **Aim and shots** (added detail): a quick shot without aiming (fast, weak), an aimed shot over the shoulder, and a charged shot (hold while aiming; bigger, faster, much stronger).
- ✅ **Anchor shot:** an arrow that crosses into Saraa's world and becomes a swing point for her whip. *(Only certain surfaces hold an anchor; Bat can keep two at a time.)*
- 🔶 **Echo shot:** fires at an enemy he can only see as a shimmer, guided by Saraa's callouts. *(Not built as described: today Bat can't see or hit Saraa-only enemies at all. The teamwork in combat comes from the Stone Guardian and the Sky Talon instead.)*
- 🔶 **Platform shot:** hits a far target to raise, freeze or reveal platforms. *(Built: waking echo platforms for Saraa, some only for a few seconds. Freezing is planned.)*

### Combined moves

- ✅ Bat fires an anchor shot; Saraa swings from it across a gap only she can cross. *(The Crossing.)*
- ✅ **Take-turns enemies** (built in place of "pull and finish"): Saraa's whip cracks the Stone Guardian's shell so Bat's arrows can hurt it, and Bat's arrow knocks the Sky Talon out of the air so Saraa's whip can hurt it. Each opening lasts a few seconds, with a glow and a timer that fades.
- ⏳ Saraa pulls an enemy into the open; Bat finishes it with a charged shot. *(Needs the whip's Pull.)*
- ⏳ Both lock onto a boss weak point at once for a synchronised strike.

### Progression

⏳ Each chapter unlocks one new ability per weapon, tied to a story beat or memory fragment. *(In the prototype every ability is available from the start.)*

## Combat

Fights follow the same rule as everything else: each sibling sees and can hurt only part of what attacks them, and the hardest enemies need both. *(This section was added after Milestone 8; everything in it is ✅ built and playable in the CombatTest arena.)*

### Both siblings

- **Health** with an on-screen bar for yourself and your partner, a red screen flash, a camera shake and a flinch when hurt.
- **Dodge roll** (Left Shift / gamepad B) with a moment of invulnerability and a short cooldown.
- **Downed** at zero health: the player can't move or attack and a countdown shows, then they get back up beside their partner. If both are down, the fight restarts from its checkpoint.
- **Feedback:** a hit flash, a short hit-stop on heavy hits, sound cues, and a spark and clink when a hit does nothing.

### Enemies

| Enemy | Who sees and fights it | How to beat it | Attack |
| --- | --- | --- | --- |
| **Dust Wraith** | Bat only (it lives in the present) | Bat's arrows | Drifts in and claws |
| **Shade** | Saraa only (it lives in the past) | Saraa's whip | Closes in and strikes |
| **Stone Guardian** | Both | Arrows bounce off its stone shell. Saraa's whip cracks the shell open for a few seconds, and only then can Bat's arrows hurt it. | A slow, heavy slam |
| **Sky Talon** | Both | It flies out of the whip's reach. Bat's arrow knocks it down and stuns it for a few seconds, and only then can Saraa's whip hurt it. | Screeches, then dives |

Enemies choose only targets in their own world, and every attack is telegraphed (a wind-up with a glow and a warning sound) so players can dodge. Health bars float over the enemies each player can see.

### The CombatTest arena (prototype)

A separate greybox arena reached through a "Combat Test" door near the start of the first level; either sibling walking in takes both. Four waves: Dust Wraiths and Shades, then Stone Guardians, then Sky Talons, then one of each. A checkpoint at the entrance, a restart button, and a return door. It's a testing ground: in the real game these fights belong inside the chapters.

## Chapter outline

Seven chapters of roughly 60–90 minutes each fill the 8–12 hour target, moving deeper into the canyon as the truth comes closer.

| # | Chapter | Setting | New mechanic | Story beat |
| --- | --- | --- | --- | --- |
| 1 | The Wind Voice | Canyon mouth | Hearing and seeing each other; basic whip swing and bow shot | Bat hears Saraa. First reunion. |
| 2 | The Dry River | Old riverbed | Cause and effect: Saraa redirects the river | They hope she can come back. First memory fragment. |
| 3 | Camp of Ancestors | Nomad spirit camp | Relay puzzles; echo shot | Ancestors warn Saraa she cannot stay. |
| 4 | Stone Watchers | Standing stones and petroglyphs | Synchronised actions; anchor shot | Memories show tension before her death. |
| 5 | The Storm | Collapsing canyon, sandstorm | Hazards only Bat can see | The siblings argue. The worlds start to tear. |
| 6 | The Heart of the Canyon | Deepest point, where she died | Everything combined | The full truth is revealed. |
| 7 | Farewell | Sacred ovoo at the summit | Both worlds merge briefly | The final choice to let go. |

Each chapter ends with a boss or set-piece that requires both weapons, followed by a memory fragment.

⏳ None of the chapters are built yet. The prototype is one greybox canyon level, **the Spirit Path**, with three sections (the opening spirit-platform path, **The Climb** with echo platforms, and **The Crossing** with anchor swings), plus the CombatTest arena. It tries out mechanics from several chapters at once: the anchor shot, for example, is planned for chapter 4 but is already in the prototype.

## Art direction, audio and tone

The game should feel like a quiet, beautiful place to grieve: vast landscapes, small figures and music that carries the emotion.

- **Visual contrast:** the present uses muted ochre, dust and cold blue skies. The past uses rich greens, deep blues and warm spirit light. *(🔶 The greybox eras already use this split of colour and light; there's no final art yet.)*
- **The ghost look:** each sibling appears to the other as a soft, wind-blown silhouette, clearer at thin places. *(🔶 Each sees the other as a translucent figure; the wind-blown look and thin places are planned.)*
- **Cultural detail:** ger tents, deel robes, prayer scarves (khadag), petroglyphs and ovoo cairns. Work with Mongolian cultural consultants for accuracy and respect. *(⏳)*
- **Music:** morin khuur (horsehead fiddle), throat singing and sparse piano. A shared theme for the siblings that evolves by chapter. *(⏳ The prototype uses placeholder sounds.)*
- **Sound as story:** wind carries Saraa's voice in early chapters before she fully appears. *(⏳)*
- **Tone:** dramatic and sincere, with small moments of sibling warmth and shared memories to keep it from feeling heavy all the time.

## Technical plan

Build on Unreal Engine 5.8, the latest release (June 2026), which Epic says is the last planned major UE5 version while it works on UE6 ([source](https://www.unrealengine.com/news/unreal-engine-5-8-is-now-available)). The prototype is on **5.8.4**. Everyone on the team needs the same engine version, because two different builds can't play together online.

### Useful UE 5.8 features for this game

| Feature | Status in 5.8 | Use in this game | In the prototype |
| --- | --- | --- | --- |
| Mesh Terrain | Experimental | True 3D terrain for canyon overhangs, tunnels and floating spirit islands | Not used: standard Landscape for the canyon floor, with greybox walls |
| Procedural Vegetation Editor | Experimental | Growing the lush ancient canyon, and trees Saraa plants | Not used yet |
| PCG framework | Available | Scattering rocks, ruins and props across both eras | ✅ Canyon rocks |
| MegaLights | Production-ready | Spirit light and glowing arrows with many dynamic lights | Not used yet |
| Fog Screen Space Scattering | Experimental | Dust, sandstorms and ghostly haze | Not used yet (standard height fog) |
| Lumen / Lumen Lite | Available | Global illumination; Lite mode for lower-end hardware | ✅ Lumen |

Experimental features can change or break, so prototype with them early and keep a fallback (for example, standard Landscape instead of Mesh Terrain).

### Two worlds in one level

- ✅ Build both eras as overlapping layers of one level, so the players' positions line up exactly. *(Built as a shared layer, which holds all gameplay and all collision, plus a Present and a Past visual layer. Nothing in an era layer can be touched; anything only one sibling can touch is a filtered object in the shared layer.)*
- ✅ Each player's client shows only their own era, plus the other player's ghost. *(Each machine loads only its own player's era layer.)*
- ✅ Cross-time objects (the planted tree, the redirected river) are networked states: a change in the past flips the matching object in the present. *(The planted tree works this way.)*

### Online co-op

- ✅ Use Unreal's built-in replication, with one player hosting (listen server). The host plays Bat.
- 🔶 Use Epic Online Services for crossplay, friends, invites and voice chat across PC and consoles. *(Built on PC: Epic sign-in, invites, 6-character join codes, voice chat, and clear messages when a player leaves or the connection drops. A LAN mode remains for testing. Consoles and crossplay are planned.)*
- ⏳ Consider a friend's pass, as It Takes Two did, so one copy lets a friend join.

### Platforms

PC, PlayStation 5 and Xbox Series X/S. Console releases need developer program approval from Sony and Microsoft; apply early. *(The prototype is packaged for Windows only.)*

### Future-proofing

UE6 early access is expected by the end of 2027, per State of Unreal 2026 coverage. Stay on UE 5.8 for this project rather than planning a mid-development engine switch.

## Prototype milestones (briefed so far)

These milestones were briefed for the greybox prototype. All eight are built and merged into `main`. Check NOTES.md, TEST_REPORT.md and the Git history for what was actually built and what changed.

| # | Milestone | Summary |
| --- | --- | --- |
| 1 | The Spirit Path | Two characters (Bat, Saraa), spirit platforms only Saraa can see and stand on, a switch that raises a bridge for Bat, shared end zone. |
| 2 | Canyon greybox | Tested Milestone 1, then built a greybox canyon around the play area with lighting, fog, boundaries and a kill volume. |
| 3 | Spirit bow and echo platforms | Bat's bow (aim, fire); echo platforms Bat sees and shoots to make solid for Saraa; "The Climb" section. |
| 4 | Sword whip and anchor arrows | Bat's arrows stick into anchor targets as swing points; Saraa's whip latches, swings and chains; "The Crossing" section. Anchorable surfaces are defined in one place so they can later become a specific physical material. |
| 5 | Title screen, menus and settings | Title scene with Bat seated (future: picking leaves), main menu, host/join lobby, full settings, credits, in-game pause menu. |
| 6 | Two eras | Shared gameplay layer (all collision) plus Past and Present visual layers, each loaded locally for the right player. Stretch goal: the sapling Saraa plants becomes a climbable tree in Bat's present. |
| 7 | Online with EOS | Epic Online Services login, sessions, invites, join codes, voice chat, disconnect handling, packaged builds, manual test checklist. |
| 8 | First combat (CombatTest level) | Health and damage, dodge roll, downed and respawn beside the partner. Bat's quick, aimed and charged shots; Saraa's lash combo and charged lash. Separate CombatTest arena with four waves, reached by a door in the first level. Four enemies: Dust Wraith (Bat only), Shade (Saraa only), Stone Guardian (Saraa must hit first, then Bat can damage it), Sky Talon (Bat must hit first, then Saraa can damage it). After playtests: both weapons always held in hand, and Saraa's focus zoom. |

## Open questions

- [ ] How did Saraa die, and what role did Bat play, if any?
- [ ] Does the ending let players choose, or is the farewell fixed?
- [ ] Final names for the siblings and the game.
- [ ] Which real Mongolian region inspires the canyon (for example, the Gobi's Yolyn Am or Tsagaan Suvarga)?
- [ ] Should there be a second weapon or tool per sibling later in the game?
- [ ] Team size, budget and target release window.
- [ ] Should a fall off the level also put the player back beside their partner, like being downed in a fight, instead of at a checkpoint?
- [ ] The echo shot: should Bat be able to see Saraa-only enemies as a shimmer and shoot them with her callouts, as first planned, or do the take-turns enemies (Stone Guardian, Sky Talon) cover that role?
- [ ] Bat has no aim animation yet, so his bow arm stays down while aiming. Animations (aiming, attacks, the roll) are needed before final art.

## Sources

- [Unreal Engine 5.8 is now available](https://www.unrealengine.com/news/unreal-engine-5-8-is-now-available), Epic Games, June 23, 2026
- [State of Unreal 2026: Top news about Unreal Engine](https://www.therookies.co/blog/headlines/state-of-unreal-2026-top-news-about-unreal-engine), The Rookies, June 19, 2026
