# Our Last Echo

An online 2-player co-op game in Unreal Engine 5. **Bat** (player 1, the listen-server host) is a grieving brother in the present-day canyon. **Saraa** (player 2) is his late sister, in the canyon's ancient past, the afterlife. Core idea: each player sees and touches things the other can't.

Started from the **Third Person C++ template**. Its Combat/Platforming/SideScrolling variant code and levels are still in the project but unused. Work is organised into milestones from briefs the user provides; see `NOTES.md` for the latest milestone's status and test steps.

## Gameplay architecture (Milestone 1)

- **Realms:** `EEchoRealm` (Living/Spirit) in `Source/OurLastEcho/Echo/EchoTypes.h`. It's a class default on `AOurLastEchoCharacter` (`Realm`). Bat = `BP_ThirdPersonCharacter` (Living, unchanged template BP); Saraa = `BP_Saraa` (Spirit, `M_Ghost` material).
- **Realm-only collision:** Spirit capsules use object channel **`SpiritPawn` = `ECC_GameTraceChannel1`**, defined in `Config/DefaultEngine.ini` (`#define ECC_SpiritPawn` in EchoTypes.h; keep the two in sync). Spirit platforms block only that channel. Because the channel choice is deterministic on every machine, client movement prediction agrees with the server. Any new trigger/volume must explicitly set Overlap for **both** `ECC_Pawn` and `ECC_SpiritPawn`. Engine profiles that list channels explicitly (Trigger, OverlapAll) fall back to the custom channel's default, **Block**.
- **Realm-only visibility** is local per machine (`UPrimitiveComponent::SetVisibility`, based on the local player's pawn realm). Never use `SetActorHiddenInGame` for this, because it replicates.
- **Networking:** listen server. Server-authoritative gameplay state is in replicated flags (`bActivated`, `bRaised`, `AEchoGameState::bMilestoneComplete`); cosmetic motion (the bridge rising) is animated locally from the flag. `AEchoGameMode` gives the first controller Bat and all later ones Saraa.
- Placeable actors (`AEchoSpiritPlatform`, `AEchoSpiritSwitch`, `AEchoRisingBridge`, `AEchoEndZone`, `AEchoRespawnVolume`) use engine BasicShapes meshes and **soft-path default materials** under `/Game/Echo/Materials/`. Their sizes are `...Size` properties (in cm) applied in `OnConstruction`, and the actor origin is the walking surface.

## The canyon (Milestone 2)

- `Lvl_SpiritPath` is wrapped in a canyon: a `CanyonFloor` landscape, terraced cube strata walls, PCG rocks, an overlook shelf with a ramp, boundaries, a kill volume and warm lighting. The layout lives in the functions at the top of `Scripts/build_canyon.py`. The ravine under the Spirit Path gap spans the whole canyon on purpose, so the pit can't be walked around.
- `AEchoBoundaryVolume`: an invisible box that blocks **only** `Pawn` + `SpiritPawn` (cameras and traces pass through). Use it for player boundaries.
- `UEchoEditorLibrary::CreateLandscapeFromHeights` (editor-only; Python name `unreal.EchoEditorLibrary.create_landscape_from_heights`): Python can't create landscapes otherwise.
- Rocks: `/Game/Echo/PCG/PCG_CanyonRocks` on the `CanyonRocks` PCG volume, **generate on demand only** (saved in the level; never regenerates at runtime). `EchoRockExclusion`-tagged boxes keep rocks off paths.
- Canyon pieces are tagged `CanyonBuilder`; rock setup is tagged `CanyonRocksBuilder`. Milestone 1 actors stay `EchoBuilder`, but `build_canyon.py` retunes the Milestone 1 Sun, sky and fog.

## Spirit bow, echo platforms, The Climb (Milestone 3)

- `UEchoSpiritBowComponent` (`SpiritBow` on every `AOurLastEchoCharacter`; only a Living character can use it). Aim = `IA_Aim` (RMB / left trigger), Fire = `IA_Fire` (LMB / right trigger), both in `/Game/Input/IMC_Bow`, which the component adds for Bat. The **server** spawns `AEchoArrow` (replicated; every machine flies it from the replicated launch velocity). Tuning is on the component in `BP_ThirdPersonCharacter`.
- Object channel **`EchoArrow` = `ECC_GameTraceChannel2`** (`#define ECC_EchoArrow` in EchoTypes.h), default **Block**: world geometry stops arrows, and the arrow itself ignores pawns. Anything arrows should hit, but nobody can stand on, needs a box that blocks only `ECC_EchoArrow` (like `AEchoPlatform::HitBox`).
- `AEchoPlatform`: dormant (gold flickering outline for Bat only, no collision) or awake (blue slab for Saraa, blocks only `SpiritPawn`). `bAwake` is replicated, plus `AwakenCount` for the cue on every machine. `bTimed`/`AwakeDuration` per instance. Visibility logic is shared with the spirit platforms in `EchoVisibility.h`, including the `EchoShowAllPlatforms` debug flag on `AEchoGameState`.
- `AEchoCheckpoint` sets a character's respawn transform. `AEchoRisingBridge` can hinge (`bHingeAtStart` + `StartRotationOffset`) for The Climb's ramp.
- The Climb is built by `Scripts/build_climb.py` (tag `ClimbBuilder`; it also moves the `EndZone` onto the ledge). Its layout is in wall coordinates (station `s`, `u` = cm out from the left wall), and it places platforms by **measured** distance because the curving wall stretches stations.
## Sword whip, anchor arrows, The Crossing (Milestone 4)

- **Anchors:** an arrow hit becomes an anchor only if **`UEchoAnchorRules::CanHoldAnchor(Hit)`** says so (`EchoAnchorable.h`). That's the one place the rule lives; today it's a `UEchoAnchorableComponent` on the hit actor, later maybe a physical material. The server's `UEchoSpiritBowComponent::CreateAnchor` spawns a replicated `AEchoAnchorPoint`. Bat sees the stuck arrow, Saraa a blue orb at `GetSwingPoint()`. The bow keeps at most `MaxActiveAnchors` (2) and destroys the oldest. `AEchoAnchorTarget` is the placeholder board (Bat-only visibility; blocks only `ECC_EchoArrow`).
- **Whip:** `UEchoSwordWhipComponent` (`SwordWhip` on every character; Spirit realm only).
  - Local targeting and highlight, input (`IA_Whip` / `IMC_Whip`: LMB, E, right trigger), and the sword and whip-line look.
  - Cues through replicated `bLatched` / `LatchCount` (COND_SkipOwner).
  - The lash (stretch goal) is a server RPC, and `AEchoTrainingDummy` takes the hit.
  - **All swing tuning lives on this component** (BP_Saraa).
- **The swing is movement, not an RPC:** `UEchoCharacterMovementComponent` (both characters use it; set in `AOurLastEchoCharacter`'s `FObjectInitializer` constructor) has a custom mode `EEchoCustomMovement::Swing`.
  - The whip input (`bWantsToSwing` + anchor point + rope length) is part of the **saved move** (`FEchoSavedMove`, `FLAG_Custom_0`) and of the **network move data** (`FEchoNetworkMoveData`), so the server replays exactly the client's latch.
  - Anything that changes swing state must go through these: `RequestSwing` / `StopSwingRequest` / `CancelSwing`, never direct mode changes. Otherwise prediction breaks.
  - After a swing, falling braking is off until landing (`bLaunchedFromSwing`, `GetMaxBrakingDeceleration`).
  - Verbose `Swing:` logs (`log LogOurLastEcho Verbose`) show latch and release on client, server and replay, and every correction.
- **Level:** `build_canyon.py` cuts The Crossing's chasm into the landscape (`CHASM_S`, stations 7850–15000; walls reach down past it). `Scripts/build_crossing.py` (tag `CrossingBuilder`) builds everything in it, places the training dummy, and **moves the EndZone to the far rim**.
  - Layout in wall coordinates: `u` = from the left wall, `r` = from the right wall.
  - Its swing distances were tuned against real swings. Moving an anchor or pillar means re-running `test_crossing_live.py`, which also proves one arch anchor can't replace the chain.
- `AEchoCheckpoint` can be limited to one realm (`bOnlyOneRealm` / `OnlyRealm`): separate respawns for Bat's and Saraa's routes.
- **Swapping roles for tests:** console variable `Echo.HostPlaysSaraa 1` before starting PIE (or `BP_EchoGameMode.bHostPlaysSaraa`). Setting the Blueprint CDO from Python does **not** reach PIE: the Blueprint recompiles when PIE starts.

## Title screen, menus, sessions, settings (Milestone 5)

- **Maps:** `GameDefaultMap` = `/Game/Echo/Maps/TitleScreen` (built by `Scripts/build_title_screen.py`; `EditorStartupMap` is still Lvl_SpiritPath). Its game mode `AEchoFrontEndGameMode` spawns no pawns. Opened with `?listen` it's the **lobby** (`IsLobby`), with `AEchoLobbyPlayerState` (replicated `bReady`, `bHostPlayer`). Start = non-seamless `ServerTravel` to Lvl_SpiritPath; `AEchoGameMode` gives the host Bat as before.
- **UI = CommonUI, built in C++** (`Source/OurLastEcho/Echo/UI/`): screens are `UEchoScreen` (a `UCommonActivatableWidget` that builds its own widget tree in `BuildContent`, not UMG Blueprints). `UEchoUIRoot` holds the `UEchoMenuStack` (fade 0.25 s), dialogs (`ShowDialog`; last choice = Back) and toasts. `UEchoButton` builds its tree in `Initialize()` before CommonButtonBase wraps it. CommonUI input data: `DT_UIActions` + `BP_EchoUIInputData` (`DefaultGame.ini`); viewport client `CommonGameViewportClient`.
  - Back is ignored for 0.3 s after a screen activates (one press = one screen).
  - A screen covered by a dialog must not take itself off the stack from the dialog's callback (it jams the stack): set a flag and close when re-activated (see `UEchoSettingsScreen::bCloseWhenActivated`).
- **Front-end controller** `AEchoFrontEndPlayerController` (title camera, UIRoot, lobby RPCs `ServerSetReady`/`ServerStartGame`, `ClientShowToast`). **In game**, `AOurLastEchoPlayerController` opens `UEchoPauseMenuScreen` on `IA_Menu` / `EchoMenu`; it **doesn't pause** (online). The old paused settings menu and `WBP_SettingsMenu` are gone.
- **Sessions:** `UEchoSessionSubsystem` (game-instance subsystem) is the only thing the menus call; it uses `Online::GetSessionInterface(World)` (per PIE instance), OSS **Null** today (switch platform service for EOS). Join = `GetResolvedConnectString` + `ClientTravel`, with a 15 s connect timeout. `UEchoGameInstance` turns network/travel failures into the message shown on the title (`ReturnToTitle`, pending message) and has exec commands `EchoHost`, `EchoFindGames`, `EchoJoinGame N`, `EchoReady 0/1`, `EchoStartGame`, `EchoLeave`.
- **Settings:** `UEchoGameUserSettings` (`GameUserSettingsClassName`; per machine, `GameUserSettings.ini`). Audio = sound classes `SC_Master` → `SC_Music`/`SC_SFX`/`SC_Dialogue`/`SC_Voice` with mix `SMix_Settings` (DefaultSoundClass = SC_SFX, VoiP = SC_Voice). Brightness = `GEngine->DisplayGamma`. **Defaults are written out in `SetToDefaults`**: a config class's CDO holds the saved values. Keys: Enhanced Input user settings (`bEnableUserSettings`), mappable names on the IAs and on IMC_Default's WASD. In the editor, Apply skips resolution/window mode.
- **Title scene actors:** `AEchoTitleCamera` (drift only in a game world; it ticks in the editor too) and `AEchoTitleBat` (poseable-mesh seated pose `SeatedPose`, `LoopAnimation` hook). Pose rotators: negative roll swings a leg forward.
## Engine & toolchain

- **Unreal Engine 5.8.3**, installed at `D:\UE_5.8`. It's a registered (non-launcher) build: the `.uproject`'s `EngineAssociation` is a GUID that maps to that path via `HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds`.
- Visual Studio 2022 Community (Game dev C++ workload, MSVC 14.44, Windows SDK 10.0.22621).
- Single runtime module: **`OurLastEcho`** (`Source/OurLastEcho/`). Dependencies are in `OurLastEcho.Build.cs`.
- The project root (this folder, with the `.uproject`) sits inside an outer folder that is also named `OurLastEcho`. Git and this file live here in the inner one.

## Building

```
"D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" OurLastEchoEditor Win64 Development "-Project=D:\Creating games in term 4\My Own games\OurLastEcho\OurLastEcho\OurLastEcho.uproject" -WaitMutex
```

- This command only works when the **editor is closed**. While the editor is open with Live Coding on, UBT refuses to build. In that case, the user hot-reloads with **Ctrl+Alt+F11** in the editor.
- Header changes that add or remove `UPROPERTY`/`UFUNCTION`/`UCLASS` or change class layout are unreliable through Live Coding. For those, ask the user to close the editor and do a full build.
- After editing code, build and fix compile errors before reporting a change as done.

## Running editor Python scripts (level / asset "builders")

`PythonScriptPlugin` and `EditorScriptingUtilities` are enabled in the `.uproject`. Scripts live in `Scripts/` (outside `Content/`). They can run headless when the editor is closed:

```
"D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Creating games in term 4\My Own games\OurLastEcho\OurLastEcho\OurLastEcho.uproject" -run=pythonscript -script="D:\Creating games in term 4\My Own games\OurLastEcho\OurLastEcho\Scripts\<script>.py" -unattended -nop4 -nosplash
```

Script output does **not** reach the console. Read it from `Saved/Logs/OurLastEcho.log`, where `unreal.log(...)` lines show up as `LogPython:`. Put a unique marker in log lines so they're easy to grep for. `Scripts/check_python.py` is a quick read-only smoke test (it lists the project's maps).

The user can also run them from inside the open editor (Output Log → switch the input to `Python`, or **Tools → Execute Python Script**). A builder script must **save** what it changes (`unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)` or `unreal.EditorAssetLibrary.save_asset`), otherwise a headless run throws the changes away.

Project scripts:
- `Scripts/build_spirit_path.py`: builds the Milestone 1 materials, `BP_Saraa`, `BP_EchoGameMode` and `Lvl_SpiritPath`. Re-running it deletes and respawns every actor tagged `EchoBuilder`. **Once the user starts hand-tuning the level, don't re-run it unprompted** (it resets their edits). Only re-run it for structural changes, and say so when you do.
- `Scripts/verify_spirit_path.py`: read-only dump of the generated assets and level actors.
- `Scripts/build_canyon.py` then `Scripts/build_canyon_rocks.py`: the Milestone 2 canyon and its PCG rocks. **Open editor only.** Landscape edit layers merge on the GPU, and headless runs have no RHI. Same re-run caution as above.
- `Scripts/build_spirit_bow.py` (Milestone 3 input assets and materials; can run headless) and `Scripts/build_climb.py` (The Climb; open editor).
- `Scripts/build_sword_whip.py` (Milestone 4 input assets and materials; can run headless) and `Scripts/build_crossing.py` (The Crossing; open editor).
- `Scripts/build_title_screen.py` (Milestone 5; open editor): audio classes and mix, CommonUI input data, mappable key names, leaves, and the TitleScreen map (rebuilt from scratch; it refuses to touch any other map, then reopens Lvl_SpiritPath). It skips filling `DT_UIActions` if rows exist (the JSON import pops a modal dialog that blocks Python) and reuses `NS_TitleLeaves` (recreating a Niagara system under the same name crashed `SetAsset`).
- **Level build order:** `build_canyon.py` → `build_climb.py` → `build_crossing.py` (it moves the EndZone after The Climb did) → `build_canyon_rocks.py`.
- **Milestone 5 tests:**
  - `Scripts/tools/run_frontend_test.ps1 -Mode title|online` runs `test_frontend_live.py` (52 / 32 checks) in a fresh front-end PIE session.
  - `Scripts/tools/run_gameplay_test.ps1 -Script <test> -Marker <tag>` runs a Milestone 1–4 live test in a fresh gameplay session.
  - `Scripts/run_lan_test.ps1` (editor closed) runs two real windowed game processes that host, join, ready and start (`test_lan_standalone.py`).
- `Scripts/test_crossing_live.py`: Milestone 4 live test, 68 checks, with real predicted swings on Saraa's client.
  - Options (set as Python globals before exec): `ECHO_CROSS_MODE` (`swing1` / `chain` / `swing3` / `host_saraa`), `ECHO_CROSS_NETEMU` (a list of `NetEmulation.*` console commands), `ECHO_CROSS_TRACE`.
  - Release timing comes from a predicted landing, not fixed delays, so it copes with frame rate.
- `Scripts/test_pie_live.py` (Milestone 1, 29 checks), `Scripts/test_canyon_live.py` (canyon, 34 checks) and `Scripts/test_climb_live.py` (Milestone 3, 60 checks, including real jumps on Saraa's client): **live 2-player PIE tests**, run inside the editor while PIE runs. Use a fresh PIE session for each. They move characters on the server world and check the client world. `Scripts/dump_level.py` is a read-only actor dump; `Scripts/measure_fps.py` reports frame rate.
- `Scripts/run_spirit_path_test.ps1` (runs `test_spirit_path.py`): the older headless **end-to-end test**. It launches a headless game (`-game -nullrhi`), summons a `BP_Saraa`, and uses `py` via `-ExecCmds` to sweep both characters around the level, checking collision, switch, bridge, respawn and end zone. Run it after gameplay changes: `powershell -File Scripts/run_spirit_path_test.ps1`. The last line is PASS/FAIL.

Headless scripting gotchas learned the hard way:
- Pass `-script=` paths with **forward slashes**. In `Scripts\test...` the engine reads `\t` as a tab character.
- The commandlet's editor world has **no collision** (traces hit nothing), and `PostInitializeComponents` doesn't run for actors spawned there. So anything that tests physics or realm collision must run in a real game world (`UnrealEditor.exe ... -game -nullrhi -ExecCmds="py <path-without-spaces>"`). In a game world, Python can't spawn actors; use the `summon` cheat (`EnableCheats` first). Use `unreal.register_slate_post_tick_callback` to wait for things over time, and call `quit` yourself at the end.
- Python names drop the `b` prefix on bools (`bActivated` → `activated`); custom channels appear as `unreal.CollisionChannel.ECC_SPIRIT_PAWN`; `K2_SetActorLocation` is `set_actor_location`.
- Some properties aren't reachable from Python (e.g. `UShapeComponent::bDrawOnlyIfSelected`): set them in C++ or through MCP `ObjectTools`. Engine typos carry over: `SkyAtmosphereComponent.aerial_pespective_view_distance_scale`. An ISM's mesh is `get_editor_property("static_mesh")`. During PIE, `EditorActorSubsystem.get_all_level_actors()` doesn't return the editor level's actors, so stop PIE first.
- **PCG Static Mesh Spawner instances default to `NoCollision`.** Set the descriptor's `body_instance` profile (`BlockAll` blocks both realms) or players walk through the meshes.
- Chains of pieces along a curve must be sized from the curve they sit on, not the centreline spacing, or gaps open on the outside of bends (see `face_chord` in `build_canyon.py`). The canyon test sweeps every 5 m to catch this.

## Unreal MCP (editor open)

The engine's experimental **Unreal MCP** plugin (`ModelContextProtocol` + `AllToolsets`) is enabled. `.mcp.json` points Claude Code at `http://127.0.0.1:8000/mcp`. The server only exists while the editor is open with **Editor Preferences → General → Model Context Protocol → Auto Start Server** on, or after running the console command `ModelContextProtocol.StartServer`. `tools/list` returns only meta-tools (`list_toolsets`, `describe_toolset`, `call_tool`), so discover the actual tools through those. `call_tool` takes the **short** tool name plus the toolset, e.g. `tool_name: "find_actors"`, `toolset_name: "editor_toolset.toolsets.scene.SceneTools"`. The fully-qualified name that `describe_toolset` prints is rejected as unknown.
- **Editor open + MCP connected:** prefer MCP for editor work, so the user's open, hand-edited level isn't overwritten.
- **Editor closed:** use `Build.bat` and the headless Python scripts above. Never run headless builds or scripts while the editor is open.
- **Launch the editor** with `powershell -File Scripts/tools/launch_editor.ps1 -Wait`. It starts `UnrealEditor.exe` with `-DDC=NoZenLocalFallback` (the local Zen DDC path asserted in `DerivedDataRequestOwner.cpp` and crashed the editor), waits for MCP, and turns background throttling off. Double-clicking the `.uproject` can open the Epic Games Launcher instead.

MCP working notes (learned in Milestone 2):
- **Running Python in the open editor:** MCP has no Python tool. Use `SlateInspectorToolset`: `Snapshot` to find the status-bar console textbox next to the `Cmd` label (ref `tb2` so far, but refs can change after a restart), `Click` it, then `Type` with `submit: true` the text `py exec(open(r'<path with spaces is fine>').read())`. Output goes to `Saved/Logs/OurLastEcho.log`; wait for it with a Bash until-loop on the marker. The same box runs any console command (`LiveCoding.Compile` for a Live Coding build, `rhi.DumpMemory`, cvars).
- `editor_toolset.toolsets.programmatic.ProgrammaticToolset.execute_tool_script` batches many MCP calls in one round trip. It only has sandboxed Python (json/re/math/time), not the `unreal` module.
- Useful toolsets: `EditorAppToolset` (`StartPIE`/`StopPIE`/`IsPIERunning`, `CaptureViewport`, `SearchCVars`), `SceneTools`, `ObjectTools` (can set properties Python can't reach, e.g. `bThrottleCPUWhenNotForeground` on `/Script/UnrealEd.Default__EditorPerformanceSettings`), and `PCGToolset` (build graphs node by node, `ExecuteGraphInstance`, `GetNodeDataView` for point counts; attribute selectors are plain strings like `.Z`).
- Screenshots (`CaptureViewport`, `SlateInspectorToolset.Screenshot`) come back as megabytes of base64 that get dumped to a tool-results file. Decode that file to a JPEG and `Read` it.
- **Don't quit the editor through MCP** (`QUIT_EDITOR`): the pending MCP call keeps the editor stuck in "Preparing to exit". Ask the user, or make sure everything is saved and end the process.
- While the editor is open you can still compile-check C++ by building the **Game** target (`Build.bat OurLastEcho Win64 Development ...`). It doesn't touch the editor DLL.
- A background editor is throttled (about 3–8 fps) unless background CPU throttling is turned off (see "Learned in Milestone 4"); with it off, 2-player PIE ran at ~45 fps in the background. Real frame-rate numbers still need the editor focused.
- **If this session's MCP connection failed** (e.g. the session started before the editor), the tools stay unavailable until the connector is re-dialled at the end of a turn. Rather than stopping, drive the same server directly with `Scripts/tools/` (`mcp.ps1`, `ue_cmd.ps1`, `ue_py.ps1`; see its README).
- **Python can't test client-to-server RPCs.** While editor Python runs, `GAllowActorScriptExecutionInEditor` makes `AActor::GetFunctionCallspace` return local, so a Server RPC called from Python on a client actor just runs on the client. Test RPC paths by typing console commands (`ue_cmd.ps1`), and test movement through real input (`add_movement_input`/`jump` on the client pawn, which go through the normal saved-move RPCs).
- During PIE, `EditorAssetLibrary` refuses to run: use `unreal.load_asset('/Game/...Asset.Asset')`. `get_current_level` returns empty while PIE runs.
- A client teleported by the server needs about 1.5 s before scripted input on it is reliable (position corrections).
- The **Message Log** tab reopens at every PIE start and covers `CaptureEditorImage`. Close it through its tab's close button (`SlateInspectorToolset`) before capturing. `CaptureViewport` doesn't work during PIE; use `CaptureEditorImage` for Bat's view and `SlateInspectorToolset.Screenshot` on the "Client 1" window for Saraa's.
- Material colours are **linear**: dark colours need small values, and glow above ~1.5 tonemaps to white in the canyon's auto-exposure.

Learned in Milestone 4:
- **Turn off background throttling after every editor start**, before any live test. Otherwise two PIE worlds run at ~3 fps in the background, and every scripted move (even walking) draws network corrections. With it off, they run at ~45 fps. The setting resets on restart:
  `powershell -File Scripts/tools/mcp.ps1 -Tool set_properties -Toolset editor_toolset.toolsets.object.ObjectTools -ArgsJson '{"instance":{"refPath":"/Script/UnrealEd.Default__EditorPerformanceSettings"},"values":"{\"bThrottleCPUWhenNotForeground\":false}"}'`
  (`get_properties` takes `{"instance":{...},"properties":[...]}`.) Check with `Scripts/measure_fps.py`.
- **Network emulation in PIE:** `NetEmulation.PktLag <ms>`, `NetEmulation.PktLagVariance <ms>`, `NetEmulation.PktLoss <percent>` console variables apply to the running session; set them back to 0 afterwards. Read the effect from a PlayerState's `get_ping_in_milliseconds()`.
- **Unity builds merge .cpp files:** names in anonymous namespaces must be unique across the module (a second `GlowParam` broke the build).
- Using `FVector_NetQuantize*` serialization in our code needs the **`NetCore`** module (the monolithic Game target linked without it; the editor DLL didn't).
- `CaptureViewport` needs every parameter: `captureTransform` (`location` / `rotation` / `scale`), `annotations` (all fields, e.g. `gridSpacing` 0, `maxLabels` 0, `classFilter.refPath` "/Script/Engine.Actor") and `bShowUI`.
- In PIE the **Message Log is its own window**. Close it by clicking the small button inside its tab (find it with `Snapshot` on the window's splitter). Window refs (e.g. the "Client 1" window) change every PIE session, so look them up again before `Screenshot`.
- **Live Coding** is fine for .cpp-only changes (`LiveCoding.Compile`, then wait for "Live coding succeeded" in the log). If the editor dies afterwards, the patch is gone: rebuild with `Build.bat` before relaunching.
- When the editor hangs with `CrashReportClientEditor` running, it has crashed: read `Saved/Crashes/<newest>/CrashContext.runtime-xml`, then end both processes.
- PowerShell `Set-Content -Encoding utf8` writes a BOM, which Python in the editor rejects (`U+FEFF`). Edit scripts with the Edit tool or `sed`.

Learned in Milestone 5:
- **PIE setups:** `Scripts/tools/pie_mode.ps1 -Mode frontend -Players N` (TitleScreen, Standalone: each window is its own game) or `-Mode gameplay` (Lvl_SpiritPath, listen server). `LevelEditorPlaySettings` isn't reachable from Python; it's set through MCP `ObjectTools`.
- **Editor Python runs RPCs locally in both directions,** also in `-game` processes started from `UnrealEditor.exe`: a server-side `ServerTravel` from Python made the *host* run the guest's `ClientTravel`. Queue such clicks with `EchoUITestLibrary.QueueClickWidget` / `QueueKey` (next engine tick, outside Python).
- **UI test input** (`UEchoUITestLibrary`) goes through Slate:
  - CommonUI ignores the first mouse move after gamepad input, and the click that switches input type, so hover (two moves) before clicking.
  - PIE windows share one cursor, and a viewport in game keeps mouse capture: release capture before hovering another window.
  - The editor's console box keeps keyboard focus after `ue_cmd`: focus the game first.
  - The **Message Log window** opens over the viewport at PIE start and catches clicks: `close_message_log.ps1`.
- **Live Coding + unity build:** a patch recompiled `EchoGameMode.cpp` alongside another file, and its file-local `TAutoConsoleVariable` was never constructed (null crash at map load). Do a full rebuild before multiplayer or travel tests.
- **Python gotchas:**
  - Array elements (e.g. IMC mappings) come back as copies: edit, collect, and set the whole list back.
  - A controller's pawn: `GameplayStatics.get_player_pawn(world, 0)` (`get_pawn` / `k2_get_pawn` aren't exposed).
  - `SubsystemBlueprintLibrary` isn't available in `-game`.
  - Order PIE worlds by `get_path_name()` (`UEDPIE_<n>_`), not `get_name()` (the same after travel).
- **`-nullrhi` games can't drive CommonUI** (screen transitions never finish). Run test games windowed, at `scalability 0`: two full-quality copies starve the 6 GB GPU.
## C++ vs Blueprint split (important)

`.uasset` (Blueprints, widgets, materials, etc.) and `.umap` (levels) are **binary**, so Claude can't read or edit them directly. Therefore:

- **Game logic and state go in C++.** Blueprints should be thin subclasses of C++ classes, used for assigning meshes, materials, sounds, animations and tuning values.
- Expose tunables as `UPROPERTY(EditAnywhere, BlueprintReadWrite)` so the user can adjust them in the editor without code changes.
- **UI:** C++ `UUserWidget` base classes with `UPROPERTY(meta=(BindWidget))` members. The user lays out the UMG widget Blueprint visually and names widgets to match.
- To create or modify levels and assets in bulk, write a Python builder script (see above) rather than asking the user to do lots of repetitive clicking.
- When a change also needs editor-side work (reparenting a Blueprint, assigning an asset, placing an actor), list the exact steps for the user.

## Content & config

- Default **game** map: **`/Game/Echo/Maps/TitleScreen`** (Milestone 5). Default **editor** map: **`/Game/Echo/Maps/Lvl_SpiritPath`** (game mode `BP_EchoGameMode` via its World Settings override). The headless tests pass the map explicitly. The project-wide default game mode is still the template's `BP_ThirdPersonGameMode`.
- Game content lives under `/Game/Echo/` (Blueprints, Materials, Maps, PCG). Greybox geometry uses `/Engine/BasicShapes/Cube` + `/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray`.
- PIE defaults to **2 players, Play As Listen Server** (`Config/DefaultEditorPerProjectUserSettings.ini`, also set in the user's own `Saved/` copy).
- Template levels: `ThirdPerson/Lvl_ThirdPerson` and the `Variant_*` levels. These use World Partition / One File Per Actor (`Content/__ExternalActors__`); `Lvl_SpiritPath` does not.
- Enabled plugins: StateTree, GameplayStateTree (used by the template AI), ModelingToolsEditorMode, PythonScriptPlugin, EditorScriptingUtilities, PCG, ModelContextProtocol, AllToolsets, and since Milestone 5 CommonUI, OnlineSubsystem, OnlineSubsystemNull, OnlineSubsystemUtils, Niagara, EngineCameras. There's no Starter Content in this engine install; use engine BasicShapes and `/Game/LevelPrototyping` meshes.
- `Config/DefaultEngine.ini` `[SystemSettingsEditor]` halves Lumen's surface cache atlas **in the editor only**. 2-player PIE renders two worlds on one 6 GB GPU and ran out of video memory otherwise.

## Git

- Remote `origin` → `https://github.com/Invisibleglass/OurLastEcho.git`, default branch `main`. GitHub's free LFS quota is 1 GB of storage and 1 GB/month of bandwidth (the initial push was about 141 MB), so watch it once large art or audio arrives.
- `.uasset`, `.umap` and source-art/audio files are tracked with **Git LFS** (see `.gitattributes`). Don't bypass LFS for binaries; GitHub rejects files over 100 MB.
- The user asked for a commit after each working step, with clear messages. In PowerShell, commit messages containing double quotes break `git commit -m`. Write the message to a temp file as UTF-8 **without BOM** and use `git commit -F`.
- Ignored: `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`, generated `.sln`/`.slnx`, and `Content/Developers/`. The solution can be regenerated with right-click `.uproject` → *Generate Visual Studio project files*.
