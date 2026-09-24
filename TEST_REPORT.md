# Milestone 1 test report

Tested on 2026-09-24 on branch `milestone-2-canyon`, before any Milestone 2 changes. Tests were run through the Unreal MCP server with the editor open.

**Result: every Milestone 1 item passes. No bugs needed fixing.** A few small risks are logged at the end.

## How it was tested

- **Compile:** the editor was open (Live Coding blocks editor builds), so I built the standalone **Game** target instead: `Build.bat OurLastEcho Win64 Development`. It compiles the same module code with the same warning settings, without touching the editor's DLL.
- **Level contents:** `Scripts/dump_level.py` was run in the open editor. It's read-only and lists every actor with its class, folder, position, bounds and gameplay settings.
- **Live 2-player test:** the MCP `StartPIE` tool started a real PIE session. The editor's play settings are 2 players, *Play As Listen Server*, which gives a server window (Bat) and a client window (Saraa). `Scripts/test_pie_live.py` then ran inside the editor. It moves the characters on the **server** world (server-authoritative) and checks what the **client** world ends up with, so it covers replication as well as game logic. 29 checks, last line `PASS`.
- **Visual check:** MCP screenshots of both PIE windows were taken at the end of the run (see below).

The MCP server has no tool for running Python directly. Its UI-automation toolset (`SlateInspectorToolset`) can type into the editor's `Cmd` console box, though, so scripts run with `py exec(open(r'<path>').read())`. Output goes to `Saved/Logs/OurLastEcho.log`.

## Results

| # | Item | Result | Notes |
|---|---|---|---|
| 1 | Level contents match NOTES.md | ✅ Pass | 16 actors, all tagged `EchoBuilder`, positions exactly as `build_spirit_path.py` places them: Sun, SkyLight, SkyAtmosphere, HeightFog, StartArea, FarSide, PlayerStart_A/B, SpiritPlatform_1–4 (250×250×25), RisingBridge (1600×300×30), SpiritSwitch (target = RisingBridge, realm = Spirit), EndZone (600×800×300), RespawnVolume (100×100×6 m). No stray or hand-edited actors. |
| 2 | Project compiles, no errors or warnings in our code | ✅ Pass | Game target: `Result: Succeeded`, 0 warnings, 0 errors. The editor DLL was already up to date with the source. |
| 3 | Host plays Bat, joining client plays Saraa | ✅ Pass | Checked live: server's local pawn is Living, client's local pawn is Spirit. |
| 4 | Spirit platforms **invisible** for Bat, **visible** for Saraa | ✅ Pass | Bat's machine: all 4 platform meshes hidden. Saraa's machine: all 4 visible. Still hidden for Bat after the bridge rises. |
| 5 | Spirit platforms **solid** for Saraa, **non-solid** for Bat | ✅ Pass | Swept drops with each character's real capsule: Saraa lands on platforms 1 and 3; Bat falls through both. Both land on the start and far-side floors. |
| 6 | Switch ignores Bat, responds to Saraa | ✅ Pass | Bat on the pad: not activated. Saraa on the pad: activated, and the server bridge is raised. |
| 7 | Bridge rise **replicates to Bat** | ✅ Pass | Checked on the client world: `bActivated` and `bRaised` replicated. The client's bridge mesh went from −815 to −15 (fully up) within the 3 s rise. |
| 8 | Bat can cross the raised bridge | ✅ Pass | Bat lands on the bridge after it rises; before that, he falls into the pit. |
| 9 | Falling into the pit respawns you | ✅ Pass | Bat dropped into the pit was back at the start (x = −1200) within a few seconds. |
| 10 | End zone triggers **only when both players are inside** | ✅ Pass | Bat alone: not complete. Bat + Saraa: complete on the server, and replicated to the client. |
| 11 | "Milestone complete" shows on both screens | ✅ Pass | Seen in screenshots of both the server (Bat) and client (Saraa) PIE windows. Saraa renders as the pale translucent ghost on both. |
| 12 | Frame rate in 2-player PIE | ⚠️ Needs manual test | The test measured about 8 fps, but only because the editor was a **background** window. On this machine a background editor runs at 7–8 fps even with no PIE running and the editor's own throttling turned off, so the number reflects Windows/editor throttling, not the game. Needs `stat fps` with the editor focused. See NOTES.md → Performance. |

Apart from the frame rate (item 12), nothing needs a manual test. The MCP server could run 2-player PIE, so everything was checked live. The one thing no script can judge is **feel**: jump distances, camera and readability. That's worth a quick play by a human.

## Things I noticed but didn't change

None of these are bugs in current play. They're logged instead of fixed, as the brief asks.

1. **A client joining after the bridge has risen replays the rise animation.** The client may run `BeginPlay` before `bRaised` arrives, so it starts lowered and then animates up. This is cosmetic and only happens to late joiners.
2. **The original respawn volume only covers 100 × 100 m around the Spirit Path.** That's fine for Milestone 1. The canyon is much bigger, so Milestone 2 adds a second, canyon-wide kill volume instead of resizing this one (Milestone 1 actors stay untouched).
3. **Spirit platforms re-check visibility 10 times a second forever**, to catch possession changes. That's cheap with 4 platforms. If there are dozens later, switch to an event, e.g. when the local player controller possesses a pawn.
4. **Respawn always goes to the player's own spawn point**, not a checkpoint. The brief allows this for now.

## Re-running these tests

- **Editor open (preferred):** start PIE with the default 2-player listen-server settings, then run in the editor console:
  `py exec(open(r'D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho/Scripts/test_pie_live.py').read())`
  and read the `ECHO_PIE` lines in the Output Log. The last one is `PASS` or `FAIL`.
- **Editor closed:** `powershell -File Scripts/run_spirit_path_test.ps1` (the Milestone 1 headless single-process test).

## Re-test after the Milestone 2 canyon

The canyon was built around the Spirit Path afterwards (see NOTES.md), so the tests were run again at the end of Milestone 2:

| Test | Result | Notes |
|---|---|---|
| `test_pie_live.py` (all Milestone 1 items above) | ✅ Pass, 29/29 | Fresh 2-player PIE session with the full canyon in place. |
| Milestone 1 actors unchanged | ✅ Pass | `dump_level.py` before and after: all 14 gameplay actors are in the same position, size and settings. Only the Sun angle and fog height were retuned for the canyon lighting. |
| `test_canyon_live.py` (new) | ✅ Pass, 34/34 | Both realms: floor, overlook, ramp and rocks are solid; walls hold every 5 m at 15 m and 90 m up; both ends hold; the canyon kill volume respawns; no rocks on the Spirit Path. |

The canyon test found three Milestone 2 bugs, all **fixed** before the final run: a boundary gap on the outside of the far bend, PCG rocks with no collision, and 2-player PIE running out of video memory. Details are in NOTES.md.
## Milestone 3: baseline before starting, then results

### Baseline (start of Milestone 3, before any changes)

Run on branch `milestone-3-spirit-bow` straight after branching from `main`, in fresh 2-player listen-server PIE sessions:

| Test | Result |
|---|---|
| `test_pie_live.py` (Milestone 1 mechanics) | ✅ Pass, 29/29 |
| `test_canyon_live.py` (Milestone 2 canyon) | ✅ Pass, 34/34 |

### Milestone 3 results (end of milestone)

| Item | Result | Notes |
|---|---|---|
| Compiles, no warnings | ✅ Pass | Editor target and Game target: `Result: Succeeded`, 0 warnings. |
| Only Bat has the bow | ✅ Pass | `CanUseBow` is true for Bat only. Bat carries the 6-part bow mesh and Saraa has none. The server refuses a shot from Saraa's bow. |
| Aim (camera, reticle, slow, facing) | ✅ Pass | Camera 400 → 160 cm, walk speed 500 → 220, faces the aim direction. Saraa's machine sees Bat aiming (replicated). Releasing restores everything. The reticle was checked visually. |
| Controls: keyboard/mouse and gamepad | ⚠️ Needs manual test | `IMC_Bow` is verified to map Aim to RMB + Gamepad_LeftTrigger and Fire to LMB + Gamepad_RightTrigger, and the component binds them. No real device was pressed. |
| Fire, cooldown, arrows seen by both | ✅ Pass | The first shot fires and an immediate second is refused. The arrow exists on both machines and is seen flying on Saraa's (40 m/s). The trail and glow were checked visually. |
| Bat sees dormant platforms; Saraa doesn't | ✅ Pass | All 7: outline visible and slab hidden on Bat's machine, both hidden on Saraa's. |
| Nobody stands on a dormant platform | ✅ Pass | Both characters fall through. |
| Shot wakes a platform: solid and visible for Saraa, cue on both | ✅ Pass | Awake on both machines. Saraa sees the blue slab and lands on it. Bat still sees the outline and falls through. The wake flash and sound counter went up on **both** machines. |
| Timed platform goes dormant again | ✅ Pass | P3 (7 s): Saraa's machine knows the time left. It went dormant on both machines, Saraa fell through it, and the permanent platforms stayed awake. |
| The "specific spot" platform | ✅ Pass | 4 shots at P6 from the open floor in front of the rock screen were all blocked. The shot from the gap behind the screen woke it. |
| Saraa can climb The Climb | ✅ Pass | **Real jumps** on her client (movement input + jump), P1 → P7 → ledge, every hop first try, with the server agreeing where she landed. P3 was re-shot just before she needed it. |
| Saraa's switch lets Bat reach the end zone | ✅ Pass | The switch ignores Bat; Saraa's press lowers the ramp on both machines. Bat walks up the ramp onto the ledge (real movement input). |
| End zone on the ledge | ✅ Pass | Not complete with Bat alone; complete with both, on both machines. |
| Checkpoint | ✅ Pass | Saraa falling into the canyon kill volume after the climb base respawns at the base. |
| Debug command shows all platforms | ✅ Pass (host) / ⚠️ client needs manual test | Typing `EchoShowAllPlatforms` in the editor console during PIE ran on Bat and turned it on for both machines. Bat then saw the spirit platforms and Saraa saw every dormant echo. It turns off again. Saraa's own console uses a server RPC that editor Python can't exercise, because every call runs locally during Python (`GAllowActorScriptExecutionInEditor`). Needs one manual try. |
| Earlier mechanics still work | ✅ Pass | `test_pie_live.py` 29/29 and `test_canyon_live.py` 34/34, updated for the moved end zone and the new checkpoint. The Milestone 1 end zone moved (allowed by the brief). No other Milestone 1 or 2 gameplay actor was touched. |
| Runs smoothly with 2 players in PIE | ⚠️ Needs manual test | Same situation as Milestone 2: a background editor on this machine is throttled to about 8 fps whatever the level, so it can't be judged from here. The new actors are cheap: 7 platforms, a few arrows at a time. |

**Totals:** Milestone 1 29/29, Milestone 2 34/34, Milestone 3 60/60.