# Milestone 8 – First combat (a separate test level)

Branch: `milestone-8-combat`, branched from `milestone-7-online` (Milestone 7 isn't merged yet: the friend test is still to do). Nothing was pushed.

**In short:** both players can now fight. There's a reusable health and damage system, Saraa's whip combo and charged lash, Bat's quick, aimed and charged shots, a dodge roll for both, and four enemy types that only work as a team. They're in a new **CombatTest** arena with four waves, reached through a door near the start of the first level.

**Status (2026-10-18):** everything in the brief is built, tested and committed. The combat test passes 56/56 in both roles, with and without network emulation; the doors work both ways in Play-In-Editor and between two real LAN game processes; and every earlier test still passes (see TEST_REPORT.md). **Not yet run:** the doors over real EOS (it needs the Dev Auth Tool running with both accounts signed in).

## How to try it

1. Play the first level as usual (PIE, or host and join from the title screen).
2. Near the start, a glowing **"Combat Test"** door stands on the left side of the start platform, facing in. Either player walks into it and **both** go to the arena (the door arms 2 s after the level starts, so nobody goes through by accident).
3. In the arena, walk forward out of the entrance to start wave 1. A banner at the top says the wave, how many enemies are left, and a hint.
4. Behind the start: the **return door** (back to the first level, beside the Combat Test door) and a **Restart arena** button on a plinth (step on it).
5. Or open `/Game/Echo/Maps/CombatTest` in the editor and press Play.

## Controls

| | Keyboard and mouse | Gamepad |
|---|---|---|
| **Saraa: light combo** | tap LMB (or E): lash, lash, wide finisher | right trigger |
| **Saraa: charged lash** | hold LMB, release (a long heavy lash) | hold right trigger |
| **Bat: quick shot** | LMB without aiming (fast, weak, no reticle needed) | right trigger |
| **Bat: aimed shot** | hold RMB to aim, tap LMB | left trigger + right trigger |
| **Bat: charged shot** | while aiming, hold LMB about 1 s (the reticle fills), release | hold right trigger while aiming |
| **Dodge roll (both)** | Left Shift | B / Circle |

Dodge is rebindable in Settings > Controls ("Dodge"). Saraa's whip still swings from anchors when one is in range: the lash only happens when there's no anchor to latch.

## What was built

All game logic is C++ in `Source/OurLastEcho/Echo/Combat/`. The Blueprints are thin tuning subclasses.

- **Health and damage** (`UEchoHealthComponent`, on players and enemies; ready for bosses):
  - **The server decides all damage.** Clients only ask to attack: Bat's shots and Saraa's lashes are server RPCs with cooldown and charge-time checks, and the server spawns the arrow or does the lash sweep.
  - Teams (players / enemies) never hurt their own side. An owner can filter every hit (`IEchoDamageFilter`); that's how the shells, stuns and "not in my world" rules work. The results are Applied, Defeated, Blocked (a clink), Opened (cracked or knocked down), Immune and Ignored.
  - Every hit is sent to every machine as a cue (`FEchoHitCue`): the hit flash, the sound, and the **hit-stop** on heavy hits (the enemy freezes for 0.07 s).
- **Players** (`UEchoPlayerCombatComponent`):
  - **Dodge roll** with **i-frames** (0.45 s), a 0.75 s cooldown, about 6.5 m. It's predicted movement like the whip swing: the roll is a flag in the saved move, so the server replays exactly the client's roll and nobody gets corrected.
  - **Health bar** bottom-left for you and your partner, a red screen flash, a camera shake and a flinch when hurt.
  - **Downed** at zero: the player lies down, can't move or attack, and a countdown shows. After 3 s they get back up **beside the partner** at full health with 1.5 s of safety. **Both down:** after 2.5 s the arena restarts from its entrance checkpoint (elsewhere: everyone goes back to their checkpoints).
- **Saraa's whip** (`UEchoSwordWhipComponent`): light lash 12, light lash 12, then a finisher (20) that sweeps a wide arc. Hold for the charged lash (40, 7.8 m long, a coil shows while charging).
- **Bat's bow** (`UEchoSpiritBowComponent`): quick shot 10, aimed shot 20, charged shot 45 (bigger, brighter, faster, heavy). The aim trace ignores players, so Saraa standing in front of Bat doesn't catch his shots.
- **Enemies** (`AEchoEnemy` and the four types in `EchoEnemies.h`). They live in the shared layer, and each machine shows them to its own player only when they're in that player's world.
  - **Dust Wraith** (present only): only Bat sees it, hits it, bumps into it, or is attacked by it. A swirling dust column that drifts in and claws.
  - **Shade** (past only): the same for Saraa. A dark hooded shape with pale eyes.
  - **Stone Guardian** (Saraa first): both see it. Arrows bounce off its shell with a clink. **Saraa's whip cracks the shell** (a crack sound, the shell slabs move apart, the core glows), and for **4 s** Bat's arrows hurt it while the glow fades; then the shell closes. Her whip never hurts it. Its attack is a slow, heavy slam.
  - **Sky Talon** (Bat first): both see it. It circles at 6.5 m, out of the whip's reach (the whip clinks off it). **Bat's arrow knocks it down**: it falls, a stun ring spins over it, and for **4 s** Saraa's whip hurts it while Bat's arrows do nothing. Then it flies back up. Its attack is a screech while hovering, then a dive.
  - **Cues for the open moments:** a sound, a glow and an **OPEN timer bar** under the enemy's health bar that runs down. A hit that does nothing gives a spark and a clink.
  - **The brain is a StateTree** (`ST_EchoEnemy`, built from C++ by `UEchoEditorLibrary::BuildEnemyStateTree`): Stunned → Attack → Chase → Idle, with conditions and tasks that call into the enemy. Targets come from **AI Perception** (sight), with an awareness radius as a fallback, and an enemy only ever targets a player whose world it's in. Every attack is **telegraphed**: a wind-up (it swells and glows, with a warning sound), then the strike, then a recovery.
  - **Health bars** float over enemies, only on the machines whose player can see them.
  - **Defeat:** a pop and a fade, then it's removed.
- **The arena** (`AEchoArenaManager`), with these waves:
  1. 2 Dust Wraiths + 2 Shades
  2. 2 Stone Guardians
  3. 2 Sky Talons
  4. one of each

  It waits 4 s between waves and ends on "Arena cleared".
  - The start zone begins it, the entrance checkpoint is where both restart, and the **Restart arena** pad (`AEchoArenaRestartPad`) starts over from wave 1.
  - All of it is tunable on the placed actor: the waves, enemy classes, spawn points and delays.
- **Doors** (`AEchoTravelDoor`): a frame with a glowing portal and a label. **Either player** walking in sends **both** to the other map. They arrive at the PlayerStarts tagged for the door (`CombatDoor_Bat`/`_Saraa` in the first level, `ArenaEntrance_Bat`/`_Saraa` in the arena).
  - Online (EOS) the travel is **seamless**, so the session, the peer-to-peer connection and voice stay up. On LAN and in PIE it's a hard travel, and the guest reconnects.
  - The eras load for each player after every trip.
- **The level:**
  - **`CombatTest`** is the shared layer: a greybox canyon arena, about 50 m across, with terraced walls, four pillars, rocks, and a raised ledge with a ramp for Bat. It also has the entrance, the return door, the restart pad and the spawn points.
  - **`CombatTest_Present` / `CombatTest_Past`** are its era layers (the same pieces in each era's materials, lighting and ambience), loaded per player by its own `EraDirector`.
  - **The first level only got the door** and its two arrival PlayerStarts.
- **Tuning in Blueprint:** `BP_DustWraith`, `BP_Shade`, `BP_StoneGuardian`, `BP_SkyTalon` (health, speed, damage, wind-up times, open/stun times, colours, sounds). Player combat is tuned on the components in `BP_ThirdPersonCharacter` and `BP_Saraa`. Sounds are hooks, filled with engine placeholder sounds for now.

## Debug console commands

| Command | What it does |
|---|---|
| `EchoGod` | god mode for yourself (immune; "GOD MODE" on the HUD) |
| `EchoSpawnEnemy wraith/shade/guardian/talon` | spawns one in front of you |
| `EchoShowAllEnemies` | shows every enemy to both players (and their bars) |
| `EchoSkipToWave N` | jumps to wave N (1–4) |
| `EchoRestartArena` | restarts the arena |
| `EchoHurt N` | hurts yourself by N |
| `EchoGoTo combat` / `EchoGoTo back` | both players to the arena / back to the first level (the doors' own travel) |
| `EchoShoot quick/aimed/charged`, `EchoLash combo/charged`, `EchoDodge` | attacks without input (used by the tests) |

All of them work from either player; the guest's go to the server.

## How to rebuild it

`Scripts/build_combat.py` (open editor; tag `CombatBuilder`) builds the materials, the dodge input, the enemy Blueprints, the StateTree, the CombatTest map and its era maps, and the door in the first level. Re-running it rebuilds the arena from scratch and replaces only its own door in the first level.

## How to test

- `powershell -File Scripts/tools/run_combat_test.ps1 -Mode normal` (editor open): 56 checks in a fresh 2-player PIE session on CombatTest. `-Mode host_saraa` swaps roles, `-Mode travel` walks through both doors, `-NetEmu average|bad` adds lag and loss.
- `powershell -File Scripts/run_lan_test.ps1` (editor closed) now also sends two real game processes to the arena and back.

## Known issues

- **Greybox and placeholder sound:** enemies are built from engine shapes and the sounds are engine editor sounds; there are no attack animations yet (the hurt, roll and downed poses are simple mesh tilts and spins).
- **Weapons in hand (after the milestone, from the user's playtest):** Bat always holds the bow by its grip in his left hand: carried flat at his hip (limbs forward and back, string below), upright while aiming, and arrows leave from it. Saraa always holds the sword in her right hand, blade forward and down; it turns to point along the whip while she lashes, charges or swings. **Right mouse / left trigger** now also moves Saraa's camera in over her shoulder (a gentler version of Bat's aim; same Hold/Toggle setting; `IMC_Focus`). Grips and zoom are tunable on the components. With no aim animation, Bat's arm stays down while aiming, so the bow sits at his side; an aim pose would fix that.
- **The doors over real EOS aren't tested yet** with two processes. They use the same seamless travel as the lobby's Start Game, which is tested.
- **Hit-stop** freezes only the enemy hit, not the whole game, so it stays fair online; it's subtle.
- AI movement uses the navmesh, with direct steering as a fallback; flyers steer directly. Enemies can bunch up around a pillar.

## Suggestions for Milestone 9

- Add real attack, hurt and roll animations (montages) on the existing hooks.
- A boss using the same health component and filters: phases that need both players, like the Guardian and the Talon.
- Combat in the real levels, where the Climb or the Crossing ends in a fight.
- Real sounds and VFX (Niagara) for the hits, cracks, knockdowns and defeats.
- Revive by your partner (hold a button near them) instead of a timer.

# Milestone 7 – Real online play with Epic Online Services

Branch: `milestone-7-online`, branched from `main` (Milestone 6 was merged). Nothing was pushed.

**In short:** the game now signs players in with their Epic accounts and plays over the internet through Epic Online Services (EOS). Friends join with an invite or a short join code, talk with voice chat, and connect peer to peer through EOS's relay, so nobody opens router ports. LAN (Online Subsystem Null) still works as a fallback.

**Status (2026-10-16, paused at the user's request):** everything is built and committed. Real EOS works end to end with two accounts on one PC (see TEST_REPORT.md), and a packaged build is made. **Later the same day:** the EOS lag tests (average and bad) and the 30-minute soak passed: no disconnects, 0.1 cm drift at most, ping settling at about 16 ms once EOS found a direct route, and flat memory. **Still to do:** the real two-PC test with a friend (`MANUAL_TEST.md`). The Dev Auth Tool's credentials are named `Default` and `Default2`.
## Setup (one time)

- **EOS IDs and the client secret** live in `Config/Windows/WindowsEngine.ini`, which git ignores; the repo is public. A blank copy for sharing is `Config/EOSCredentials.example.ini`. They are packaged with the game, as Epic expects for game clients.
- **Developer Portal** (product "Our Last Echo", organization Invisibleglass Productions):
  - client "Test of online" with the Peer2Peer client policy (sessions, lobbies, voice, P2P)
  - Epic Account Services: permissions Basic Profile, Online Presence and Friends; the client linked
  - Brand settings aren't done, so the app is in **Draft**: players see an "unreviewed" notice at sign-in, and only accounts in the organization can sign in. **Add testers' Epic accounts under Organization > Team members.**
  - The product has only the **Live** sandbox, which the IDs point to.
- **Windows Smart App Control must be off** on any PC that runs the editor or a build: it blocks every freshly built, unsigned game DLL or EXE ("Bad Image", error 0xc0e90002). It was turned off on this PC on 2026-10-16.

## The switch: EOS or LAN

- **One setting:** `[OnlineSubsystem] DefaultPlatformService` in `Config/DefaultEngine.ini`: `EOS` (default) or `Null` (LAN, no login, no voice).
- **A game process can override it:** `-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=Null` (the LAN test does this).
- **Play-In-Editor uses `PIEOnlineService`** in `Config/DefaultGame.ini` (default `Null`).
  - PIE connects its windows by IP, which EOS's peer-to-peer sessions can't use, and every PIE window would need its own Epic login.
  - So the editor tests run on LAN, and EOS is tested with real game processes or a packaged build.
- **Connections:** the game net driver is EOS's `NetDriverEOS`.
  - EOS games listen peer to peer: the connect address is the host's EOS user ID, relayed by EOS when a direct route fails.
  - LAN games add `?bUseIPSockets` to the listen URL, and the driver falls back to plain IP.
  - The editor keeps the engine's IP driver.

## What was built

All of it is in C++ under `Source/OurLastEcho/Echo/Online/` plus changes to the session and menu code.

- **Login** (`UEchoLoginSubsystem`, `UEchoLoginScreen`):
  - It's the first screen of a run: "Signing in with your Epic account...", then the main menu ("Signed in as <name> (Epic account)").
  - The first time, the Epic account portal opens (the overlay or a browser). After that the login is remembered (`bPreferPersistentAuth`).
  - On failure: a plain-language reason (no internet, cancelled, setup incomplete, not in the organization, Dev Auth Tool not running), **Try Again** and Quit.
  - LAN mode signs in instantly as the computer's name.
- **Hosting** creates an EOS lobby session: 2 players, presence on, invites on, a voice room, and a 6-character **join code** from letters and digits that are easy to read aloud (no 0/O or 1/I/L). The lobby shows "Join code: K7Q2MD" to the host.
  - *Private, simplest version (noted as the brief asks):* the lobby is advertised, because EOS can only search advertised lobbies and the code search needs that. But no screen ever lists internet games, so in practice only someone with the code or an invite gets in.
  - A fixed bucket ID (`OurLastEcho`) means editor-launched and packaged builds find each other.
- **Invites:**
  - **Invite Friend** opens the Epic friends overlay. OSS EOS has no "invite dialog" (`ShowInviteUI` isn't implemented), but its friends overlay invites to the current presence lobby.
  - **Invite from Friends List** is an in-game list of Epic friends (online first) with one-click invites, for when the overlay isn't available.
  - **Accepting** an invite in the overlay joins the host's lobby straight away, from anywhere in the game.
  - An invite that arrives in game, without the overlay, shows "<name> invited you. Join?" on the main menu.
  - *While the game is closed:* invites wait in the Epic overlay (Shift+F3) and can be accepted after starting the game. A build outside the Epic Games Store can't be *launched* by an invite; only the store's launcher does that.
- **Join by code:** Play > Join Game has a code box (any case; spaces and dashes are ignored) and **Join with Code**. LAN mode also lists the games on the network, as before.
  - Clear messages for a wrong code, a full game, an expired invite, a host that doesn't answer (20 s), and a lost connection.
  - **Cancel** on "Joining..." gives up at any point.
- **Voice chat** (`UEchoVoiceSubsystem`): EOS voice through the lobby's voice room, which both players join with the session automatically. It lasts through the lobby and the game.
  - Settings > Audio: **Microphone** on/off, **Voice mode** (Open mic / Push-to-talk), **Push-to-talk key** (default V, rebindable; gamepad: left stick click), **Voice chat volume** (under the master volume).
  - Push-to-talk reads raw key events (a Slate input pre-processor), so it works in menus and in game and never eats the key.
  - **Speaking indicator:** a blue "((( <name> is speaking" box bottom-left while the other player talks, and a red "Mic on" box while you hold push-to-talk.
- **Disconnects:**
  - **Guest leaves or drops mid-game:** the host returns to *their lobby*. The session and join code stay, so the friend can rejoin with the same code. The host sees "Saraa left the game."
  - **Host leaves:** the host tells the guest first, and the guest goes to the title screen with "The host left the game." A host that drops without warning gives the guest the same message, or "Lost the connection to the host" after a timeout.
  - **Different builds:** "You and the host have different versions of the game."
  - **Never stuck on black:** the era fade-in gives up waiting after 20 s (`MaxBlackScreenSeconds`) and shows the game anyway.
  - Every failed join ends on a message with OK and the menus behind it.
- **Diagnostics** (`UEchoNetDiagnostics`, `UEchoOnlineOverlay`):
  - **Network overlay**, top right: transport (EOS P2P / IP), ping, packet loss in/out, KB/s in/out. Toggle it in Settings > Graphics > Network stats overlay, or with the console command `EchoNetStats`.
  - **Session log** per run: `Saved/Logs/OnlineSessions/Online-<date-time>.log`. It records sign-in, hosting with the code, joins, invites, lobby arrivals and departures, map changes, connects and disconnects, network failures, voice room changes, and a network sample every 10 s (ping, loss, bandwidth, memory).
  - `EchoLogNote <text>` marks a moment in the log.
  - The EOS SDK's own start-up messages print the product IDs, so only its warnings and errors are logged (`[Core.Log] LogEOSSDK=Warning`).
- **Console commands added:** `EchoJoinCode <code>`, `EchoNetStats`, `EchoLogNote <text>`. `EchoHost`, `EchoReady` and the rest still work.

## Two Epic accounts on one PC (Developer Authentication Tool)

1. **Get the tool:** it ships with the EOS SDK (portal > SDK download). It's unzipped at `D:\EOS_SDK\DevAuthTool\EOS_DevAuthTool.exe`.
2. **Start it:** run the tool, set the **Port** to `6300`, and click **Start**.
3. **Log in twice:** sign in with the first Epic account and the second; here the credentials are named `Default` and `Default2` (the test script's defaults; pass `-HostCredential`/`-GuestCredential` for other names). Both accounts must be in the portal organization, and should be Epic friends to test invites.
4. **Run the game with a credential:** add `-EchoDevAuth=Default` (or `Default2`) to its command line. It signs in through the tool instead of the account portal; the tool's address is `DevAuthHost` in DefaultGame.ini.
5. **Run the two-player test:** `powershell -File Scripts/run_eos_test.ps1` (editor closed) starts both games and plays a full round. Add `-NetEmu average` or `-NetEmu bad` to add lag and packet loss.

## Packaged build and playing from another PC

**Build it** (editor closed):
```
powershell -File Scripts/package_game.ps1
```
It makes `Packaged/Windows/` (the game, with `Play Our Last Echo.exe` and `READ ME FIRST.txt`) and `Packaged/OurLastEcho-Win64.zip`. `Packaged/` is git-ignored.
- `Play Our Last Echo.exe` is the **EOS Bootstrapper**: it makes sure Epic Online Services, which the overlay needs, is installed, then starts `OurLastEcho.exe`.
- The build includes Microsoft's runtime installer (prerequisites).

**Send it:**
- Upload the zip to Google Drive, OneDrive or Dropbox and share the link. It's too big for most email or chat attachments.
- Before testing together, add your friend's Epic account to the portal organization, and become Epic friends.

**Your friend, on their PC:**
1. Download and **unzip the whole folder**; don't run it from inside the zip.
2. Make sure Windows **Smart App Control is off** (Windows Security > App & browser control), or Windows blocks the game.
3. Run `Play Our Last Echo.exe`. If SmartScreen warns, click More info > Run anyway: the test build isn't signed.
4. Sign in with their Epic account.
5. Join you: accept your invite (Epic overlay, Shift+F3), or choose Play > Join Game, type your join code, and press **Join with Code**.

**Then** follow `MANUAL_TEST.md` together. Afterwards, swap the session logs (`OurLastEcho\Saved\Logs\OnlineSessions`).
# Milestone 6 â€“ Two eras: one skeleton, two skins

Branch: `milestone-6-two-eras`, branched from `main` (Milestone 5 was merged). Nothing was pushed. Built with the editor open, driven through its MCP server.

**In short:** Bat now sees a dry, eroded present-day canyon and Saraa a lush, ancient one. Each game loads only its own era's visuals, on its own machine. Everything anyone can stand on or bump into is shared, so gameplay and online movement are exactly as before.

## How the level is split

| Layer | Level | Loaded on | What's in it |
|---|---|---|---|
| **Shared** | `Lvl_SpiritPath` | every machine | All gameplay actors, spawns, triggers and **all collision**. The 540 canyon rock/floor meshes and PCG rocks are still here and still solid, but **hidden in game**. The canyon floor landscape stays here too, visible. |
| **Present** (Bat) | `Lvl_SpiritPath_Present` | Bat's machine only | Visible copies of all 540 shapes (same transforms, present materials), low sunset sun, dusty fog, desaturated ochre grading, dry cracked riverbed, dead trees and stumps, a fallen cairn and broken walls, a wind ambience hook. **No collision at all.** |
| **Past** (Saraa) | `Lvl_SpiritPath_Past` | Saraa's machine only | The same 540 shapes in past materials, bright mid-morning sun, clear blue sky, light mist, saturated green/blue grading, a shallow river along the riverbed's line, 3,200 grass tufts, living trees where the dead ones stand, an intact ovoo cairn (stones, pole, blue khadag scarves) where the ruins are, intact stone walls, and water, birds and distant drums hooks. **No collision at all.** |

- **Why the landscape floor stays shared:** a landscape can't be copied without its collision, and it's walkable, so it belongs in the shared layer anyway. Its colour comes from `MPC_Era`, a material parameter each machine sets locally (0 = present, 1 = past). So it's ochre for Bat and green for Saraa. The same tint reaches the few other visible shared rock surfaces (the anchor targets' rock mounts).
- **The shapes line up exactly:** the era levels are built by copying every shared shape's transform, and the eras test compares all 540 on each machine.

## How each machine loads its era

**Choice:** `Lvl_SpiritPath` doesn't use World Partition, so each era is an ordinary level loaded as a **level instance** with `ULevelStreamingDynamic::LoadLevelInstance`, on that machine only.
- Ordinary streaming sublevels were the other option. But when the server streams a sublevel in, Unreal tells every client to load it too, which is exactly what must not happen here.
- A level instance is purely local. The server never hears about it, and the other player's machine never loads it. (I checked the engine code: the client does tell the server its level is visible, and the server only checks the name is well formed.)

`AEchoEraDirector`, placed once in the shared level and never replicated, runs on every machine:
- **Role to era:** it waits for the local player's character (Bat means present, Saraa means past; this also works with Saraa as host), then loads that era.
- **The fade:** the screen stays black from the first frame until the era is loaded and visible, then fades in over 0.6 s. Nobody sees the bare shared layer.
- **Collision safety net:** as an era level loads, it switches collision off on everything in it, even Unreal's editor builder brush. An era layer can't block one player alone, even if someone later drops a solid prop into one.
- **Respawning:** the character's role doesn't change, so the era stays loaded.
- **Joining from the lobby / returning to the title:** the era loads after travel and goes away with the world when leaving (both checked in the online test).
- **During a pause:** it keeps working while the game is paused.

## Other things that changed with the eras

- **Each player sees the other as a ghost:** Saraa already was one on both screens. Now, on Saraa's machine, Bat shows as a warm amber ghost from the present (`MI_GhostFromPresent`).
- **Bat's arrows:** his normal golden arrow in the present, a spirit-blue arrow and trail in the past (`MI_ArrowPast`, `MI_ArrowTrailPast`).
- **Saraa's whip line:** her bright spirit line in the past, a faint pale echo in the present (`MI_WhipLinePresent`).
- **Spirit and echo platforms:** they keep their own per-player rules, unchanged.

## Debug console commands

(Type them in the game window's console. Each affects only the machine you type it on.)
- `EchoSwapEra`: preview the other era on this machine. Type it again to swap back.
- `EchoEra present` / `EchoEra past` / `EchoEra auto`: pick an era, or go back to your own.
- `EchoShowBothEras`: load both eras at once, to check that they line up. Type it again to turn it off. With both loaded, two suns, two fogs and two gradings overlap, so colours look washed out and Unreal warns about two directional lights. It's a debug view.

## Stretch goal: the sapling (done)

- **In the past:** a mark glows on the ground in front of the overlook ledge. When Saraa steps on it she plants a sapling.
- **In the present,** a great tree stands on that spot: 10 branch steps (90 cm apart) spiral up to the ledge's height, with the top one pointing at the ledge. It gives Bat a second way up to the overlook.
- **Networking:** the tree's collision is in the shared layer, turned on for everyone at once through a replicated flag. It blocks only Bat; Saraa walks through it.
- **Tested with real jumps:** Bat climbs all 10 branches and steps onto the ledge.
- *Simplest option:* planting happens by stepping on the mark, with no extra button.

## Build order

`build_canyon.py` â†’ `build_climb.py` â†’ `build_crossing.py` â†’ `build_canyon_rocks.py` â†’ **`build_eras.py` last**:
- **Why last:** the earlier builders respawn visible shared geometry, and `build_canyon.py` respawns a shared post-process volume. `build_eras.py` then copies the shapes into the eras, hides them, and moves the lighting out.
- **Rebuilds:** it rebuilds both era levels from scratch and is safe to re-run. **Re-run it after changing the canyon or any rock piece**, or the eras will draw the old shapes (the eras test catches this).

## How to test

**Just play (editor):** Play as usual (2 players, Listen Server).
- **Bat's window** shows the dusty present, **Saraa's window** the green past.
- **The sapling:** walk Saraa onto the glowing mark in front of the overlook ledge, near the end of the Spirit Path. A tree appears in Bat's window; climb it.
- **Previewing without a second player:** type `EchoSwapEra` in the console.

**Automated (editor open):**
- `Scripts/tools/run_gameplay_test.ps1 -Script test_eras_live.py -Marker ECHO_ERAS_TEST`: 38 checks, about 20 s.
- **With Saraa as host:** type `Echo.HostPlaysSaraa 1` before starting PIE, then run it with `py ECHO_ERAS_MODE="host_saraa"` first (35 checks: the climb needs Bat on the host). Set the variable back to 0 afterwards.
- `Scripts/measure_eras.py`: frame rate with each era, and with both.

**Editor closed:** `powershell -File Scripts/run_lan_test.ps1` runs two real game processes; each must load only its own era.

## Results

**All suites pass:**

| Suite | Checks |
|---|---|
| Eras (Bat hosting) | 38/38 |
| Eras (Saraa hosting) | 35/35 |
| Online front end (with era checks) | 36/36 |
| Two real game processes over LAN | host PASS, guest PASS |
| The Crossing with network emulation | 67/67 at 108 ms ping, 30 ms jitter, 5% loss; 0 corrections |
| The Crossing, Saraa hosting, network emulation | 38/38 at 141 ms ping; 0 corrections |
| Milestone 1 | 29/29 |
| Milestone 2 | 34/34 |
| Milestone 3 | 60/60 |
| The Crossing | 67/67 |
| Title / settings | 52/52 |
| Headless end-to-end | PASS |

**Performance** (2-player PIE, editor in the background with throttling off):

| View | Frame rate |
|---|---|
| Normal (each machine shows its own era) | 46 fps |
| Both views on the present | 45.8 fps |
| Both views on the past | 46.0 fps |
| One machine loading both eras | 44.6 fps (about 0.7 ms per frame for an extra era) |

- **Memory:** an extra era layer added only about 3â€“10 MB, because the eras are instanced basic shapes.
- **What that means:** the eras cost about the same, and only one is ever loaded per machine. Real art will change these numbers; measure again once it arrives.

Details are in TEST_REPORT.md.

## Known issues

- **Needs a manual check: the look and the fade.** Colours and props are greybox placeholders. The fade-in is tested only as "ready"; watch it once when joining.
- **Silent ambience:** the ambience hooks (wind; water, birds, drums) have no sounds yet.
- **River pieces show faint seams** where the flat boxes overlap (greybox).
- **`EchoShowBothEras` washes the colours out**, because both eras' lights and gradings overlap (see *Debug console commands*).
- **The landscape floor isn't duplicated per era**, only tinted (see *How the level is split*). If the eras ever need differently *shaped* ground, that ground has to be shared collision plus per-era visuals, like the rock is now.
- **A Lumen warning** ("surface cache atlas oversubscribed") appeared in 2-player PIE, mostly with both eras loaded. The editor halves Lumen's surface cache for 2-player PIE on this 6 GB GPU (see CLAUDE.md). Not checked in a single full-quality game window.
- **Rebuild order:** `build_eras.py` must run after any canyon or rock builder (see *Build order*).

## Suggestions for Milestone 7

- **Era art:** replace the greybox shapes in each era level with real meshes and materials (eroded vs intact strata, a real river with a flow material, foliage). The structure is ready: art goes in the era levels, collision stays shared.
- **Era-specific geometry:** decide how to handle places where the eras should differ in shape (a collapsed arch in the present). The shared-collision rule means a player-specific obstacle must be a realm-filtered collider, like the spirit platforms.
- **More cross-time interactions**, built like the sapling: Saraa moves a stone and a path opens in the present, or Bat clears rubble and reveals a carving Saraa made.
- **Sound:** fill the ambience hooks and add era-specific footsteps.
- **Transitions:** a proper "echo" effect instead of a plain fade when the era loads or swaps.

---

# Milestone 5 â€“ Title screen, menus, hosting and settings

Branch: `milestone-5-title-screen`, branched from `milestone-4-sword-whip` (Milestone 4 isn't merged into `main` yet, as asked). Nothing was pushed. Built with the editor open, driven through its MCP server.

**In short:** the game now starts on a title screen. From there one player hosts over LAN, the other joins, both ready up in a lobby, and the host starts the Spirit Path. Settings (graphics, audio, controls, accessibility) are saved per machine, and there's an in-game menu that pauses the game for both players.

## What was built

Where the brief offered a choice, I picked the simplest option. Each choice is noted.

**1. The title scene** (`/Game/Echo/Maps/TitleScreen`, built by `Scripts/build_title_screen.py`):
- **The game's default map** (`GameDefaultMap`); the editor still opens `Lvl_SpiritPath`.
- **Scene:** a greybox corner of the canyon at sunset.
  - Terraced walls and boulders, with the canyon running away towards a low, warm sun.
  - Soft warm height fog and a light vignette.
- **Bat** (`AEchoTitleBat`): he sits on a rock beside a placeholder tree and bush, three-quarters turned away from the camera.
  - *Simplest option:* the seated pose isn't a Control Rig or Sequencer animation. It's a pose set directly on his bones (`SeatedPose`, a list of bone rotations you can edit in the Details panel).
  - **Picking-leaves hook:** set `LoopAnimation` to an animation and he plays it on a loop instead of the pose.
- **Camera** (`AEchoTitleCamera`, a Cine Camera): a slow drift and sway. Tuning: `DriftAmplitude`, `DriftPeriod`, `SwayDegrees`.
- **Leaves:** Niagara (`NS_TitleLeaves`, from the engine's Blowing Particles template), drawn as placeholder leaf-coloured quads (`M_Leaf`).
- **Music and wind hooks:** two Ambient Sound actors, `TitleMusic` (Music sound class) and `WindAmbience` (Effects sound class). They're silent until you drop a sound onto each.

**2. The main menu:**
- **Buttons:** Play, Settings, Credits and Quit, under the placeholder title "Our Last Echo".
- **Built with CommonUI:** every screen is a C++ `CommonActivatableWidget`, and screens fade in and out (0.25 s).
- **Mouse, keyboard and gamepad** all work:
  - Arrows / d-pad move the focus, and Enter / Space / gamepad A press the focused button.
  - Esc / P / gamepad B go back.
  - The focused (or hovered) button lights up amber.
- *Simplest option:* the screens build their own layout in C++ instead of UMG Blueprints. Nothing binary had to be hand-edited, and CommonUI's focus and back handling works the same. The catch: you can't rearrange them in the UMG designer yet (see *Suggestions*).

**3. Play: hosting and joining:**
- **Sessions:** `UEchoSessionSubsystem` wraps the online subsystem's sessions. It uses **Online Subsystem Null** (LAN) today (`DefaultPlatformService=Null` in `DefaultEngine.ini`).
  - The menus only ever talk to this subsystem, never to the online subsystem directly.
  - **Switching to EOS later:** enable the EOS plugin, set the platform service, and adjust the subsystem's LAN flag. The menus don't change.
- **Host Game:** creates the session and reopens the title map as a listen server. That's the **lobby**.
- **Join Game:** lists games found on the network (host name and ping), with Refresh and Back.
- **Lobby:**
  - Shows both players with their roles: host = Bat, second player = Saraa.
  - Each player has a Ready toggle.
  - **Start Game** appears only for the host, and is enabled once both players are ready.
  - Start takes both players to `Lvl_SpiritPath` (a normal server travel; the guest reconnects on the new map).
- **Messages, each with a way back:**

  | Situation | What the player sees |
  |---|---|
  | Guest joins or leaves the lobby | A toast on the host's screen |
  | Game is full | The host turns the third player away, and they see a "couldn't join / connect" message (not tested: PIE was set up for two players) |
  | Game has closed | "Couldn't connect to that game. It may have closed." |
  | Nobody is hosting | The Join list says there are no games |
  | Host leaves (lobby or game) | The guest returns to the title with "The host left the game." |
  | Guest leaves the game | The host returns to the title with "Saraa left the game." |

  - The closed-game case gives up after 15 s; the engine alone would wait minutes.

**4. Settings** (four tabs; switch with the tab buttons, LB/RB or Q/E):
- **Graphics:**
  - Window mode, resolution (the monitor's supported list), quality preset (Lowâ€“Epic).
  - V-sync and frame rate cap (30/60/120/144/unlimited).
  - Brightness (applied as display gamma).
- **Audio:** master, music, effects, dialogue and voice chat volumes, plus a microphone toggle.
  - The sliders drive **real sound classes**: `SC_Master` is the parent of `SC_Music`, `SC_SFX`, `SC_Dialogue` and `SC_Voice`.
  - The volumes are applied through the `SMix_Settings` sound mix, so a change is heard immediately.
  - Sounds with no class use Effects; voice chat uses Voice.
- **Controls:**
  - Mouse, aim and gamepad sensitivity, and invert Y.
  - Aim mode: hold or toggle.
  - **Key rebinding** for keyboard and mouse through Enhanced Input's user settings: move forward/back/left/right, jump, aim, fire, whip and menu. Select a row, press the new key; Esc cancels.
- **Accessibility:**
  - Subtitles on/off and subtitle size (Small / Medium / Large / Extra large).
  - Reduce camera shake: the bow's shake plays at 20%.
- **Buttons:**
  - **Apply** saves.
  - **Reset to Defaults** asks first, then resets settings and key bindings.
  - **Back** with unsaved changes asks *Apply / Discard / Cancel*.
- **When changes take effect:** straight away where possible, so you hear or see them before applying. Discard puts them back. Resolution and window mode change on Apply (only outside the editor).
- **Saving:** settings go to `GameUserSettings.ini`, and key bindings through Enhanced Input's own save. Both load on the next launch. They're **per machine**, which also settles the Milestone 3 to-do: each player has their own volume.

**5. Credits and quitting:** a slowly scrolling credits screen (placeholder names). Quit asks "Quit Our Last Echo?".

**6. In-game menu:**
- **Opening it:** Esc / P / gamepad Start opens Resume, Settings and Leave Game.
- **It pauses for both players** (changed after testing; the brief asked for no pause, but you preferred the Milestone 4 behaviour).
  - The game stays paused until everyone who opened the menu has closed it.
  - The other player's screen dims and says who paused ("Paused - Bat paused the game"). They can open their own menu too.
  - If a player leaves while the game is paused, the pause ends with them.
- **Settings** opens the same settings screen as the title menu.
- **Leave Game** asks first, then returns both players to the title screen. The other player is told why (see the table above).
- **Replaced:** the Milestone 4 settings/pause menu widget and its scripts were removed. This menu replaces it and keeps its pause-for-both behaviour.

**Changes after your playtest** (Milestone 4 mechanics):
- **The whip holds straight away:**
  - Its length is exactly Saraa's distance to the anchor when she latches (the old 3 m minimum left it slack), and it shortens if she keeps moving in.
  - The latch push goes along the swing, not at the anchor.
  - The whip line reaches the anchor instantly.
  - Server corrections carry the whip's length, so online prediction stays exact (still 0 corrections).
- **Anchor targets:**
  - They hang upright just under the overhangs and the arch, facing across the canyon at Bat's shelf. Target 1 keeps your hand placement.
  - Each board sits on a rock mount that reaches up into the rock (`bMount`, `MountReachUp`, `MountDepth` on each target), so boards don't float or clip.
  - `build_crossing.py` places them this way.
- **Pillar 2:** one arch anchor can now just reach it. You accepted that for this greybox level, so the test logs it instead of failing.
- **The in-game menu pauses both players again** (see 6 above).

**Gameplay hooks for the new settings:** the character's look input uses the sensitivities and invert Y. The bow uses hold/toggle aim and plays a (reducible) camera shake when firing. The HUD can show subtitles at the chosen size (test with the console command `EchoSubtitle Hello`).

## How to test

**Just play (editor):** PIE settings decide which setup you get. `Scripts/tools/pie_mode.ps1` switches between them:
- `pie_mode.ps1 -Mode frontend -Players 2`: TitleScreen, 2 players in Standalone mode. Each window is its own game, like two PCs on a LAN. Host in one window, join from the other.
- `pie_mode.ps1 -Mode gameplay`: back to the project default, Lvl_SpiritPath with 2 players as listen server, for the gameplay levels.
- In the editor by hand: open TitleScreen, set *Net Mode* to *Play Standalone* and *Number of Players* to 2 in the Play options.

**Two real copies of the game (editor closed):** `powershell -File Scripts/run_lan_test.ps1`. It opens two small game windows. One hosts, the other joins through the menus, both ready up, and the host starts. Both must end up in the Spirit Path as Bat and Saraa (about 2 minutes). To play it yourself, launch the game twice with `UnrealEditor.exe "<project>.uproject" -game -windowed`.

**Automated (editor open):**
- `Scripts/tools/run_frontend_test.ps1 -Mode title` (1 player, 52 checks, ~20 s): the scene, the menus by keyboard, gamepad and mouse, Credits, Quit, and every settings tab, including Apply / Discard / Reset, saving and reloading.
- `Scripts/tools/run_frontend_test.ps1 -Mode online` (2 players, 32 checks, ~100 s): host, join, lobby, ready, start, the in-game menu not pausing, each player leaving, a closed game, and an empty list.
- `Scripts/tools/run_gameplay_test.ps1 -Script test_climb_live.py -Marker ECHO_CLIMB_TEST` (and the same for the other Milestone 1â€“4 tests) runs a gameplay suite in a fresh session.

**Rebuilding the title map:** `build_title_screen.py`, in the open editor. It opens TitleScreen, rebuilds it from scratch (so hand edits to it are lost), then reopens Lvl_SpiritPath. It also (re)creates the audio classes, the CommonUI input data and the rebindable key names.

## Results

**All suites pass:**

| Suite | Checks |
|---|---|
| Title scene, menus and settings | 52/52 |
| Hosting, joining, lobby and in-game menu | 34/34 |
| Two real game processes over LAN | host PASS, guest PASS |
| Milestone 1 | 29/29 |
| Milestone 2 | 34/34 |
| Milestone 3 | 60/60 |
| The Crossing (Milestone 4) | 67/67 (the pillar 2 check is now a logged note) |
| Headless end-to-end (`run_spirit_path_test.ps1`) | PASS |

Details, and the bugs the tests caught, are in TEST_REPORT.md.

## Known issues

- **Needs a manual check: feel and looks.**
  - The tests press keys, gamepad buttons and mouse clicks through the same input path a player's devices use, but nobody has pressed a real controller yet.
  - Worth checking by eye: the focus highlight, the fades, and the title shot.
  - Colours are placeholders.
- **Needs a manual check: resolution and window mode.** These only apply outside the editor (in PIE they'd resize the editor). The two-process LAN test ran real games but didn't change them.
- **Silent title:** the music and wind hooks have no sounds yet, and the credits are placeholders.
- **Voice chat doesn't exist yet.** Its volume slider and the microphone toggle are saved and drive the Voice sound class, but there's nothing to hear or mute.
- **Gamepad buttons can't be rebound**, only keyboard and mouse. The gamepad layout is fixed.
- **The menu layouts are built in C++**, so changing spacing or order means editing code, not the UMG designer.
- **Starting the game is a hard map change:** both players see a short load, and the guest reconnects. Seamless travel (with a loading screen) is a Milestone 6 suggestion.
- **Two copies of the game on this PC** (6 GB GPU) crawl at full quality; the LAN test runs them at Low. One copy per PC is fine.
- **The Message Log window pops up at every PIE start,** because of an engine plugin's asset-manager warning (`GameFeatureData`). It's harmless; `Scripts/tools/close_message_log.ps1` closes it.
- **EOS isn't tested.** It needs an Epic developer account and product IDs.

## Suggestions for Milestone 6

- **Online for real:** switch to EOS (friend invites and joining across the internet), and use seamless travel with a loading screen so the guest never disconnects between maps.
- **Menus in UMG:** make each screen a Blueprint subclass with `BindWidget` parts, so the layout, fonts and art can be done in the designer. Add controller button icons (CommonUI input data can show the right glyph per device).
- **Title scene art:** the picking-leaves animation for Bat, a real tree, and the title music and wind.
- **Progress:** save which section the pair reached and add a Continue option.
- **Voice chat,** now that the settings are ready for it, and gamepad rebinding.
- **Story subtitles:** hook the subtitle system to the first lines of dialogue.

---

# Milestone 4 â€“ The sword whip and anchor arrows

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
- **Targeting:** every frame, on Saraa's own machine. The nearest anchor within **whip range (18 m)**, within **40Â°** of the camera's view and in clear line of sight gets highlighted: the orb swells and brightens, and blue brackets appear around it on her screen.
- **Latch:** hold the whip button: **left mouse / E / gamepad right trigger** (`IA_Whip` in `IMC_Whip`, from `Scripts/build_sword_whip.py`). The whip line lashes out to the anchor in 0.12 s, with a snap sound and a flash on both machines.
- **Swing:** a pendulum under the anchor.
  - Physics: gravity (Ã—1.4) with the rope as a maximum length. A slack rope is free fall, and when it goes taut she keeps only her speed along the swing.
  - Steering: a little air control from the move stick. She faces the way she swings.
  - Momentum is kept, capped at 16 m/s.
  - Walkable ground: touching it ends the swing. Walls: she scrapes along them.
- **Release:** let go of the whip button, **or press jump**. She launches with her momentum plus a small forward and upward boost. Her speed isn't braked in the air until she lands (the template's 15 m/sÂ² falling braking otherwise killed the launch).
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
| **Chasm** | 71 m long, wall to wall, 14 m deep (stations 7850â€“15000). It's cut into the landscape by `build_canyon.py`, with a kill volume inside. |
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
- **BP_Saraa â†’ SwordWhip:** whip range, targeting angle, re-latch delay, max swing speed ("swing speed"), latch boost, gravity during swing, air control, min rope length, ground-latch hop, release boost (forward and up), lash range/radius/duration/cooldown, look and sounds.
- **BP_ThirdPersonCharacter â†’ SpiritBow:** `MaxActiveAnchors` and `AnchorClass`.

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
- **Frame rate:** before running, turn off *Use Less CPU when in Background* (Editor Preferences â†’ Performance, or the MCP `ObjectTools.set_properties` call in CLAUDE.md). It resets on every editor start. Otherwise a background editor runs at ~3 fps with two PIE worlds, and scripted input that coarse isn't meaningful.

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
1. **Swings drifted sideways off the pillars** in the first runs. The test steered along the canyon's centreline, but the left wall bends 12Â° away from it there. It now steers along Saraa's line, like a player.
2. **The arch's front rock curtain blocked Saraa's own view** of the first arch anchor from pillar 1, so her whip had nothing to latch onto. It was removed; the back curtain stays.
3. **Target 3 sat right where the first arch swing tops out,** so the second swing had no arc. The arch was lengthened, target 3 moved 6 m on, pillar 2 moved back, and target 4 moved out.
4. **Momentum was lost on release.** The template's falling braking cut her to walking speed in a fraction of a second. Flights from a swing are no longer braked.
5. **Too much energy:** pumping with the stick (air control 700) plus a 26 m/s cap let one arch anchor fling her past the chained swing. Retuned to air control 300, cap 16 m/s, boosts 250, with the arch curtain as a physical block. The test proves no single-anchor release reaches pillar 2 (best: 1.9 m short).
6. **Bat stuck in his doorway:** the far rim's slope is ~70 cm higher than the shelf there. The shelf and sill were raised to that ground, with a ramp at the start.
7. **Engine crash (not ours):** one editor crash in the engine's derived-data cache HTTP code while PIE started. Restarting fixed it.

## Known issues

- **Needs a manual check: real controls and feel.**
  - Whip input is verified: IA_Whip is on LMB, E and the right trigger, and the component binds it. Every path is driven through the same functions the bindings call.
  - Nobody has pressed a real button yet. Worth feeling: the 40Â° targeting cone (anchors are high above, so she may need to look up; widen `TargetingAngle` if it's fiddly), swing speed, and release timing.
- **Needs a manual check: the lash from Saraa's client.** The lash is a server RPC from her machine, which editor Python can't send (same reason as Milestone 3's debug command). The test drives it on the server's copy of her, which covers the hit, replication and cooldown. Pressing the button in Saraa's window once confirms the RPC.
- **How Saraa's swing looks on Bat's screen** hasn't been measured. It uses the engine's standard smoothing for other players' characters, which extrapolates in straight lines between updates. At high ping her arc may look slightly less round to Bat than to her.
- **A perfect fling might skip the echo platform.** Her doorway's sill is 90 cm above the platform, so she'd have to fly into a 3 Ã— 2.6 m window. Not seen in testing, but not impossible.
- **The whip line can pass through the arch's back curtain** for a moment during the second arch swing. It's only a visual; nothing collides with the line.
- **Arch targets can also be shot from the start area** (only target 4 and the echo platform need the shelf). A front curtain would fix it, but it blocked Saraa (see *Results*).
- **Colours:** the whip line was toned down (glow 1.4 â†’ 0.8) after a screenshot showed it near-white in the canyon's auto-exposure. It still reads very pale. Tune `MI_WhipLine` / `MI_SpiritAnchor` by eye.
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
# Milestone 3 â€“ The spirit bow and echo platforms

Branch: `milestone-3-spirit-bow`, branched from `main` after Milestone 2 was merged. Built with the editor open, driving it through its MCP server.

**New mechanic:** Bat sees faint gold "echo platforms" that Saraa can't. When he shoots one with his spirit bow, it becomes solid and blue for her, so she can climb it.

## What was built

**The spirit bow** (`UEchoSpiritBowComponent`; every character has one, but only a Living-realm character can use it):
- **Aim:** hold **right mouse / left trigger**. The camera pulls in over the shoulder (400 â†’ 160 cm, FOV 70), a gold reticle appears, Bat faces where he aims, and he slows to 2.2 m/s.
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
1. **Hops came out 5â€“6 m apart.** Platforms were spaced along the canyon centreline, and the curving wall stretched them. They're now placed by measured distance.
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
# Milestone 2 â€“ Testing the Spirit Path, and building the canyon

Branch: `milestone-2-canyon`. Built and tested live through the Unreal MCP server, with the editor open.

## Phase 1: Milestone 1 test

See **`TEST_REPORT.md`**. Every Milestone 1 item passed in a real 2-player listen-server PIE session (29 checks, including replication to the client), and nothing needed fixing. After the canyon was built, the same test was run again and still passes.

## What was built

**Layout.** Coordinates are in cm. The Spirit Path runs along +X at Y = 0, from X = âˆ’1500 to 3600.

| | |
|---|---|
| Length | About 350 m. The canyon is closed by rock end caps **150 m before the start** (X = âˆ’16 500) and **150 m past the far side** (X = 18 600). |
| Floor width | 37â€“52 m. Each wall is placed from control points, so the width varies. |
| Wall height | 40â€“80 m, varying per side, plus Â±5 m per segment. |
| Bends | The canyon is straight around the play area and bends away at both ends, 55 m sideways toward âˆ’Y at the start and 65 m toward +Y at the far end, so you can't see out of either end. |
| Ravine | Bat's pit now cuts **right across the canyon**, wall to wall, between X = 0 and 1600, so nobody can walk around the Spirit Path puzzle. Its floor is inside the Milestone 1 respawn volume. |
| Overlook | A raised rock shelf 9 m up on the +Y wall, just past the end zone (X 30â€“44 m), reached by a 26 m ramp from the far side. It's the future vista point. |

**How it's built** (all in `Lvl_SpiritPath`; Milestone 1 actors weren't moved or resized):
- **Landscape `CanyonFloor`:** 378 Ã— 252 m, 1 m resolution, 24 components. It has a gentle rolling floor, a talus apron rising into each wall and rockfall slopes into the end caps. It's flattened to just under the Milestone 1 slabs. Python has no API for creating landscapes, so there's a small editor-only C++ helper, `UEchoEditorLibrary::CreateLandscapeFromHeights`, that does what Landscape mode's *New Landscape* does.
- **Walls:** 251 engine cubes (`/Engine/BasicShapes/Cube`) stacked in 4â€“6 strata per 14 m segment. Each stratum is set back 1â€“6 m from the one below (terraces) and slightly rotated, so the walls read as layered sandstone rather than flat walls. On the outside of each bend, pieces are sized from the actual face line so they always overlap (see *Bugs found* below).
- **Rocks (PCG):** `/Game/Echo/PCG/PCG_CanyonRocks` was built node by node through the MCP PCG toolset. It samples the landscape twice: dense large rocks on the talus at the wall bases, and sparse boulders on the open floor. It removes points inside `EchoRockExclusion` boxes (the Spirit Path and the overlook ramp), prunes overlaps and spawns about **440 instances** in 3 ISMs (LevelPrototyping ChamferCube, engine Sphere and Cone). The volume is set to **generate on demand only**: the result is saved in the level and never regenerates at runtime, so both players get identical rocks. The rocks have `BlockAll` collision (solid for both realms).
- **Materials:** `M_CanyonRock` is texture-free. Horizontal sedimentary bands come from world-space Z (wobbled by X/Y), with occasional thin dark bands and gradient-noise brightness variation. It works on scaled cubes and on the landscape without UVs. There are three instances: `MI_CanyonRock` (walls), `MI_CanyonGround` (landscape, sandier) and `MI_CanyonBoulder` (rocks, tighter bands).
- **Boundaries:** `AEchoBoundaryVolume` (new C++) is an invisible box that blocks **only** `Pawn` and `SpiritPawn`, so cameras and traces pass through. There's a chain of them along each wall face, from below the ravine to 140 m up, plus one across each end. Their outlines only draw when selected.
- **Kill volume:** a second `EchoRespawnVolume` (`CanyonKillVolume`), 400 Ã— 260 m, under the whole canyon floor. The original Milestone 1 one only covers the ravine area. Anyone who falls through respawns at their start point.
- **Lighting:** the Milestone 1 lights were retuned. The Sun is low (19Â°) and warm, from down-canyon, and rakes across the +Y wall. The atmosphere has more dust (Mie) and less blue (Rayleigh). The height fog is warmer and dustier. An unbound `CanyonPostProcess` adds slight desaturation, warm gain and a vignette.

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
1. **A gap in the wall boundary on the outside of the far bend.** At rim height a player could have gone straight out at X â‰ˆ 77 m. Wall pieces were spaced by centreline distance, but the face line on the outside of a curve is longer. They're now sized from the actual face-line chord (`face_chord` in `build_canyon.py`).
2. **The PCG rocks had no collision.** The PCG spawner defaults to `NoCollision`, so players walked through them. They now use `BlockAll`.
3. **2-player PIE ran out of video memory** with the canyon (the on-screen message was "Video memory has been exhausted", about 120 MB over budget on this 6 GB RTX 2060). Fixed with the editor-only Lumen setting above; see *Performance*.

## Performance

- **What the canyon adds:** 1 landscape (24 components), 251 cube actors, about 440 rock instances in 3 ISMs, 56 invisible boundary boxes, 1 post-process volume. That's modest for UE5.
- **Frame rate: couldn't be measured meaningfully from here.** The editor was always a background window while I drove it, and on this machine a background editor runs at about 7â€“8 fps even with the level empty of PIE and "Use Less CPU when in Background" turned off. Milestone 1 measured the same 8 fps before the canyon existed. So those numbers measure Windows/editor throttling, not the level. **Please check it yourself:** click into the PIE window and type `stat fps` (or `stat unit`) in the console. `Scripts/measure_fps.py` also works if the editor is focused.
- **GPU profile** (`ProfileGPU`, 2-player PIE, 2 views): no single canyon-related pass dominates. Shadow depths are about 1.5 ms and there are about 350k primitives per view. The largest costs are character skinning, hardware-ray-traced skinned-mesh updates and Lumen's radiance cache, all of which Milestone 1 already had. Absolute timings were inflated because the GPU was downclocked in the background.
- **Video memory:** 2-player PIE renders two separate worlds on one GPU, so every per-scene buffer (Lumen surface cache, virtual shadow maps, distance fields) exists twice. Halving the Lumen surface cache atlas in the editor only freed about 500 MB and removed the warning, with no visible difference. A packaged game renders one world per machine and keeps the default.

## Known issues

- **The frame rate hasn't been measured with the editor focused.** See above; this needs a quick manual check.
- **Re-running `build_spirit_path.py` (Milestone 1)** respawns the Sun, sky and fog with their old settings. Run `build_canyon.py` afterwards to re-apply the canyon lighting.
- **The editor viewport looks brighter and more washed out than PIE,** because of auto-exposure and the editor view. Judge the look in PIE.
- **The boundary sits up to about 0.6 m off the ideal face line,** and players are stopped 1â€“2 m from the rock on some stretches. It works, but the invisible wall might be noticeable against the rock. A future pass could use per-segment walls that follow the rocks more tightly.
- **The landscape has no paint layers:** it uses one material and one colour variation. Hand-sculpting works, but `build_canyon.py` would overwrite it.
- **After regenerating rocks,** the editor viewport briefly showed faint "ghost" circles where old rocks were (stale lighting cache). They weren't visible in PIE.
- **Closing the editor through MCP hangs:** `QUIT_EDITOR` typed via the MCP server leaves the editor stuck in "Preparing to exit", because the MCP call never returns. Everything was saved, so ending the process was safe. It's better to close the editor by hand.
- **Startup shows an "Asset Manager â€¦ GameFeatureData" load error.** It comes from the MCP `AllToolsets` plugin and is harmless.

## Suggestions for Milestone 3

- **Past and present canyons:** generalise the spirit platform's local-visibility trick into a reusable `UEchoRealmComponent` (visible/solid per realm) and put it on canyon pieces. For example, a rockfall that blocks the present but not the past, or an ancient bridge only Saraa has. `build_canyon.py` could tag realm variants.
- **Checkpoints:** an `AEchoCheckpoint` volume that updates each character's respawn transform, so both kill volumes send players to the last checkpoint instead of the start.
- **The vista moment:** use the overlook for a short camera or sound beat, with both players seeing their own era from the same spot.
- **Better rocks:** enable GeometryScripting to generate irregular rock meshes (or build a small modular kit), and add landscape paint layers (sand, gravel, scree) with PCG scrub and small debris.
- **More realistic multiplayer perf testing:** PIE with *Run Under One Process* off (a separate client process), plus a scalability preset for lower-end GPUs (hardware RT off, Lumen at lower quality).
- **The next area:** turn one end cap into a narrow passage or collapsed slot canyon leading to the next level section. The builder already supports moving the ends.

---
# Milestone 1 â€“ "The Spirit Path" greybox

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
| Compiles with no errors | âœ… Clean build, no warnings |
| PIE, 2 players, Listen Server | âš ï¸ Set as the default. A real listen server + client connected and got the right characters headless, but it hasn't been played in the editor yet |
| Saraa crosses on spirit platforms; Bat falls through | âœ… Verified in a game world by the automated test |
| Bat can't see the platforms; Saraa can | âš ï¸ Logic is in place; needs a visual check in PIE (headless runs don't render) |
| Switch raises the bridge; Bat sees it rise | âœ… Raise verified by the test on the server; âš ï¸ the client-side animation needs a visual check |
| Both in end zone â†’ "Milestone complete" | âœ… Game state verified by the test; âš ï¸ the on-screen text needs a visual check |

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
