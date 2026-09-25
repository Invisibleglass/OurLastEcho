# Milestone 4 – The sword whip and anchor arrows

Branch: `milestone-4-sword-whip`, branched from `main` after Milestone 3 was merged. Built with the editor open, driven through its MCP server.

**New mechanic:** when Bat shoots an anchor target, his arrow sticks in it and becomes a glowing anchor point. Saraa latches onto it with her sword whip and swings from it.

## What was built

**Anchor arrows** (Bat's same bow and arrow; what happens depends on what the arrow hits):
- **Echo platform:** wakes it, as before.
- **Anchorable surface:** the arrow sticks and becomes an **anchor point** (`AEchoAnchorPoint`, spawned and replicated by the server).
- **Anything else:** nothing happens; the arrow stays stuck for 2.5 s, then disappears.
- **"Can this surface hold an anchor?"** is a single function, **`UEchoAnchorRules::CanHoldAnchor(Hit)`** (`EchoAnchorable.h`). The arrow never checks anything else. Today it looks for a `UEchoAnchorableComponent` on the hit actor. Arrow sweeps already return the physical material (`bReturnMaterialOnMove`), so switching to "a certain rock or wood" later means changing only that function; a commented example is in it.
- **Anchor targets** (`AEchoAnchorTarget`): a round, ringed board with the Anchorable component. Only Bat sees it (present-day wood; Saraa sees it only with `EchoShowAllPlatforms` on). Its board blocks only the arrow channel.
- **What each player sees:** Bat sees his arrow stuck in the target. Saraa sees a glowing blue orb with a faint halo, 35 cm off the surface. That's the point her whip latches onto.
- **Max 2 anchors** (`MaxActiveAnchors` on the SpiritBow component). A third removes the oldest. If Saraa is hanging from the removed one, she drops on both machines.
- **Networking:** the anchor's position and surface replicate once, so both machines agree to the millimetre (tested).

**The sword whip** (`UEchoSwordWhipComponent`, on every character; only a Spirit-realm character can use it):
- **Targeting:** every frame, on Saraa's own machine. The nearest anchor within **whip range (18 m)**, within **40°** of the camera's view and in clear line of sight gets highlighted: the orb swells and brightens, and blue brackets appear around it on her screen.
- **Latch:** hold the whip button: **left mouse / E / gamepad right trigger** (`IA_Whip` in `IMC_Whip`, from `Scripts/build_sword_whip.py`). The whip line lashes out to the anchor in 0.12 s, with a snap sound and a flash on both machines.
- **Swing:** a pendulum under the anchor.
  - Physics: gravity (×1.4) with the rope as a maximum length. A slack rope is free fall, and when it goes taut she keeps only her speed along the swing.
  - Steering: a little air control from the move stick. She faces the way she swings.
  - Momentum is kept, capped at 16 m/s.
  - Walkable ground: touching it ends the swing. Walls: she scrapes along them.
- **Release:** let go of the whip button, **or press jump**. She launches with her momentum plus a small forward and upward boost. Her speed isn't braked in the air until she lands (the template's 15 m/s² falling braking otherwise killed the launch).
- **Chain:** press the whip again mid-air to latch onto the next anchor. The anchor she just left can't be re-targeted for 0.5 s.
- **Bat can't use it:** no mesh, no input, and his movement refuses to swing.
- **Placeholder look:**
  - Sword: a 4-part sword from engine shapes on her left hip, raised at her right shoulder and pointing at the anchor while she swings.
  - Line: a stretched thin cylinder (a greybox "rope", no cable plugin), visible to both players.

**Networking the swing:** the swing is a **custom movement mode** in a new `UEchoCharacterMovementComponent`, which both characters now use. It runs inside Unreal's normal server-authoritative movement with client prediction.
- Saraa's machine swings immediately from her input.
- Each move sent to the server carries the whip state (a move flag, the anchor point and the rope length), so the server replays exactly the same latch.
- The server corrects her only if the two disagree.

Result: **0 corrections** in every swing, including at 105 ms ping with 30 ms jitter and 5% packet loss, and with Saraa as the host. Details are in TEST_REPORT.md.

**New level section, "The Crossing"** (`Scripts/build_crossing.py`, tag `CrossingBuilder`), straight after The Climb:

| | |
|---|---|
| **Chasm** | 71 m long, wall to wall, 14 m deep (stations 7850–15000). It's cut into the landscape by `build_canyon.py`, with a kill volume inside. |
| **Swing 1** (single, teaches the basics) | From The Climb's ledge (7.4 m up), under target 1 on an overhang, to pillar 1. |
| **Swing 2** (chained, teaches the 2-anchor limit) | From pillar 1, under targets 2 and 3 on the underside of a 22 m-deep rock arch, to pillar 2. Both anchors must exist before she jumps. One arch anchor alone can't reach pillar 2, however she times the release (checked by the test). The arch's rock "curtain" at its back edge blocks the big high flings a single anchor would need. |
| **Swing 3** (new + old mechanics) | From pillar 2, under target 4 on another overhang, onto an **echo platform** Bat must wake. From it she steps up 90 cm through **her doorway** in a gate wall onto the far rim. |
| **Bat's route** | A rock shelf along the right wall, with a ramp up at the start, to **his own doorway** in the gate wall. A 10 m gap near the end is spanned by a **drawbridge** that **Saraa's switch** on the far rim lowers. |
| **Bat's shooting spots** | Target 1 from the start area or the ledge. The arch targets from his shelf underneath them. **Target 4 and the echo platform only from further along his shelf:** the arch's back curtain hides target 4 from the start area (tested), and the echo platform is beyond bow range from there. |
| **Respawn** | Falling into the chasm sends Saraa back to the start of it, on The Climb's ledge. Bat goes back to the start of his shelf. These are realm-specific checkpoints (`AEchoCheckpoint` gained `bOnlyOneRealm` / `OnlyRealm`). Bat's anchors stay where they are. |
| **End zone** | Moved to the far rim, past the gate. |

**Stretch goal: the whip lash.**
- Pressing the whip button with no anchor targeted cracks the whip forward instead.
- The server sweeps along the lash (4.5 m reach, 0.6 s cooldown).
- A **training dummy** (`AEchoTrainingDummy`) on the far rim takes the hit. The hit count is replicated, and both players see it wobble and hear it.

**Tuning in Blueprint:**
- **BP_Saraa → SwordWhip:** whip range, targeting angle, re-latch delay, max swing speed ("swing speed"), latch boost, gravity during swing, air control, min rope length, ground-latch hop, release boost (forward and up), lash range/radius/duration/cooldown, look and sounds.
- **BP_ThirdPersonCharacter → SpiritBow:** `MaxActiveAnchors` and `AnchorClass`.

**Debug:** type **`EchoWhipDebug`** in the console. It draws:
- the whip range sphere and targeting cone
- every anchor target (orange circle) and anchor (yellow = targeted, green = in range, red = out of range)
- while swinging: the rope, the arc so far, and the predicted arc from here
- before latching: where a latch now would swing her

It's per machine, and a line on screen says it's on.

**Choices where the brief was open** (simplest option, as asked):
- **Hold to swing.** The whip button is held while swinging, and letting go (or jumping) releases. Chaining means press, release, press.
- **Anchor targets are Bat-only.** They're present-day objects, like the canyon echo outlines. Saraa sees the anchors he makes, not the boards.
- **A new movement component, not a physics constraint,** so the swing gets the engine's client prediction for free.
- **The whip line is a stretched cylinder,** not the Cable Component plugin. It's always taut while swinging anyway.
- **The chasm is cut into the landscape.** `build_canyon.py` was re-run with the chasm added. It's deterministic, so every other canyon piece came back identical. `test_canyon_live.py` passes, with two floor-check spots moved out of the chasm.
- **`bHostPlaysSaraa` / `Echo.HostPlaysSaraa`:** a game-mode switch and console variable that swap roles, so Saraa can be tested as the host.

## How to test

**Just play:** Play (2 players, Listen Server). Do the Spirit Path and The Climb (or drop in with the debug view), then continue past the ledge:
1. **Bat:** from the ledge or the floor, shoot the target under the overhang ahead. **Saraa:** a blue orb appears. Jump off the ledge toward it, **hold LMB/E/RT** while its brackets show, swing, let go over the pillar.
2. **Bat:** walk down and up the ramp onto the shelf along the right wall. Standing under the arch, shoot both targets on its underside (the first anchor disappears: only two at a time). **Saraa:** jump off the pillar, latch the first arch anchor, let go near the top of the swing, latch the second, let go over pillar 2.
3. **Bat:** further along the shelf, shoot the overhang target and the gold outline in front of the gate wall's small doorway. **Saraa:** swing onto the blue platform, jump up through the doorway, drop down and step on the amber switch.
4. **Bat:** cross the lowered drawbridge and go through his doorway. Both stand in the green end zone.
5. **Saraa:** press the whip button with no anchor in view (or near the dummy) to lash the training dummy.

**Automated (editor open):**
- **Run it:** start a fresh 2-player PIE session, then run `py exec(open(r'D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho/Scripts/test_crossing_live.py').read())`. That's 68 `ECHO_CROSS_TEST` checks, last line `PASS`/`FAIL`, about 90 s.
- **Options:** set these before exec'ing, e.g. `py ECHO_CROSS_MODE="host_saraa"`.
  - `ECHO_CROSS_MODE`: `swing1`, `chain`, `swing3`, `host_saraa`
  - `ECHO_CROSS_NETEMU=["NetEmulation.PktLag 100", "NetEmulation.PktLagVariance 30", "NetEmulation.PktLoss 5"]`
  - `ECHO_CROSS_TRACE=True`
- **Saraa as host:** type `Echo.HostPlaysSaraa 1` in the console before starting PIE, then use mode `host_saraa`. Set it back to 0 afterwards.
- **Real swings:** on Saraa's own machine the test holds movement input, jumps, aims the camera, presses the whip, and lets go when the predicted landing is on the target, like a player. Every landing is checked on the server.
- **Frame rate:** before running, turn off *Use Less CPU when in Background* (Editor Preferences → Performance, or the MCP `ObjectTools.set_properties` call in CLAUDE.md). It resets on every editor start. Otherwise a background editor runs at ~3 fps with two PIE worlds, and scripted input that coarse isn't meaningful.

**Rebuilding** (resets hand edits to those actors):
1. `build_sword_whip.py`: assets; can run headless.
2. `build_canyon.py`: the chasm; open editor only.
3. `build_climb.py`.
4. `build_crossing.py`: moves the end zone to the far rim, so run it after `build_climb.py`.
5. `build_canyon_rocks.py`.

## Results

**All suites pass:**

| Suite | Checks |
|---|---|
| Milestone 1 | 29/29 |
| Milestone 2 | 34/34 |
| Milestone 3 | 60/60 |
| Settings menu | 17/17 |
| The Crossing | 68/68 |
| The Crossing with Saraa as host | 39/39 |

Editor and game builds: 0 warnings. Details are in TEST_REPORT.md.

**Bugs and design problems the tests caught (all fixed):**
1. **Swings drifted sideways off the pillars** in the first runs. The test steered along the canyon's centreline, but the left wall bends 12° away from it there. It now steers along Saraa's line, like a player.
2. **The arch's front rock curtain blocked Saraa's own view** of the first arch anchor from pillar 1, so her whip had nothing to latch onto. It was removed; the back curtain stays.
3. **Target 3 sat right where the first arch swing tops out,** so the second swing had no arc. The arch was lengthened, target 3 moved 6 m on, pillar 2 moved back, and target 4 moved out.
4. **Momentum was lost on release.** The template's falling braking cut her to walking speed in a fraction of a second. Flights from a swing are no longer braked.
5. **Too much energy:** pumping with the stick (air control 700) plus a 26 m/s cap let one arch anchor fling her past the chained swing. Retuned to air control 300, cap 16 m/s, boosts 250, with the arch curtain as a physical block. The test proves no single-anchor release reaches pillar 2 (best: 1.9 m short).
6. **Bat stuck in his doorway:** the far rim's slope is ~70 cm higher than the shelf there. The shelf and sill were raised to that ground, with a ramp at the start.
7. **Engine crash (not ours):** one editor crash in the engine's derived-data cache HTTP code while PIE started. Restarting fixed it.

## Known issues

- **Needs a manual check: real controls and feel.**
  - Whip input is verified: IA_Whip is on LMB, E and the right trigger, and the component binds it. Every path is driven through the same functions the bindings call.
  - Nobody has pressed a real button yet. Worth feeling: the 40° targeting cone (anchors are high above, so she may need to look up; widen `TargetingAngle` if it's fiddly), swing speed, and release timing.
- **Needs a manual check: the lash from Saraa's client.** The lash is a server RPC from her machine, which editor Python can't send (same reason as Milestone 3's debug command). The test drives it on the server's copy of her, which covers the hit, replication and cooldown. Pressing the button in Saraa's window once confirms the RPC.
- **How Saraa's swing looks on Bat's screen** hasn't been measured. It uses the engine's standard smoothing for other players' characters, which extrapolates in straight lines between updates. At high ping her arc may look slightly less round to Bat than to her.
- **A perfect fling might skip the echo platform.** Her doorway's sill is 90 cm above the platform, so she'd have to fly into a 3 × 2.6 m window. Not seen in testing, but not impossible.
- **The whip line can pass through the arch's back curtain** for a moment during the second arch swing. It's only a visual; nothing collides with the line.
- **Arch targets can also be shot from the start area** (only target 4 and the echo platform need the shelf). A front curtain would fix it, but it blocked Saraa (see *Results*).
- **Colours:** the whip line was toned down (glow 1.4 → 0.8) after a screenshot showed it near-white in the canyon's auto-exposure. It still reads very pale. Tune `MI_WhipLine` / `MI_SpiritAnchor` by eye.
- **Per-player volume** (from Milestone 3) is still a to-do.
- **Two editor settings reset on every start:** frame rate with the editor in the background (see *How to test*), and the Lumen warning shown in PIE windows.

## Suggestions for Milestone 5

- **Swing animation:** a hanging pose and arm raised to the whip (Control Rig or a simple montage), a whip-crack effect, and a rope that sags when slack (Cable Component, or a few sagging segments).
- **Readability:** a faint trajectory preview while holding the whip button over a gap (the debug arc, made pretty), and a subtle ring showing whip range on anchors in view.
- **Anchor variety:** anchors that break after one swing, moving anchors (a swinging log Bat shoots), and timed anchors (fading like timed echo platforms).
- **Physical-material anchors:** switch `CanHoldAnchor` to a "Soft Rock" or "Old Wood" physical material and paint whole walls anchorable instead of placing boards.
- **Combat groundwork:** turn the training-dummy hit into a small damage interface (`IEchoLashable`) and add a first spirit enemy that only Saraa can hit and only Bat can see coming.
- **Co-op communication:** a ping/marker, now more needed, since Bat has to tell Saraa where he's putting anchors and she can't see the targets.

---
# Milestone 3 – The spirit bow and echo platforms

Branch: `milestone-3-spirit-bow`, branched from `main` after Milestone 2 was merged. Built with the editor open, driving it through its MCP server.

**New mechanic:** Bat sees faint gold "echo platforms" that Saraa can't. When he shoots one with his spirit bow, it becomes solid and blue for her, so she can climb it.

## What was built

**The spirit bow** (`UEchoSpiritBowComponent`; every character has one, but only a Living-realm character can use it):
- **Aim:** hold **right mouse / left trigger**. The camera pulls in over the shoulder (400 → 160 cm, FOV 70), a gold reticle appears, Bat faces where he aims, and he slows to 2.2 m/s.
- **Fire:** **left mouse / right trigger** while aiming. The **server** spawns a replicated glowing arrow (`AEchoArrow`) with a point light and a tapering streak trail. It flies on a slight arc, and the launch angle is corrected for the arc so it lands where the reticle points. Unlimited arrows, 0.6 s cooldown.
- **Saraa can't use it:** she gets no bow mesh, her input isn't bound and the server rejects her shots.
- **Controls:** Enhanced Input assets `IA_Aim`, `IA_Fire` and `IMC_Bow`, made by `Scripts/build_spirit_bow.py`. The component adds `IMC_Bow` on top of the template's controls.
- **Placeholder bow:** built from 6 engine shapes. It's slung across Bat's back, and held upright in front of him while aiming. The template has no aim animation; attached to his hand, the bow just dangled at his hip.
- **Tuning** (select the **SpiritBow** component on `BP_ThirdPersonCharacter`): arrow speed, arc (gravity scale), range, cooldown, muzzle offset, aim camera distance/offset/FOV, blend speed, aim walk speed, and the bow's back and aim placements.
- **New collision channel:** `EchoArrow` (`ECC_GameTraceChannel2`, default Block). Walls, rocks and the landscape stop arrows; characters don't.

**Echo platforms** (`AEchoPlatform`):

| | Bat sees | Saraa sees | Who can stand on it |
|---|---|---|---|
| Dormant | faint **flickering gold outline** | nothing | nobody |
| Awake | a steady, fainter outline | a **blue** slab (her spirit style) | Saraa only |

- **When an arrow hits** (a hit box that blocks only the `EchoArrow` channel): the platform wakes on the server, and both machines play a blue flash light, a material glow burst and a sound cue.
- **Stay awake or timed:** each platform has **`bTimed`** (default off = stays awake) and **`AwakeDuration`**. A timed platform's slab blinks for Saraa in its last 2.5 s, then it goes dormant with a sound. Shooting it again restarts the timer.
- **Networking:** the state is server-authoritative and replicated (`bAwake`, a wake counter for the cue, and the server time a timed platform expires). Visibility is decided locally per machine, like the Milestone 1 spirit platforms.

**The Climb** (`Scripts/build_climb.py`, actors tagged `ClimbBuilder`), on the left canyon wall just past the bridge's far side:
- **7 echo platforms**, 90 cm higher each (the jump apex is about 128 cm) and 4.3 m apart centre to centre. They're placed by measured distance because the wall curves.
- **P3 is timed (7 s)**, so Bat has to wake it just before Saraa needs it.
- **P6 and P7 are behind a tall rock screen.** From the open canyon floor, Bat's shots hit the screen. He has to walk into the gap between the screen and the wall (the "specific spot") to see and shoot them.
- **The ledge** is 7.2 m up. Saraa's switch on it swings down a hinged ramp for Bat (`AEchoRisingBridge` can now hinge and swing, not just rise). **The end zone moved onto the ledge**, so both players finish there together.
- **A checkpoint** (`AEchoCheckpoint`) at the base: after reaching it, falling into any kill volume respawns you there. A fall from the platforms lands at the base anyway. Platforms stay awake, except the timed one.
- **The rock scatter** has an extra exclusion zone, so no boulders sit in the climb.

**Debug:** type **`EchoShowAllPlatforms`** in the console (backtick key) to toggle every spirit and echo platform visible to both players. It's replicated through the game state, and a line on screen says it's on.

**Settings / pause menu** (added after the milestone, before merging):
- **Opening it:** **Esc**, **P** or **gamepad Start** (in PIE Esc stops the session, so use **P**). The console command `EchoMenu` also toggles it.
- **Pausing:** opening it pauses the game for **both** players. The other player's screen dims with "Paused - Bat is in the settings menu". Either player can open theirs while paused, and the game only resumes once everyone who opened it has closed it (Resume button, the same key, or gamepad B).
- **Volume:** a **sound volume** slider. Each player's volume is their own, applied to their machine's audio device and saved in `GameUserSettings.ini`.
- **Quit game** button.
- **How it's built:**
  - Logic in C++: `AOurLastEchoPlayerController`, `AEchoGameMode` (pause bookkeeping, using the engine's own pause, which replicates), `UEchoSettingsMenu` and `UEchoAudioSettings`.
  - Layout: `/Game/Echo/UI/WBP_SettingsMenu`, built with the MCP UMG toolset by `Scripts/build_settings_menu_widget.mcp.py`. Restyle it freely, but keep the widget names `MasterVolumeSlider`, `MasterVolumeText`, `ResumeButton` and `QuitButton`.
  - Input: `IA_Menu` (triggers while paused) and `IMC_Menu`, from `Scripts/build_settings_menu.py`.
- **One bug found while building it:** the other player's world didn't pause. While paused, the server's clock stops, so the world settings (which carry the pause) never came due for a network update. They're now pushed out immediately on every pause and resume.
## How to test

**Just play:** press Play (2 players, Listen Server). Cross the Spirit Path as before, then turn left at the far side:
1. As **Bat**, hold RMB and shoot the gold outlines on the left wall. Saraa should see each one turn blue.
2. Shoot **P3** (the timed one) right before Saraa jumps to it.
3. For **P6 and P7**, walk into the gap behind the tall rock screen.
4. As **Saraa**, climb to the ledge and step on the amber switch. The ramp swings down.
5. **Bat** walks up the ramp, and both stand in the green end zone.

**Automated (editor open):** start a fresh 2-player PIE session, then run in the console:
```
py exec(open(r'D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho/Scripts/test_climb_live.py').read())
```
That's 60 `ECHO_CLIMB_TEST` checks, ending in `PASS`/`FAIL`, and it takes about a minute. Saraa's climb uses **real jumps**: the test holds movement input on her client and presses jump near each platform edge, and the server has to agree where she lands. Run `test_pie_live.py` (Milestone 1) and `test_canyon_live.py` (Milestone 2) the same way, each in its own fresh PIE session.

**Rebuilding** (resets hand edits to those actors): `build_spirit_bow.py` (assets, can run headless), `build_climb.py`, then `build_canyon_rocks.py`.

## Results

- All three live suites pass: **Milestone 1 29/29, Milestone 2 34/34, Milestone 3 60/60**. Both builds (editor and game targets) have 0 warnings. Details are in TEST_REPORT.md.
- The visual check in PIE showed Bat's over-the-shoulder aim, the reticle, the arrow's arc trail, the gold outlines behind the rock screen, and Saraa standing on a blue awake platform.

**Bugs the tests caught (all fixed):**
1. **Hops came out 5–6 m apart.** Platforms were spaced along the canyon centreline, and the curving wall stretched them. They're now placed by measured distance.
2. **A 59 cm step at the top of the ramp,** too high to walk up (the maximum is 45 cm). The ledge is a straight block on a curving wall, so its edge sat about 1 m past the ramp's hinge. The hinge is now placed on the block's actual face.
3. **Colours washed out:** the bow looked white, the outline cream and the spirit platforms white. Linear colours are brighter than they look, and the canyon's auto-exposure clips glow. All were retuned. The Milestone 1 spirit platforms share the blue material and now read blue too.

**Menu test:** `Scripts/test_menu_live.py` (fresh 2-player PIE) passes, 17/17. It checks that opening pauses both machines, that movement and a timed platform's countdown freeze, that the game stays paused until both players close their menus, that both resume, and that the volume is saved and clamped.
## Known issues

- **Needs a manual check: real controls and feel.** The input assets and bindings are verified, and every aim and fire path is tested by calling it directly, but nobody has pressed a real mouse button or trigger yet. Also worth checking: jump spacing, the timed platform's 7 s, and camera feel.
- **Needs a manual check: `EchoShowAllPlatforms` typed on Saraa's machine.** Typed on Bat's side it works end to end. From Saraa's side it's a standard server RPC on her own character, but Python can't test that: editor Python makes every call run locally. The engine confirms this: `AActor::GetFunctionCallspace` returns local while `GAllowActorScriptExecutionInEditor` is set. Press the backtick key in Saraa's window and type it once to confirm.
- **Needs a manual check: the menu from real keys and on Saraa's side.** Pressing P/Esc/Start, dragging the slider and hearing the change haven't been done by a person. Saraa opening the menu goes through a server RPC, which editor Python can't exercise. Her side of the pause logic is tested on the server.- **To do later: each player must control their own volume, not have it synced.** On two real machines this already holds: the value is saved in each machine's own `GameUserSettings.ini` and applied to that machine's audio device. In 2-player PIE, though, both players run in one editor process, so they share one config file and the slider effectively moves both. Later, key the setting per player (e.g. a save slot per local player / PIE instance), make sure each PIE instance gets its own audio device, and test that one player's slider leaves the other's volume alone.- **Frame rate** still can't be measured with the editor in the background (see Milestone 2). Please check `stat fps` with the editor focused.
- **Aim slowdown isn't part of character movement's saved moves.** A remote Bat could see small corrections when he starts or stops aiming. Bat is always the listen-server host, so it doesn't happen in practice.
- **Placeholder sounds** come from the engine's VR editor content (`/Engine/VREditor/Sounds/...`), assigned as soft references on the arrow and the platform. Swap them for real audio later.
- **The aim trace stops on Saraa** (pawns block the arrow channel for traces), but the arrow itself flies through her. It's minor: aiming at her puts the arc's endpoint on her.
- **Re-running `build_spirit_path.py`** moves the end zone back and resets the lights. Run `build_canyon.py` and `build_climb.py` after it.

## Suggestions for Milestone 4

- **A way to communicate:** a ping or marker each player can place, visible in the other's era. It's more needed now that Bat has to guide Saraa up platforms she can't see.
- **Timer feedback for Bat:** he can't see a timed platform's countdown. Add a shrinking ring on its outline, or a small HUD timer when he aims at one.
- **Reverse puzzles, where Saraa helps Bat:** "memory" objects only Saraa can reveal for Bat, e.g. she touches an ancient glyph and a present-day handhold appears for him.
- **More echo-platform variants:** moving echoes, chains (one arrow wakes a sequence), or platforms that need two hits from different angles.
- **Bow feel:** a draw-and-release animation (Control Rig or a simple montage), a bow draw sound, arrow impact dust, and a real aim-down-sights camera curve.
- **Checkpoint polish:** a visible checkpoint marker, and checkpoints saved per player across sessions.

---
# Milestone 2 – Testing the Spirit Path, and building the canyon

Branch: `milestone-2-canyon`. Built and tested live through the Unreal MCP server, with the editor open.

## Phase 1: Milestone 1 test

See **`TEST_REPORT.md`**. Every Milestone 1 item passed in a real 2-player listen-server PIE session (29 checks, including replication to the client), and nothing needed fixing. After the canyon was built, the same test was run again and still passes.

## What was built

**Layout.** Coordinates are in cm. The Spirit Path runs along +X at Y = 0, from X = −1500 to 3600.

| | |
|---|---|
| Length | About 350 m. The canyon is closed by rock end caps **150 m before the start** (X = −16 500) and **150 m past the far side** (X = 18 600). |
| Floor width | 37–52 m. Each wall is placed from control points, so the width varies. |
| Wall height | 40–80 m, varying per side, plus ±5 m per segment. |
| Bends | The canyon is straight around the play area and bends away at both ends, 55 m sideways toward −Y at the start and 65 m toward +Y at the far end, so you can't see out of either end. |
| Ravine | Bat's pit now cuts **right across the canyon**, wall to wall, between X = 0 and 1600, so nobody can walk around the Spirit Path puzzle. Its floor is inside the Milestone 1 respawn volume. |
| Overlook | A raised rock shelf 9 m up on the +Y wall, just past the end zone (X 30–44 m), reached by a 26 m ramp from the far side. It's the future vista point. |

**How it's built** (all in `Lvl_SpiritPath`; Milestone 1 actors weren't moved or resized):
- **Landscape `CanyonFloor`:** 378 × 252 m, 1 m resolution, 24 components. It has a gentle rolling floor, a talus apron rising into each wall and rockfall slopes into the end caps. It's flattened to just under the Milestone 1 slabs. Python has no API for creating landscapes, so there's a small editor-only C++ helper, `UEchoEditorLibrary::CreateLandscapeFromHeights`, that does what Landscape mode's *New Landscape* does.
- **Walls:** 251 engine cubes (`/Engine/BasicShapes/Cube`) stacked in 4–6 strata per 14 m segment. Each stratum is set back 1–6 m from the one below (terraces) and slightly rotated, so the walls read as layered sandstone rather than flat walls. On the outside of each bend, pieces are sized from the actual face line so they always overlap (see *Bugs found* below).
- **Rocks (PCG):** `/Game/Echo/PCG/PCG_CanyonRocks` was built node by node through the MCP PCG toolset. It samples the landscape twice: dense large rocks on the talus at the wall bases, and sparse boulders on the open floor. It removes points inside `EchoRockExclusion` boxes (the Spirit Path and the overlook ramp), prunes overlaps and spawns about **440 instances** in 3 ISMs (LevelPrototyping ChamferCube, engine Sphere and Cone). The volume is set to **generate on demand only**: the result is saved in the level and never regenerates at runtime, so both players get identical rocks. The rocks have `BlockAll` collision (solid for both realms).
- **Materials:** `M_CanyonRock` is texture-free. Horizontal sedimentary bands come from world-space Z (wobbled by X/Y), with occasional thin dark bands and gradient-noise brightness variation. It works on scaled cubes and on the landscape without UVs. There are three instances: `MI_CanyonRock` (walls), `MI_CanyonGround` (landscape, sandier) and `MI_CanyonBoulder` (rocks, tighter bands).
- **Boundaries:** `AEchoBoundaryVolume` (new C++) is an invisible box that blocks **only** `Pawn` and `SpiritPawn`, so cameras and traces pass through. There's a chain of them along each wall face, from below the ravine to 140 m up, plus one across each end. Their outlines only draw when selected.
- **Kill volume:** a second `EchoRespawnVolume` (`CanyonKillVolume`), 400 × 260 m, under the whole canyon floor. The original Milestone 1 one only covers the ravine area. Anyone who falls through respawns at their start point.
- **Lighting:** the Milestone 1 lights were retuned. The Sun is low (19°) and warm, from down-canyon, and rakes across the +Y wall. The atmosphere has more dust (Mie) and less blue (Rayleigh). The height fog is warmer and dustier. An unbound `CanyonPostProcess` adds slight desaturation, warm gain and a vignette.

**Scripts** (run in the **open editor**; see *How to test*):
- `Scripts/build_canyon.py` builds the materials, landscape, walls, end caps, overlook, boundaries, kill volume and lighting. It deletes and rebuilds only actors tagged `CanyonBuilder`, and it's deterministic (fixed seed). It needs a rendering editor, because landscape edit layers merge on the GPU, so it won't work headless.
- `Scripts/build_canyon_rocks.py` sets up the PCG rocks: mesh list and collision, exclusion zones and volume size, then regenerates. **Run it after every `build_canyon.py`.**
- `Scripts/test_canyon_live.py` is a live 2-player PIE test of the canyon (34 checks, described below).
- `Scripts/test_pie_live.py` is the live 2-player PIE test of Milestone 1 (29 checks).
- `Scripts/dump_level.py` (read-only actor dump) and `Scripts/measure_fps.py`.

**Choices where the brief was open** (simplest option, as asked):
- **Starter Content isn't installed** in this engine (there's no pack in `D:\UE_5.8\FeaturePacks` or `Samples`), and importing it would add a lot to the 1 GB LFS quota. So everything uses engine BasicShapes plus the template's `LevelPrototyping` meshes, with new materials.
- **Walls are scaled engine cubes**, not Modeling Mode or Geometry Script. The GeometryScripting plugin isn't enabled, and cubes are the most predictable to generate and test.
- **The ravine spans the whole canyon.** Otherwise the new floor would let Bat walk around the pit and skip the puzzle.
- **The overlook is reachable now**, via the ramp, so it can be tested. It's inside the boundaries.
- **The editor Lumen setting** (`[SystemSettingsEditor] r.LumenScene.SurfaceCache.AtlasSize=2048`): see *Performance*.

## How to test

**Just play:** open the project; `Lvl_SpiritPath` loads. Press Play (2 players, Listen Server). Walk both characters away from the Spirit Path in both directions: around the bends to the end caps, up the ramp to the overlook, into the wall bases. Try to climb or jump out, and jump into the ravine away from the path.

**Automated (editor open):** start PIE with the default 2-player listen-server settings. In the editor's Output Log console (Cmd), run:
```
py exec(open(r'D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho/Scripts/test_canyon_live.py').read())
```
Read the `ECHO_CANYON_TEST` lines; the last is `PASS`/`FAIL`. It checks, for **both** Bat and Saraa:
- standing on the landscape floor at 3 spots, on the overlook shelf, on the ramp, and on a PCG rock
- being held by the boundary at **every 5 m along both walls**, at 15 m and at 90 m up (above every rim)
- being stopped at both ends, at ground level and 90 m up
- falling below the floor far down the canyon and respawning at the start
- no rocks on the Spirit Path.

Then **stop PIE, start a fresh one** and run `test_pie_live.py` the same way (Milestone 1, `ECHO_PIE` lines). The canyon test moves both characters around, so each test needs its own PIE session.

**Rebuilding the canyon** (resets any hand edits to `CanyonBuilder` actors, but not to Milestone 1 actors): run `build_canyon.py`, then `build_canyon_rocks.py`, the same way (`py exec(open(r'...').read())`).

## Results

- `test_canyon_live.py`: **PASS** (34/34).
- `test_pie_live.py` (Milestone 1, run after the canyon was built): **PASS** (29/29).
- The level dump confirms all 14 Milestone 1 gameplay actors are exactly where Milestone 1 left them. Only the Sun angle and fog height (lighting) changed.
- Both PIE windows were checked visually: warm ochre strata, dusty light, rocks at the wall bases, and "Milestone complete" on both screens.

**Bugs the tests found (all fixed):**
1. **A gap in the wall boundary on the outside of the far bend.** At rim height a player could have gone straight out at X ≈ 77 m. Wall pieces were spaced by centreline distance, but the face line on the outside of a curve is longer. They're now sized from the actual face-line chord (`face_chord` in `build_canyon.py`).
2. **The PCG rocks had no collision.** The PCG spawner defaults to `NoCollision`, so players walked through them. They now use `BlockAll`.
3. **2-player PIE ran out of video memory** with the canyon (the on-screen message was "Video memory has been exhausted", about 120 MB over budget on this 6 GB RTX 2060). Fixed with the editor-only Lumen setting above; see *Performance*.

## Performance

- **What the canyon adds:** 1 landscape (24 components), 251 cube actors, about 440 rock instances in 3 ISMs, 56 invisible boundary boxes, 1 post-process volume. That's modest for UE5.
- **Frame rate: couldn't be measured meaningfully from here.** The editor was always a background window while I drove it, and on this machine a background editor runs at about 7–8 fps even with the level empty of PIE and "Use Less CPU when in Background" turned off. Milestone 1 measured the same 8 fps before the canyon existed. So those numbers measure Windows/editor throttling, not the level. **Please check it yourself:** click into the PIE window and type `stat fps` (or `stat unit`) in the console. `Scripts/measure_fps.py` also works if the editor is focused.
- **GPU profile** (`ProfileGPU`, 2-player PIE, 2 views): no single canyon-related pass dominates. Shadow depths are about 1.5 ms and there are about 350k primitives per view. The largest costs are character skinning, hardware-ray-traced skinned-mesh updates and Lumen's radiance cache, all of which Milestone 1 already had. Absolute timings were inflated because the GPU was downclocked in the background.
- **Video memory:** 2-player PIE renders two separate worlds on one GPU, so every per-scene buffer (Lumen surface cache, virtual shadow maps, distance fields) exists twice. Halving the Lumen surface cache atlas in the editor only freed about 500 MB and removed the warning, with no visible difference. A packaged game renders one world per machine and keeps the default.

## Known issues

- **The frame rate hasn't been measured with the editor focused.** See above; this needs a quick manual check.
- **Re-running `build_spirit_path.py` (Milestone 1)** respawns the Sun, sky and fog with their old settings. Run `build_canyon.py` afterwards to re-apply the canyon lighting.
- **The editor viewport looks brighter and more washed out than PIE,** because of auto-exposure and the editor view. Judge the look in PIE.
- **The boundary sits up to about 0.6 m off the ideal face line,** and players are stopped 1–2 m from the rock on some stretches. It works, but the invisible wall might be noticeable against the rock. A future pass could use per-segment walls that follow the rocks more tightly.
- **The landscape has no paint layers:** it uses one material and one colour variation. Hand-sculpting works, but `build_canyon.py` would overwrite it.
- **After regenerating rocks,** the editor viewport briefly showed faint "ghost" circles where old rocks were (stale lighting cache). They weren't visible in PIE.
- **Closing the editor through MCP hangs:** `QUIT_EDITOR` typed via the MCP server leaves the editor stuck in "Preparing to exit", because the MCP call never returns. Everything was saved, so ending the process was safe. It's better to close the editor by hand.
- **Startup shows an "Asset Manager … GameFeatureData" load error.** It comes from the MCP `AllToolsets` plugin and is harmless.

## Suggestions for Milestone 3

- **Past and present canyons:** generalise the spirit platform's local-visibility trick into a reusable `UEchoRealmComponent` (visible/solid per realm) and put it on canyon pieces. For example, a rockfall that blocks the present but not the past, or an ancient bridge only Saraa has. `build_canyon.py` could tag realm variants.
- **Checkpoints:** an `AEchoCheckpoint` volume that updates each character's respawn transform, so both kill volumes send players to the last checkpoint instead of the start.
- **The vista moment:** use the overlook for a short camera or sound beat, with both players seeing their own era from the same spot.
- **Better rocks:** enable GeometryScripting to generate irregular rock meshes (or build a small modular kit), and add landscape paint layers (sand, gravel, scree) with PCG scrub and small debris.
- **More realistic multiplayer perf testing:** PIE with *Run Under One Process* off (a separate client process), plus a scalability preset for lower-end GPUs (hardware RT off, Lumen at lower quality).
- **The next area:** turn one end cap into a narrow passage or collapsed slot canyon leading to the next level section. The builder already supports moving the ends.

---
# Milestone 1 – "The Spirit Path" greybox

## What was built

**C++ (`Source/OurLastEcho/Echo/`)**
- `EchoTypes.h`: the `EEchoRealm` enum (Living / Spirit) and the `SpiritPawn` collision channel (`ECC_GameTraceChannel1`, registered in `Config/DefaultEngine.ini`).
- `AOurLastEchoCharacter` (template class, extended) now has:
  - a per-Blueprint `Realm`
  - Spirit characters switching their capsule to the `SpiritPawn` channel and ignoring `Pawn`, so Bat and Saraa pass through each other
  - an optional `GhostMaterial` that replaces every mesh material
  - `RespawnAtStart()`.
- `AEchoSpiritPlatform`: collision blocks **only** `SpiritPawn`, so Bat falls through. Visibility is decided separately on each machine from the realm of the locally controlled character. Nothing about it is replicated, so each screen shows only what that player can see.
- `AEchoSpiritSwitch`: a floor pad. When a character of `RequiredRealm` (default Spirit) steps on it, it raises `TargetBridge`. It's one-shot, and the pressed state is replicated.
- `AEchoRisingBridge`: only the `bRaised` flag is replicated. Every machine plays the 3-second rise locally, so it's smooth for both players.
- `AEchoEndZone` + `AEchoGameState` + `AEchoHUD`: when a Living and a Spirit character are both in the zone, the server sets a replicated `bMilestoneComplete` flag. The HUD then draws "Milestone complete" on both screens.
- `AEchoGameMode`: the first player to join plays Bat and everyone after plays Saraa. If Bat leaves, the next player to join becomes Bat.
- `AEchoRespawnVolume`: sends anyone who falls into the pit back to their spawn point.

**Content (`Content/Echo/`), generated by `Scripts/build_spirit_path.py`**
- `BP_Saraa`: a child of `BP_ThirdPersonCharacter` with Realm = Spirit and the `M_Ghost` material. Bat is just `BP_ThirdPersonCharacter`, unchanged.
- `BP_EchoGameMode`: uses the template's player controller, with Bat/Saraa pawn classes set.
- Materials:
  - `M_EchoGlow` plus instances: `MI_SpiritPlatform` (glowing blue), `MI_Switch` (amber), `MI_EndZone` (green)
  - `M_Ghost`: unlit, translucent, pale blue with a brighter rim.
- `Lvl_SpiritPath` (now the default editor and game map):
  - start area with two PlayerStarts
  - a 16 m gap with four spirit stepping stones on the left lane (small hops, rising up to 60 cm)
  - a rising bridge on the right lane
  - the switch just past the last stepping stone
  - a green end zone at the far end
  - a respawn volume under everything.

## How to test

**In the editor**
1. Open the project. `Lvl_SpiritPath` opens automatically.
2. Play settings are already set to **2 players, Play As Listen Server**. Check the dropdown next to Play if they've been changed.
3. Press Play. The server window is **Bat** (normal mannequin) and the client window is **Saraa** (pale, see-through).
4. Check the following:
   - Bat's window shows no blue platforms; Saraa's does.
   - Saraa hops across the left lane.
   - Bat walking onto the left lane falls into the pit and respawns at the start.
   - Saraa steps on the amber pad. The bridge rises on the right lane, and Bat sees it rise.
   - Bat crosses the bridge.
   - With both players standing on the green zone, "Milestone complete" shows on both screens.

**Automated (editor closed)**
- `powershell -File Scripts/run_spirit_path_test.ps1` runs a headless game-world test: 16 checks covering platform collision for both characters, the switch ignoring Bat and responding to Saraa, the bridge rising and holding Bat, pit respawn, and the end zone needing both players. It currently passes.
- A headless listen server + client run confirmed the host gets Bat and the joining client gets Saraa.

## Done-when checklist

| Item | Status |
|---|---|
| Compiles with no errors | ✅ Clean build, no warnings |
| PIE, 2 players, Listen Server | ⚠️ Set as the default. A real listen server + client connected and got the right characters headless, but it hasn't been played in the editor yet |
| Saraa crosses on spirit platforms; Bat falls through | ✅ Verified in a game world by the automated test |
| Bat can't see the platforms; Saraa can | ⚠️ Logic is in place; needs a visual check in PIE (headless runs don't render) |
| Switch raises the bridge; Bat sees it rise | ✅ Raise verified by the test on the server; ⚠️ the client-side animation needs a visual check |
| Both in end zone → "Milestone complete" | ✅ Game state verified by the test; ⚠️ the on-screen text needs a visual check |

## What didn't work / caveats

- **The Unreal MCP plugin isn't installed**, so the level is built by a Python script instead. Re-running `build_spirit_path.py` deletes and respawns everything it placed (tagged `EchoBuilder`). **Hand edits to those actors get reset**, so stop re-running it once you start polishing the layout by hand. Actors you add yourself are left alone.
- **Nothing visual could be checked by me.** Headless runs have no rendering, so ghost translucency, platform visibility per screen, and how the glow and lighting look all still need to be checked in PIE.
- **Things I tried that didn't work:** testing collision in the editor world during a headless script. That world has no collision loaded, and Saraa's special collision is only applied when gameplay starts. The test runs in a real game world instead.
- **Simplest-option choices, per the brief:**
  - Bat and Saraa **pass through each other**, since she's in the past.
  - The switch is **one-shot**, so the bridge stays up.
  - Anyone falling into the pit **respawns at their own start**. This respawn wasn't in the brief, but without it, falling was a dead end.
  - Saraa is visible to Bat. The brief said she should "read as a ghost", not be invisible.
  - Any 3rd player would also be Saraa.
  - The "Milestone complete" text is drawn on the HUD canvas rather than as a UMG widget, since menus/UI were out of scope.
- Movement values are the template's; stepping-stone spacing was chosen for its default jump. If hops feel too easy or hard, move the platforms by hand in the editor.

## Suggestions for Milestone 2

- **The reverse mechanic:** objects only Bat can see or touch (e.g. present-day rubble, ropes, bridges that don't exist in the past). This needs a matching `LivingPawn` channel or a `SolidForRealm` setting on the platform.
- **Shared puzzles that need both players at once:** Bat holds a pressure plate in the present so a spirit platform appears for Saraa. This is the first real two-way puzzle.
- **Communication:** a simple ping or marker each player can place that shows up in the other's world, since they can't see the same things.
- **Proper game flow:** a lobby or join screen (Online Subsystem / Steam for real online play instead of LAN/PIE), a UMG HUD showing whose realm you're in, and a checkpoint system to replace respawn-at-start.
- **Look and feel:** a spirit-world post-process for Saraa's camera (desaturated, bloom), a proper ghost material and trail, and platforms fading in and out instead of popping.
