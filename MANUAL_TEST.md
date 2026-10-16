# Our Last Echo: testing online with a friend

A checklist for two people on **different computers and different internet connections** (for example, you at
home and a friend at their place, or one PC on a phone hotspot). Tick each box; write down anything odd with the
time it happened. Takes about 45 minutes.

**You:** the host (Bat). **Friend:** the guest (Saraa). Swap roles at the end (section 9).

## 0. Before you start

- [ ] Both have the same build (same zip from `Scripts/package_game.ps1`).
- [ ] Both have an Epic Games account, and you're **friends on Epic** (Epic Games Launcher > Friends).
- [ ] While the game is in testing, your friend's Epic account must be a member of your Developer Portal
      organization (Organization > Team members), or Epic refuses their sign-in.
- [ ] Both have a microphone and headphones (headphones stop your voice echoing back through their mic).
- [ ] Talk on your phone or Discord for the first few minutes, in case voice chat doesn't connect.
- [ ] Windows Smart App Control is off on both PCs, or the game won't start (Windows Security > App & browser control).

## 1. Start and sign in (both)

- [ ] Unzip the whole folder, run `Play Our Last Echo.exe`. If Windows warns: More info > Run anyway.
- [ ] The first time, an Epic sign-in window opens (in the game or a browser). Sign in.
  - Look for: "Signing in with your Epic account...", then the main menu with **"Signed in as <your name>  (Epic account)"**.
- [ ] Close the game and start it again: it signs in by itself this time (no window).
- [ ] Optional: start the game with the internet unplugged. Look for a clear message and **Try Again**; plug it
      back in, press Try Again, and it signs in.

## 2. Host and join by invite

- [ ] **You:** Play > Host Game. The lobby opens and shows **"Join code:  XXXXXX"** in amber.
- [ ] **You:** press **Invite Friend**. The Epic overlay opens with your friends list; invite your friend.
  - If the overlay doesn't appear: press **Invite from Friends List** instead and pick your friend.
- [ ] **Friend:** the invite appears (Epic overlay notification, or a "<name> invited you" box on the main menu).
      Accept it. You arrive in the lobby straight away.
- [ ] **You:** a toast says "Saraa joined the lobby." Both see both names and "Not ready".
- [ ] **Invite while closed:** Friend closes the game. You invite again. Friend starts the game: after signing in,
      the invite shows in the Epic overlay (Shift+F3); accepting it joins you. (A test build can't be *launched*
      by an invite while closed; that needs the Epic Games Store.)

## 3. Join by code (backup)

- [ ] **Friend:** Leave Lobby. On the main menu: Play > Join Game, type the code (any case, spaces OK), Join with Code.
- [ ] Wrong code (e.g. AAAAAA): a message says no game has that code; you stay on the Join screen.
- [ ] While "Joining..." shows, **Cancel** brings you back to the Join screen.

## 4. Voice chat (in the lobby, then keep checking in the game)

- [ ] Talk normally (open mic is the default). Each hears the other clearly.
- [ ] While the other person talks, a blue box "((( <name> is speaking" appears bottom-left.
- [ ] Settings > Audio > **Voice chat volume** down and up: their voice gets quieter and louder.
- [ ] Settings > Audio > **Microphone** off (Apply): they can't hear you. On again: they can.
- [ ] Settings > Audio > **Voice mode: Push-to-talk** (Apply). Silent until you hold **V** (gamepad: click the left
      stick); while held, a red "Mic on" box shows and they hear you. Change the key in the same tab and try it.
- [ ] Listen for: echo, robot voice, cutting out, a delay of more than about half a second.

## 5. Play the whole level (both Ready, you press Start Game)

- [ ] Both see the screen fade in from black within a few seconds (never stuck black).
- [ ] **You (Bat)** see the sunset present; **Friend (Saraa)** sees the morning past.
- [ ] Turn on Settings > Graphics > **Network stats overlay** (or type `EchoNetStats` in the console). Note the
      ping (top right) at the start: ______ ms. It should say **EOS P2P**.
- [ ] **Spirit Path:** Saraa crosses the blue spirit platforms Bat can't see; the switch raises the bridge for Bat.
- [ ] **The Climb:** Bat shoots the gold echo platforms awake; Saraa climbs them; timed ones go out again.
- [ ] **The Crossing:** Bat shoots anchor arrows into the boards; Saraa whips onto them and swings across the chain.
      Swings feel smooth on Saraa's own screen, and Bat sees her swing without jumps or teleports.
- [ ] **Sapling:** Saraa steps on the glowing mark by the overlook; a tree appears for Bat, and Bat climbs it.
- [ ] **End zone:** both reach it; the game says the milestone is complete for both.
- [ ] Respawn: each falls into the ravine once and comes back at their checkpoint.
- [ ] Pause: either opens the menu (Esc); the game pauses for both and the other sees "<name> paused the game".

Look for, the whole time: the other player sliding or snapping around, actions that happen on one screen but
not the other, arrows or the whip appearing in the wrong place, long freezes.

## 6. Disconnects

- [ ] **Friend leaves** (menu > Leave Game): Friend goes to the title screen. **You** get "Saraa left the game."
      and are back in your lobby with the same join code. Friend joins again with the code.
- [ ] **Host leaves** (you: menu > Leave Game): Friend goes to the title screen with **"The host left the game."**
- [ ] **Connection drops** (Friend unplugs the internet / turns off Wi-Fi mid-game for 30 seconds): within about
      a minute both get a message and a menu, not a frozen or black screen. Plug back in and rejoin.
- [ ] **Full game:** with you two in the lobby, a third person (or a second copy) tries your code: "That game is full."
- [ ] **Old invite:** you invite your friend, then leave your lobby before they accept. Accepting says the invite
      has expired, and they stay on the main menu.

## 7. Settings online

- [ ] Change graphics, audio and controls during a game (pause menu > Settings): they apply, and nothing breaks
      for the other player.

## 8. A long session

- [ ] Play or idle together for 30 minutes. Note anything that gets worse over time (ping, stutter, voice).
- [ ] Ping at the end: ______ ms.

## 9. Swap roles

- [ ] Friend hosts (Bat), you join (Saraa). Repeat sections 2, 4 and 5 quickly.

## Afterwards: send these

- Each PC's newest file in `OurLastEcho\Saved\Logs\OnlineSessions\` (the online session log: sign-in, joins,
  disconnects, a network sample every 10 seconds). Mark moments while playing with the console command
  `EchoLogNote something_happened`.
- The time and a description of anything that went wrong.
