"""
Live Play-In-Editor test for Milestone 5 (title screen, menus, settings, hosting and joining, in-game menu).
Set PIE up first with Scripts/tools/pie_mode.ps1 -Mode frontend -Players <1 or 2>, start a fresh PIE session, then:
  py ECHO_FE_MODE="title"     (1 player: the title scene, menu navigation by keyboard / gamepad / mouse, settings)
  py ECHO_FE_MODE="online"    (2 players: host, join, lobby, ready, start, the in-game menu, leaving)
  py exec(open(r'<project>/Scripts/test_frontend_live.py').read())
Output lines are tagged ECHO_FE_TEST; the last line is PASS or FAIL.

Input goes through Slate (UEchoUITestLibrary: real key presses and mouse clicks), so focus, navigation, CommonUI and
the buttons are exercised the way a player's input would be. Clicks that lead to a client-to-server RPC are queued
to the next engine tick (QueueClickWidget), because editor Python runs RPCs locally.
"""
import os
import re

import unreal

PROJECT = "D:/Creating games in term 4/My Own games/OurLastEcho/OurLastEcho"
MODE = globals().get("ECHO_FE_MODE", "title")

gs = unreal.GameplayStatics
ui = unreal.EchoUITestLibrary
T = {"failures": 0, "checks": 0, "wait": 0.0, "elapsed": 0.0, "handle": None, "gen": None}
S = {}


def log(msg):
    unreal.log(f"ECHO_FE_TEST: {msg}")


def check(ok, text):
    T["checks"] += 1
    T["failures"] += 0 if ok else 1
    log(f"{'ok  ' if ok else 'FAIL'} {text}")
    return ok


def worlds():
    """PIE worlds in player order (they're replaced when a player changes map, so fetch them fresh)"""
    return sorted(unreal.EditorLevelLibrary.get_pie_worlds(False), key=lambda w: w.get_path_name())  # /Game/.../UEDPIE_<instance>_<map>


def world(index):
    ws = worlds()
    return ws[index] if index < len(ws) else None


def pc(index):
    w = world(index)
    return gs.get_player_controller(w, 0) if w else None


def pawn(index):
    w = world(index)
    return gs.get_player_pawn(w, 0) if w else None


def root(index):
    p = pc(index)
    return p.get_ui_root() if p and hasattr(p, "get_ui_root") else None


def top(index):
    r = root(index)
    return r.get_top_screen() if r else None


def top_is(index, cls_name):
    t = top(index)
    return t is not None and t.get_class().get_name() == cls_name


def map_name(index):
    w = world(index)
    return re.sub(r"^UEDPIE_\d+_", "", w.get_name()) if w else ""


def net_mode(index):
    w = world(index)
    if not w:
        return "none"
    return "server" if unreal.SystemLibrary.is_server(w) and not unreal.SystemLibrary.is_standalone(w) else \
        "standalone" if unreal.SystemLibrary.is_standalone(w) else "client"


def wait_for(cond, timeout=5.0, step=0.1):
    t = 0.0
    while t < timeout:
        try:
            if cond():
                # Let the screen's fade-in finish: CommonUI ignores input while screens are transitioning
                yield 0.4
                return True
        except Exception:
            pass
        yield step
        t += step
    try:
        return bool(cond())
    except Exception:
        return False


def settings():
    return unreal.EchoGameUserSettings.get_echo_settings()


def button(index, label):
    t = top(index)
    return t.find_button(label) if t else None


def click(index, label, queued=False):
    """A real mouse click: move onto the button first (a mouse moves before it clicks; CommonUI also ignores the click
    that switches it from keyboard/gamepad to mouse mode), then click"""
    b = button(index, label)
    if not b:
        check(False, f"player {index + 1}: button '{label}' on {top(index).get_class().get_name() if top(index) else 'no screen'}")
        return False
    ui.hover_widget(b)
    yield 0.15
    if queued:
        ui.queue_click_widget(b)
    else:
        ui.click_widget(b)
    return True


def key(name):
    k = unreal.Key()
    k.import_text(name)
    return k


def press(name):
    ui.send_key(key(name))


def focused_label(index, labels):
    t = top(index)
    return next((l for l in labels if t and t.find_button(l) and ui.has_focus(t.find_button(l))), ui.get_focused_widget_description())


def focus_game(index=0):
    """
    The test is started by typing into the editor's console, which leaves keyboard focus in that text box. A player
    clicks into the game window first; this is the same: give focus to the menu's own starting widget
    """
    t = top(index)
    target = t.get_desired_focus_target() if t else None
    if target:
        target.set_focus()


# ------------------------------------------------------------------ title (1 player)

def title_scenario():
    check(map_name(0) == "TitleScreen", f"the game starts on the title screen map ({map_name(0)})")
    mode = gs.get_game_mode(world(0))
    check(mode is not None and mode.get_class().get_name() == "EchoFrontEndGameMode", f"front-end game mode ({mode.get_class().get_name() if mode else None})")

    # The scene
    w = world(0)
    cams = gs.get_all_actors_of_class(w, unreal.EchoTitleCamera)
    check(len(cams) == 1 and pc(0).get_view_target() == cams[0], "the player looks through the title camera")
    start = cams[0].get_drift_offset()
    yield 2.0
    moved = (cams[0].get_drift_offset() - start).length()
    check(moved > 1.0, f"the camera drifts gently ({moved:.1f} cm in 2 s)")

    bat = gs.get_all_actors_of_class(w, unreal.EchoTitleBat)[0]
    pelvis, knee, foot, head = (bat.get_bone_location(b) for b in ("pelvis", "calf_l", "foot_l", "head"))
    fwd = bat.get_actor_forward_vector()
    knee_fwd = (knee - pelvis).dot(fwd)
    check(knee_fwd > 25 and abs(knee.z - pelvis.z) < 20 and knee.z - foot.z > 30 and head.z > pelvis.z + 50,
          f"Bat is seated: knees {knee_fwd:.0f} cm ahead of his hips at hip height, feet below the knees, head up")
    check(not bat.is_playing_loop_animation(), "no loop animation set yet (the picking-leaves hook uses the static pose until one is)")
    fx = gs.get_all_actors_of_class(w, unreal.NiagaraActor)
    check(len(fx) == 1 and fx[0].get_component_by_class(unreal.NiagaraComponent).is_active(), "leaves are drifting (Niagara)")
    sounds = gs.get_all_actors_of_class(w, unreal.AmbientSound)
    classes = sorted(s.get_component_by_class(unreal.AudioComponent).get_editor_property("sound_class_override").get_name() for s in sounds)
    check(classes == ["SC_Music", "SC_SFX"], f"music and wind ambience hooks, on the music and effects sound classes ({classes})")

    # Main menu and focus
    check(top_is(0, "EchoMainMenuScreen"), "the main menu is showing")
    labels = [l for l in ("Play", "Settings", "Credits", "Quit") if button(0, l)]
    check(labels == ["Play", "Settings", "Credits", "Quit"], f"Play, Settings, Credits, Quit ({labels})")
    focus_game()
    yield 0.5
    check(ui.has_focus(button(0, "Play")), f"Play has focus when the menu opens ({ui.get_focused_widget_description()})")

    # Keyboard: arrows move the highlight
    press("Down")
    yield 0.2
    check(ui.has_focus(button(0, "Settings")), "keyboard: Down moves to Settings")
    press("Down")
    yield 0.2
    check(ui.has_focus(button(0, "Credits")), "keyboard: Down again moves to Credits")

    # Gamepad: the d-pad moves the highlight one button at a time, A presses the highlighted one
    order = ["Play", "Settings", "Credits", "Quit"]
    press("Gamepad_DPad_Up")
    yield 0.3
    first = focused_label(0, order)
    press("Gamepad_DPad_Down")
    yield 0.2
    second = focused_label(0, order)
    check(first in order and second in order and order.index(second) == order.index(first) + 1,
          f"gamepad: d-pad down moves the highlight to the next button ({first} -> {second})")
    for _ in range(4):
        current = focused_label(0, order)
        if current == "Settings" or current not in order:
            break
        press("Gamepad_DPad_Up" if order.index(current) > 1 else "Gamepad_DPad_Down")
        yield 0.2
    press("Gamepad_FaceButton_Bottom")
    ok = yield from wait_for(lambda: top_is(0, "EchoSettingsScreen"), 2.0)
    check(ok, "gamepad: A opens Settings")
    press("Gamepad_FaceButton_Right")
    ok = yield from wait_for(lambda: top_is(0, "EchoMainMenuScreen"), 2.0)
    check(ok, "gamepad: B goes back to the main menu")

    # Mouse: hovering highlights, clicking Credits opens it; keyboard Back (P) returns
    ui.hover_widget(button(0, "Quit"))
    yield 0.3
    check(ui.has_focus(button(0, "Quit")), "mouse: hovering a button highlights it (and moves the keyboard focus there)")
    yield from click(0, "Credits")
    ok = yield from wait_for(lambda: top_is(0, "EchoCreditsScreen"), 2.0)
    check(ok, "mouse: clicking Credits opens the credits")
    yield 1.0
    press("P")
    ok = yield from wait_for(lambda: top_is(0, "EchoMainMenuScreen"), 2.0)
    check(ok, "keyboard: P (Back) returns to the main menu")

    # Quit asks first
    yield from click(0, "Quit")
    ok = yield from wait_for(lambda: top_is(0, "EchoDialog"), 2.0)
    check(ok and "Quit" in top(0).get_message_text(), f"Quit asks for confirmation ({top(0).get_message_text() if ok else ''})")
    yield from click(0, "Cancel")
    ok = yield from wait_for(lambda: top_is(0, "EchoMainMenuScreen"), 2.0)
    check(ok, "Cancel keeps the game running")

    yield from settings_checks()


def settings_checks():
    s = settings()
    # Start from defaults so the run is repeatable, key bindings included (only the screen resets those)
    yield from click(0, "Settings")
    yield from wait_for(lambda: top_is(0, "EchoSettingsScreen"), 2.0)
    if top_is(0, "EchoSettingsScreen"):
        top(0).reset_to_defaults()
        top(0).apply_changes()
        press("Gamepad_FaceButton_Right")
        yield from wait_for(lambda: top_is(0, "EchoMainMenuScreen"), 2.0)
    s.set_to_defaults()
    s.set_overall_scalability_level(3)
    s.apply_non_resolution_settings()
    s.save_settings()

    yield from click(0, "Settings")
    ok = yield from wait_for(lambda: top_is(0, "EchoSettingsScreen"), 2.0)
    scr = top(0)
    check(ok, "Settings opens")

    # Tabs: RB moves Graphics -> Audio
    yield 0.3
    press("Gamepad_RightShoulder")
    yield 0.3
    check(scr.get_tab() == 1, f"gamepad: RB switches to the Audio tab (tab {scr.get_tab()})")

    # Keyboard left/right on the focused first row (Master volume) changes it immediately
    press("Left")
    press("Left")
    yield 0.3
    check(abs(s.master_volume - 0.9) < 0.01, f"keyboard: Left twice on Master volume -> 90% ({s.master_volume:.2f})")
    for _ in range(2):
        scr.step_row("Master volume", -1)
    check(abs(s.master_volume - 0.8) < 0.01 and scr.get_row_value("Master volume") == "80%", f"Master volume 80% ({scr.get_row_value('Master volume')})")
    for _ in range(4):
        scr.step_row("Music volume", -1)
    yield 0.5
    w = world(0)
    master_class, music_class = s.get_sound_class_for_volume("Master"), s.get_sound_class_for_volume("Music")
    master_vol = unreal.EchoGameUserSettings.get_effective_sound_class_volume(w, master_class)
    music_vol = unreal.EchoGameUserSettings.get_effective_sound_class_volume(w, music_class)
    check(abs(master_vol - 0.8) < 0.02, f"the Master sound class plays at 80% straight away ({master_vol:.2f})")
    check(abs(music_vol - 0.8 * 0.6) < 0.02 or abs(music_vol - 0.6) < 0.02, f"the Music sound class follows the Music slider (60%, x master: {music_vol:.2f})")
    scr.step_row("Microphone", 1)
    check(not s.microphone_enabled, "the microphone can be switched off")

    # Controls
    scr.select_tab(2)
    yield 0.3
    scr.step_row("Mouse sensitivity", 1)
    scr.step_row("Mouse sensitivity", 1)
    scr.step_row("Gamepad sensitivity", -1)
    scr.step_row("Aim sensitivity", 1)
    scr.step_row("Invert Y", 1)
    scr.step_row("Aim mode", 1)
    check(abs(s.mouse_sensitivity - 1.1) < 0.01 and abs(s.gamepad_sensitivity - 0.95) < 0.01 and abs(s.aim_sensitivity - 0.65) < 0.01,
          f"sensitivities change (mouse {s.mouse_sensitivity:.2f}, gamepad {s.gamepad_sensitivity:.2f}, aim {s.aim_sensitivity:.2f})")
    check(s.invert_y and s.aim_mode == unreal.EchoAimMode.TOGGLE, "Invert Y on, Aim set to Toggle")
    check(abs(s.get_look_scale(False, False) - 1.1) < 0.01 and abs(s.get_look_scale(False, True) - 1.1 * 0.65) < 0.01,
          f"the look scale the character uses follows them ({s.get_look_scale(False, False):.2f}, aiming {s.get_look_scale(False, True):.2f})")
    jump_before = str(scr.get_bound_key("Jump").get_editor_property("key_name"))
    ok = scr.rebind_key("Jump", key("J"))
    jump_after = str(scr.get_bound_key("Jump").get_editor_property("key_name"))
    check(jump_before == "SpaceBar" and ok and jump_after == "J", f"key rebinding: Jump {jump_before} -> {jump_after}")
    forward = str(scr.get_bound_key("MoveForward").get_editor_property("key_name"))
    check(forward == "W", f"movement keys are rebindable too (MoveForward = {forward})")

    # Accessibility
    scr.select_tab(3)
    yield 0.3
    scr.step_row("Subtitle size", 1)
    scr.step_row("Reduce camera shake", 1)
    check(abs(s.subtitle_scale - 1.35) < 0.01 and s.reduce_camera_shake and abs(s.get_camera_shake_scale() - 0.2) < 0.01,
          f"subtitle size Large ({s.subtitle_scale}), reduce camera shake -> shakes at {s.get_camera_shake_scale():.1f}")

    # Graphics
    scr.select_tab(0)
    yield 0.3
    quality_before = s.get_overall_scalability_level()
    scr.step_row("Quality", -1)
    scr.step_row("Frame rate cap", -1)
    vsync_before = s.is_v_sync_enabled()
    scr.step_row("V-sync", 1)
    vsync_after = s.is_v_sync_enabled()
    scr.step_row("Brightness", 1)
    scr.step_row("Window mode", 1)
    check(s.get_overall_scalability_level() == max(0, quality_before - 1), f"quality preset applies ({quality_before} -> {s.get_overall_scalability_level()})")
    check(s.get_frame_rate_limit() in (144.0, 120.0, 60.0, 30.0) and vsync_after != vsync_before and abs(s.brightness - 1.05) < 0.01,
          f"frame rate cap {s.get_frame_rate_limit():.0f}, v-sync {vsync_before} -> {vsync_after}, brightness {s.brightness:.2f}")
    check(scr.has_unsaved_changes(), "the screen knows there are unsaved changes")

    # Back with unsaved changes asks; Apply saves
    press("Gamepad_FaceButton_Right")
    ok = yield from wait_for(lambda: top_is(0, "EchoDialog"), 2.0)
    check(ok and "Keep" in top(0).get_message_text(), "leaving with unsaved changes asks to apply or discard them")
    yield from click(0, "Apply")
    ok = yield from wait_for(lambda: top_is(0, "EchoMainMenuScreen"), 2.0)
    check(ok, f"Apply saves and closes Settings ({top(0).get_class().get_name() if top(0) else None}: {top(0).get_message_text() if top_is(0, 'EchoDialog') else ''})")

    # Saved to disk, and loads back
    ini = os.path.join(PROJECT, "Saved", "Config", "WindowsEditor", "GameUserSettings.ini")
    text = open(ini, encoding="utf-8", errors="ignore").read() if os.path.exists(ini) else ""
    # Only our own section (the file also has other classes' keys, e.g. an old MasterVolume)
    text = text.split("[/Script/OurLastEcho.EchoGameUserSettings]", 1)[-1].split("\n[", 1)[0]
    for name, value in (("MasterVolume", "0.8"), ("MusicVolume", "0.6"), ("MouseSensitivity", "1.1"), ("bInvertY", "True"),
                        ("AimMode", "Toggle"), ("SubtitleScale", "1.35"), ("bReduceCameraShake", "True"), ("bMicrophoneEnabled", "False")):
        m = re.search(rf"^{name}=(.*)$", text, re.M)
        saved = m.group(1).strip() if m else None
        same = saved is not None and (saved == value or (re.match(r"^[\d.]+$", value) and abs(float(saved) - float(value)) < 0.001))
        check(same, f"saved {name}={saved} (want {value})")
    s.load_settings(True)
    check(abs(s.master_volume - 0.8) < 0.01 and s.invert_y and abs(s.subtitle_scale - 1.35) < 0.01, "loading the settings file brings them back")

    # Discard puts things back
    yield from click(0, "Settings")
    yield from wait_for(lambda: top_is(0, "EchoSettingsScreen"), 2.0)
    scr = top(0)
    scr.select_tab(1)
    scr.step_row("Master volume", -1)
    check(abs(s.master_volume - 0.75) < 0.01, "a new change applies straight away (master 75%)")
    press("Gamepad_FaceButton_Right")
    yield from wait_for(lambda: top_is(0, "EchoDialog"), 2.0)
    yield from click(0, "Discard")
    ok = yield from wait_for(lambda: top_is(0, "EchoMainMenuScreen"), 2.0)
    check(ok and abs(s.master_volume - 0.8) < 0.01, f"Discard puts it back ({s.master_volume:.2f})")

    # The rebinding was saved: it's still J when Settings opens again; then Reset to Defaults puts everything back
    yield from click(0, "Settings")
    yield from wait_for(lambda: top_is(0, "EchoSettingsScreen"), 2.0)
    scr = top(0)
    check(str(scr.get_bound_key("Jump").get_editor_property("key_name")) == "J", "the Jump rebinding is still there when Settings opens again")
    yield from click(0, "Reset to Defaults")
    yield from wait_for(lambda: top_is(0, "EchoDialog"), 2.0)
    yield from click(0, "Reset")
    yield from wait_for(lambda: top_is(0, "EchoSettingsScreen"), 2.0)
    scr = top(0)
    check(abs(s.master_volume - 1.0) < 0.01 and not s.invert_y and str(scr.get_bound_key("Jump").get_editor_property("key_name")) == "SpaceBar",
          "Reset to Defaults resets the settings and key bindings")
    scr.apply_changes()
    press("Gamepad_FaceButton_Right")
    yield from wait_for(lambda: top_is(0, "EchoMainMenuScreen"), 2.0)


# ------------------------------------------------------------------ online (2 players)

def lobby_players(index):
    t = top(index)
    return t.get_players_text() if t and t.get_class().get_name() == "EchoLobbyScreen" else ""


def host_and_join():
    # Player 1 hosts through the menus
    yield from click(0, "Play")
    yield from wait_for(lambda: top_is(0, "EchoPlayMenuScreen"), 2.0)
    yield from click(0, "Host Game")
    ok = yield from wait_for(lambda: net_mode(0) == "server" and top_is(0, "EchoLobbyScreen"), 20.0)
    check(ok, f"player 1 hosts: a lobby opens on a listen server ({net_mode(0)}, {map_name(0)})")
    yield 1.0

    # Player 2 finds the game and joins
    yield from click(1, "Play")
    yield from wait_for(lambda: top_is(1, "EchoPlayMenuScreen"), 2.0)
    yield from click(1, "Join Game")
    yield from wait_for(lambda: top_is(1, "EchoJoinMenuScreen"), 2.0)
    ok = yield from wait_for(lambda: top(1).get_num_games_listed() >= 1, 10.0)
    check(ok, f"player 2 finds the hosted game on the network ({top(1).get_num_games_listed() if top_is(1, 'EchoJoinMenuScreen') else 0} listed)")
    if not ok:
        return False
    top(1).join_first_game()
    ok = yield from wait_for(lambda: net_mode(1) == "client" and top_is(1, "EchoLobbyScreen"), 20.0)
    check(ok, f"player 2 joins the lobby ({net_mode(1)})")
    ok = yield from wait_for(lambda: "waiting" not in lobby_players(0) and "Saraa:" in lobby_players(0), 5.0)
    return ok


def online_scenario():
    if not check(len(worlds()) == 2 and map_name(0) == "TitleScreen" and map_name(1) == "TitleScreen", "two players on the title screen"):
        return
    # Park player 2's window in the bottom-right corner: it floats above the editor and would catch clicks meant
    # for player 1's menus (the window stays put across map changes)
    moved = ui.move_widget_window(root(1), unreal.Vector2D(1265.0, 500.0))
    log(f"player 2's window moved out of the way: {moved}")
    yield 0.5
    ok = yield from host_and_join()
    if not ok:
        return

    host_text, guest_text = lobby_players(0), lobby_players(1)
    check("Bat (host)" in host_text and "(you)" in host_text.split("|")[0] and "Saraa:" in host_text, f"host's lobby: {host_text}")
    check("Bat (host)" in guest_text and "(you)" in guest_text.split("|")[1], f"guest's lobby: {guest_text}")
    start = button(0, "Start Game")
    check(start is not None and not start.get_is_enabled(), "Start is disabled until both are ready")
    check(button(1, "Start Game").get_visibility() == unreal.SlateVisibility.COLLAPSED, "only the host has a Start button")

    # Both ready (the guest's click goes to the server)
    yield from click(0, "Ready")
    yield from click(1, "Ready", queued=True)
    ok = yield from wait_for(lambda: lobby_players(0).count(" Ready") == 2, 5.0)
    check(ok, f"both players ready, seen on the host ({lobby_players(0)})")
    ok = yield from wait_for(lambda: lobby_players(1).count(" Ready") == 2, 5.0)
    check(ok, "and on the guest's screen")
    ok = yield from wait_for(lambda: button(0, "Start Game").get_is_enabled(), 3.0)
    check(ok, "Start is enabled once both are ready")

    # Start: everyone travels to the game
    yield from click(0, "Start Game", queued=True)  # travel sends client RPCs: must not run inside Python
    ok = yield from wait_for(lambda: map_name(0) == "Lvl_SpiritPath" and map_name(1) == "Lvl_SpiritPath"
                             and pawn(0) is not None and pawn(1) is not None, 40.0)
    check(ok, f"Start takes both players into the game ({map_name(0)}, {map_name(1)}, pawns {pawn(0) is not None}/{pawn(1) is not None})")
    if not ok:
        return
    yield 2.0
    host_realm, guest_realm = pawn(0).get_realm(), pawn(1).get_realm()
    check(host_realm == unreal.EchoRealm.LIVING and guest_realm == unreal.EchoRealm.SPIRIT, f"the host plays Bat, the second player Saraa ({host_realm}, {guest_realm})")

    # In-game menu: shows over the game, which keeps running
    guest = pc(1)
    guest.toggle_pause_menu()
    ok = yield from wait_for(lambda: top_is(1, "EchoPauseMenuScreen"), 2.0)
    check(ok, "the in-game menu opens (Esc / P / Start)")
    t0 = gs.get_time_seconds(world(0))
    yield 1.0
    check(not gs.is_game_paused(world(0)) and not gs.is_game_paused(world(1)) and gs.get_time_seconds(world(0)) > t0 + 0.5,
          "the game doesn't pause (it's online)")
    labels = [l for l in ("Resume", "Settings", "Leave Game") if button(1, l)]
    check(labels == ["Resume", "Settings", "Leave Game"], f"Resume, Settings, Leave Game ({labels})")
    yield from click(1, "Settings")
    ok = yield from wait_for(lambda: top_is(1, "EchoSettingsScreen"), 2.0)
    check(ok, "Settings opens from the in-game menu (the same settings screen)")
    press("Gamepad_FaceButton_Right")
    yield from wait_for(lambda: top_is(1, "EchoPauseMenuScreen"), 2.0)
    yield from click(1, "Resume")
    ok = yield from wait_for(lambda: not guest.is_pause_menu_open(), 2.0)
    check(ok, "Resume closes the menu and returns to the game")

    # The guest leaves: both end up on the title screen, the host with a message
    guest.toggle_pause_menu()
    yield from wait_for(lambda: top_is(1, "EchoPauseMenuScreen"), 2.0)
    yield from click(1, "Leave Game")
    yield from wait_for(lambda: top_is(1, "EchoDialog"), 2.0)
    yield from click(1, "Leave")
    ok = yield from wait_for(lambda: map_name(0) == "TitleScreen" and map_name(1) == "TitleScreen" and top(0) is not None, 30.0)
    check(ok, f"Leave Game returns both players to the title screen ({map_name(0)}, {map_name(1)})")
    ok = yield from wait_for(lambda: top_is(0, "EchoDialog") and "Saraa left" in top(0).get_message_text(), 5.0)
    check(ok, f"the host is told why ({top(0).get_message_text() if top_is(0, 'EchoDialog') else top(0).get_class().get_name() if top(0) else None})")
    if top_is(0, "EchoDialog"):
        yield from click(0, "OK")
        yield 1.0

    # The host leaves during the game: the guest returns to the title screen with a message
    ok = yield from host_and_join()
    if ok:
        yield from click(0, "Ready")
        yield from click(1, "Ready", queued=True)
        yield from wait_for(lambda: button(0, "Start Game").get_is_enabled(), 5.0)
        yield from click(0, "Start Game", queued=True)
        ok = yield from wait_for(lambda: map_name(0) == "Lvl_SpiritPath" and map_name(1) == "Lvl_SpiritPath"
                                 and pawn(0) is not None and pawn(1) is not None, 40.0)
        check(ok, "a second game starts")
        yield 2.0
        pc(0).toggle_pause_menu()
        yield from wait_for(lambda: top_is(0, "EchoPauseMenuScreen"), 2.0)
        yield from click(0, "Leave Game")
        yield from wait_for(lambda: top_is(0, "EchoDialog"), 2.0)
        yield from click(0, "Leave")
        ok = yield from wait_for(lambda: map_name(0) == "TitleScreen" and map_name(1) == "TitleScreen" and net_mode(1) == "standalone"
                                 and top_is(1, "EchoDialog"), 30.0)
        message = top(1).get_message_text() if top_is(1, "EchoDialog") else ""
        check(ok and "host left" in message, f"when the host leaves the game, both return to the title screen and the guest is told ({message})")
        if top_is(1, "EchoDialog"):
            yield from click(1, "OK")
        yield 1.0

    # The host leaves the lobby: the guest returns to the title screen with a message
    yield from wait_for(lambda: top_is(0, "EchoMainMenuScreen") and top_is(1, "EchoMainMenuScreen"), 5.0)
    ok = yield from host_and_join()
    if ok:
        yield from click(0, "Leave Lobby")
        yield from wait_for(lambda: top_is(0, "EchoDialog"), 2.0)
        yield from click(0, "Leave")
        ok = yield from wait_for(lambda: net_mode(1) == "standalone" and map_name(1) == "TitleScreen" and top_is(1, "EchoDialog"), 30.0)
        message = top(1).get_message_text() if top_is(1, "EchoDialog") else ""
        check(ok and "host left" in message, f"when the host leaves the lobby, the guest goes back to the title screen and is told ({message})")
        if top_is(1, "EchoDialog"):
            yield from click(1, "OK")
        yield 1.0

    # Joining a game that has just closed fails with a message and a way back
    yield from wait_for(lambda: top_is(0, "EchoMainMenuScreen") and top_is(1, "EchoMainMenuScreen"), 5.0)
    yield from click(0, "Play")
    yield from wait_for(lambda: top_is(0, "EchoPlayMenuScreen"), 2.0)
    yield from click(0, "Host Game")
    yield from wait_for(lambda: net_mode(0) == "server" and top_is(0, "EchoLobbyScreen"), 20.0)
    yield 1.0
    yield from click(1, "Play")
    yield from wait_for(lambda: top_is(1, "EchoPlayMenuScreen"), 2.0)
    yield from click(1, "Join Game")
    yield from wait_for(lambda: top_is(1, "EchoJoinMenuScreen"), 2.0)
    listed = yield from wait_for(lambda: top(1).get_num_games_listed() >= 1, 10.0)
    yield from click(0, "Leave Lobby")
    yield from wait_for(lambda: top_is(0, "EchoDialog"), 2.0)
    yield from click(0, "Leave")
    yield from wait_for(lambda: net_mode(0) == "standalone" and top_is(0, "EchoMainMenuScreen"), 20.0)
    yield 1.0
    if listed:
        top(1).join_first_game()
    ok = yield from wait_for(lambda: net_mode(1) == "standalone" and top_is(1, "EchoDialog") and "connect" in top(1).get_message_text(), 90.0)
    message = top(1).get_message_text() if top_is(1, "EchoDialog") else (top(1).get_class().get_name() if top(1) else None)
    check(listed and ok, f"joining a game that has closed fails with a message ({message})")
    if top_is(1, "EchoDialog"):
        yield from click(1, "OK")
    ok = yield from wait_for(lambda: top_is(1, "EchoMainMenuScreen") or top_is(1, "EchoJoinMenuScreen"), 5.0)
    check(ok, f"and OK leads back to the menus ({top(1).get_class().get_name() if top(1) else None})")
    for _ in range(3):
        if top_is(1, "EchoMainMenuScreen") or not button(1, "Back"):
            break
        yield from click(1, "Back")
        yield 0.5

    # Nothing to join
    yield from wait_for(lambda: top_is(0, "EchoMainMenuScreen") and top_is(1, "EchoMainMenuScreen"), 5.0)
    yield from click(1, "Play")
    yield from wait_for(lambda: top_is(1, "EchoPlayMenuScreen"), 2.0)
    yield from click(1, "Join Game")
    yield from wait_for(lambda: top_is(1, "EchoJoinMenuScreen"), 2.0)
    yield 5.0
    check(top_is(1, "EchoJoinMenuScreen") and top(1).get_num_games_listed() == 0, "with nobody hosting, Join Game lists no games (and says so)")


def scenario():
    log(f"mode {MODE}, {len(worlds())} PIE player(s)")
    if MODE == "online":
        yield from online_scenario()
    else:
        yield from title_scenario()


def on_tick(delta):
    T["elapsed"] += delta
    if T["wait"] > 0:
        T["wait"] -= delta
        return
    try:
        T["wait"] = next(T["gen"]) or 0.0
    except StopIteration:
        finish()
    except Exception as e:
        import traceback
        check(False, f"crashed: {e!r} {traceback.format_exc()}")
        finish()


def finish():
    unreal.unregister_slate_post_tick_callback(T["handle"])
    log(f"{T['checks']} checks in {T['elapsed']:.0f}s")
    log("PASS" if T["failures"] == 0 else f"FAIL ({T['failures']} failures)")


T["gen"] = scenario()
T["handle"] = unreal.register_slate_post_tick_callback(on_tick)
log("started")
