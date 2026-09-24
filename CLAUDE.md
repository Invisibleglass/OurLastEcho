# Our Last Echo

An Unreal Engine 5 C++ game project. Started from the **Third Person C++ template** with all three template variants included (Combat, Platforming, SideScrolling). Actual game design hasn't been decided yet; treat the template code as a starting point to build on or delete, not as settled architecture.

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

## C++ vs Blueprint split (important)

`.uasset` (Blueprints, widgets, materials, etc.) and `.umap` (levels) are **binary**, so Claude can't read or edit them directly. Therefore:

- **Game logic and state go in C++.** Blueprints should be thin subclasses of C++ classes, used for assigning meshes, materials, sounds, animations and tuning values.
- Expose tunables as `UPROPERTY(EditAnywhere, BlueprintReadWrite)` so the user can adjust them in the editor without code changes.
- **UI:** C++ `UUserWidget` base classes with `UPROPERTY(meta=(BindWidget))` members. The user lays out the UMG widget Blueprint visually and names widgets to match.
- To create or modify levels and assets in bulk, write a Python builder script (see above) rather than asking the user to do lots of repetitive clicking.
- When a change also needs editor-side work (reparenting a Blueprint, assigning an asset, placing an actor), list the exact steps for the user.

## Content & config

- Default game/editor map: `/Game/ThirdPerson/Lvl_ThirdPerson`. Default game mode: `BP_ThirdPersonGameMode` (a Blueprint subclass of `AOurLastEchoGameMode`).
- Template variant levels: `Variant_Combat/Lvl_Combat`, `Variant_Platforming/Lvl_Platforming`, `Variant_SideScrolling/Lvl_SideScrolling`.
- The project uses World Partition / One File Per Actor (`Content/__ExternalActors__`, `__ExternalObjects__`). Placed actors are stored as individual small `.uasset` files there, not inside the `.umap`.
- Enabled plugins: StateTree, GameplayStateTree (used by the template AI), ModelingToolsEditorMode, PythonScriptPlugin, EditorScriptingUtilities.

## Git

- `.uasset`, `.umap` and source-art/audio files are tracked with **Git LFS** (see `.gitattributes`). Don't bypass LFS for binaries; GitHub rejects files over 100 MB.
- Ignored: `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`, generated `.sln`/`.slnx`, and `Content/Developers/`. The solution can be regenerated with right-click `.uproject` → *Generate Visual Studio project files*.
