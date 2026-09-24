# Our Last Echo

An online 2-player co-op game in Unreal Engine 5. **Bat** (player 1, the listen-server host) is a grieving brother in the present-day canyon. **Saraa** (player 2) is his late sister, in the canyon's ancient past, the afterlife. Core idea: each player sees and touches things the other can't.

Started from the **Third Person C++ template**. Its Combat/Platforming/SideScrolling variant code and levels are still in the project but unused. Work is organised into milestones from briefs the user provides; see `NOTES.md` for the latest milestone's status and test steps.

## Gameplay architecture (Milestone 1)

- **Realms:** `EEchoRealm` (Living/Spirit) in `Source/OurLastEcho/Echo/EchoTypes.h`. It's a class default on `AOurLastEchoCharacter` (`Realm`). Bat = `BP_ThirdPersonCharacter` (Living, unchanged template BP); Saraa = `BP_Saraa` (Spirit, `M_Ghost` material).
- **Realm-only collision:** Spirit capsules use object channel **`SpiritPawn` = `ECC_GameTraceChannel1`**, defined in `Config/DefaultEngine.ini` (`#define ECC_SpiritPawn` in EchoTypes.h; keep the two in sync). Spirit platforms block only that channel. Because the channel choice is deterministic on every machine, client movement prediction agrees with the server. Any new trigger/volume must explicitly set Overlap for **both** `ECC_Pawn` and `ECC_SpiritPawn`. Engine profiles that list channels explicitly (Trigger, OverlapAll) fall back to the custom channel's default, **Block**.
- **Realm-only visibility** is local per machine (`UPrimitiveComponent::SetVisibility`, based on the local player's pawn realm). Never use `SetActorHiddenInGame` for this, because it replicates.
- **Networking:** listen server. Server-authoritative gameplay state is in replicated flags (`bActivated`, `bRaised`, `AEchoGameState::bMilestoneComplete`); cosmetic motion (the bridge rising) is animated locally from the flag. `AEchoGameMode` gives the first controller Bat and all later ones Saraa.
- Placeable actors (`AEchoSpiritPlatform`, `AEchoSpiritSwitch`, `AEchoRisingBridge`, `AEchoEndZone`, `AEchoRespawnVolume`) use engine BasicShapes meshes and **soft-path default materials** under `/Game/Echo/Materials/`. Their sizes are `...Size` properties (in cm) applied in `OnConstruction`, and the actor origin is the walking surface.

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
- `Scripts/run_spirit_path_test.ps1` (runs `test_spirit_path.py`): the **end-to-end test**. It launches a headless game (`-game -nullrhi`), summons a `BP_Saraa`, and uses `py` via `-ExecCmds` to sweep both characters around the level, checking collision, switch, bridge, respawn and end zone. Run it after gameplay changes: `powershell -File Scripts/run_spirit_path_test.ps1`. The last line is PASS/FAIL.

Headless scripting gotchas learned the hard way:
- Pass `-script=` paths with **forward slashes**. In `Scripts\test...` the engine reads `\t` as a tab character.
- The commandlet's editor world has **no collision** (traces hit nothing), and `PostInitializeComponents` doesn't run for actors spawned there. So anything that tests physics or realm collision must run in a real game world (`UnrealEditor.exe ... -game -nullrhi -ExecCmds="py <path-without-spaces>"`). In a game world, Python can't spawn actors; use the `summon` cheat (`EnableCheats` first). Use `unreal.register_slate_post_tick_callback` to wait for things over time, and call `quit` yourself at the end.
- Python names drop the `b` prefix on bools (`bActivated` → `activated`); custom channels appear as `unreal.CollisionChannel.ECC_SPIRIT_PAWN`; `K2_SetActorLocation` is `set_actor_location`.

## C++ vs Blueprint split (important)

`.uasset` (Blueprints, widgets, materials, etc.) and `.umap` (levels) are **binary**, so Claude can't read or edit them directly. Therefore:

- **Game logic and state go in C++.** Blueprints should be thin subclasses of C++ classes, used for assigning meshes, materials, sounds, animations and tuning values.
- Expose tunables as `UPROPERTY(EditAnywhere, BlueprintReadWrite)` so the user can adjust them in the editor without code changes.
- **UI:** C++ `UUserWidget` base classes with `UPROPERTY(meta=(BindWidget))` members. The user lays out the UMG widget Blueprint visually and names widgets to match.
- To create or modify levels and assets in bulk, write a Python builder script (see above) rather than asking the user to do lots of repetitive clicking.
- When a change also needs editor-side work (reparenting a Blueprint, assigning an asset, placing an actor), list the exact steps for the user.

## Content & config

- Default game/editor map: **`/Game/Echo/Maps/Lvl_SpiritPath`** (game mode `BP_EchoGameMode` via its World Settings override). The project-wide default game mode is still the template's `BP_ThirdPersonGameMode`.
- Game content lives under `/Game/Echo/` (Blueprints, Materials, Maps). Greybox geometry uses `/Engine/BasicShapes/Cube` + `/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray`.
- PIE defaults to **2 players, Play As Listen Server** (`Config/DefaultEditorPerProjectUserSettings.ini`, also set in the user's own `Saved/` copy).
- Template levels: `ThirdPerson/Lvl_ThirdPerson` and the `Variant_*` levels. These use World Partition / One File Per Actor (`Content/__ExternalActors__`); `Lvl_SpiritPath` does not.
- Enabled plugins: StateTree, GameplayStateTree (used by the template AI), ModelingToolsEditorMode, PythonScriptPlugin, EditorScriptingUtilities.

## Git

- Remote `origin` → `https://github.com/Invisibleglass/OurLastEcho.git`, default branch `main`. GitHub's free LFS quota is 1 GB of storage and 1 GB/month of bandwidth (the initial push was about 141 MB), so watch it once large art or audio arrives.
- `.uasset`, `.umap` and source-art/audio files are tracked with **Git LFS** (see `.gitattributes`). Don't bypass LFS for binaries; GitHub rejects files over 100 MB.
- The user asked for a commit after each working step, with clear messages. In PowerShell, commit messages containing double quotes break `git commit -m`. Write the message to a temp file as UTF-8 **without BOM** and use `git commit -F`.
- Ignored: `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`, generated `.sln`/`.slnx`, and `Content/Developers/`. The solution can be regenerated with right-click `.uproject` → *Generate Visual Studio project files*.
