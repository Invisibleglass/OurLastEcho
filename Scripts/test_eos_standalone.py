"""
Milestone 7: two real game processes on one PC play online through Epic Online Services, signed in as two Epic
accounts with the EOS Developer Authentication Tool (credentials "Bat" and "Saraa", see NOTES.md).
Run by Scripts/run_eos_test.ps1 (editor closed), which sets ECHO_EOS_ROLE ("host" / "guest") and ECHO_EOS_DIR
(a folder both processes share, for handing over the join code).

Host: signs in, hosts, shows a join code, waits for the guest, readies, starts, checks the game (Bat, the present,
EOS peer-to-peer transport, the voice room), then leaves for the title screen.
Guest: signs in, first tries a wrong code (must fail with a message and stay on the Join screen), joins with the
host's code through the Join screen, readies, checks the game (Saraa, the past, EOS transport, voice room), and
then must be sent back to the title screen with "The host left the game." when the host leaves.
Output lines are tagged ECHO_EOS (host) / ECHO_EOS_GUEST; each process ends with PASS or FAIL (n failures).
"""
import os
import unreal

ROLE = globals().get("ECHO_EOS_ROLE", "host")
SHARED = globals().get("ECHO_EOS_DIR", "")
TAG = "ECHO_EOS" if ROLE == "host" else "ECHO_EOS_GUEST"
gs = unreal.GameplayStatics
lib = unreal.EchoUITestLibrary
T = {"failures": 0, "wait": 0.0, "handle": None, "gen": None}
MAPS = ("/Game/Echo/Maps/Lvl_SpiritPath.Lvl_SpiritPath", "/Game/Echo/Maps/TitleScreen.TitleScreen")
CODE_FILE = os.path.join(SHARED, "join_code.txt")
GUEST_DONE = os.path.join(SHARED, "guest_checked.txt")


def log(msg):
    unreal.log(f"{TAG}: {msg}")


def check(ok, text):
    T["failures"] += 0 if ok else 1
    log(f"{'ok  ' if ok else 'FAIL'} {text}")
    return ok


def world():
    for path in MAPS:
        w = unreal.find_object(None, path)
        if w and gs.get_player_controller(w, 0):
            return w
    return None


def map_name():
    w = world()
    return w.get_name() if w else ""


def run(command):
    w = world()
    if w:
        unreal.SystemLibrary.execute_console_command(w, command)


def net_mode():
    w = world()
    if not w:
        return "none"
    if unreal.SystemLibrary.is_standalone(w):
        return "standalone"
    return "server" if unreal.SystemLibrary.is_server(w) else "client"


def players():
    w = world()
    state = gs.get_game_state(w) if w else None
    return list(state.get_editor_property("player_array")) if state else []


def all_ready():
    ps = players()
    return len(ps) == 2 and all(p.is_ready() for p in ps if hasattr(p, "is_ready"))


def local_realm():
    w = world()
    pawn = gs.get_player_pawn(w, 0) if w else None
    return pawn.get_realm() if pawn and hasattr(pawn, "get_realm") else None


def shown_era():
    w = world()
    found = gs.get_all_actors_of_class(w, unreal.EchoEraDirector) if w else []
    if not found or not found[0].is_ready():
        return "none"
    d = found[0]
    present, past = d.is_era_loaded(unreal.EchoEra.PRESENT), d.is_era_loaded(unreal.EchoEra.PAST)
    return "both" if present and past else "present" if present else "past" if past else "none"


def login():
    w = world()
    return lib.get_login(w) if w else None


def logged_in():
    l = login()
    return l is not None and l.is_logged_in()


def stats():
    w = world()
    d = lib.get_net_diagnostics(w) if w else None
    return d.get_stats() if d else None


def transport():
    s = stats()
    return s.transport if s and s.connected else ""


def in_voice_room():
    w = world()
    v = lib.get_voice(w) if w else None
    return v is not None and v.is_in_voice_room()


def voice_status():
    w = world()
    v = lib.get_voice(w) if w else None
    return v.get_status_text() if v else ""


def top():
    w = world()
    pc = gs.get_player_controller(w, 0) if w else None
    root = pc.get_ui_root() if pc and hasattr(pc, "get_ui_root") else None
    return root.get_top_screen() if root else None


def top_is(name):
    t = top()
    return t is not None and t.get_class().get_name() == name


def queue_click(label):
    t = top()
    button = t.find_button(label) if t else None
    if not button or not button.get_is_enabled():
        return False
    lib.queue_click_widget(button)
    return True


def wait_for(cond, timeout):
    t = 0.0
    while t < timeout:
        try:
            if cond():
                return True
        except Exception:
            pass
        yield 0.25
        t += 0.25
    return False


def common_login():
    # The first sign-in of each account shows Epic's consent screen in the game (overlay): someone clicks Allow
    ok = yield from wait_for(lambda: world() is not None and logged_in(), 300.0)
    l = login()
    check(ok, f"signed in with an Epic account ({l.get_display_name() if l else '?'}, service {lib.get_online_service(world()) if world() else '?'})")
    if not ok and l:
        log(f"login error: {l.get_error_text()}")
    ok = yield from wait_for(lambda: top_is("EchoMainMenuScreen"), 10.0)
    check(ok, f"the main menu follows the sign-in ({top().get_class().get_name() if top() else None})")
    return logged_in()


def in_game_checks(role_realm, era):
    ok = yield from wait_for(lambda: map_name() == "Lvl_SpiritPath" and len(players()) == 2 and local_realm() is not None, 90.0)
    check(ok, f"in the game together ({map_name()}, {len(players())} players)")
    check(local_realm() == role_realm, f"playing the right role ({local_realm()})")
    ok = yield from wait_for(lambda: shown_era() == era, 30.0)
    check(ok, f"only the {era} era loaded ({shown_era()})")
    ok = yield from wait_for(lambda: transport() != "", 10.0)
    check(transport() == "EOS P2P", f"connected peer to peer through EOS ({transport()}, ping {stats().ping_ms:.0f} ms)")
    ok = yield from wait_for(in_voice_room, 30.0)
    check(ok, f"in the voice room ({voice_status()})")


def host():
    if not (yield from common_login()):
        return
    run("EchoHost")
    ok = yield from wait_for(lambda: net_mode() == "server" and map_name() == "TitleScreen" and top_is("EchoLobbyScreen"), 30.0)
    check(ok, f"hosting: the lobby is open ({net_mode()})")
    sessions = lib.get_sessions(world())
    code = sessions.get_join_code() if sessions else ""
    check(len(code) == 6, f"the lobby has a 6-character join code ({len(code)} characters)")
    ok = yield from wait_for(lambda: "Join code" in top().get_code_text(), 5.0)
    check(ok, "the lobby shows the join code")
    with open(CODE_FILE, "w") as f:
        f.write(code)

    ok = yield from wait_for(lambda: len(players()) == 2, 150.0)
    check(ok, f"the guest joined with the code ({len(players())} players)")
    if not ok:
        return
    log(f"lobby: {top().get_players_text() if top_is('EchoLobbyScreen') else '?'}")
    yield 1.0
    log(f"Ready -> {queue_click('Ready')}")
    ok = yield from wait_for(all_ready, 40.0)
    check(ok, "both players are ready")
    yield 1.0
    log(f"Start Game -> {queue_click('Start Game')}")
    yield from in_game_checks(unreal.EchoRealm.LIVING, "present")

    ok = yield from wait_for(lambda: os.path.exists(GUEST_DONE), 90.0)
    check(ok, "the guest finished its in-game checks")
    yield 2.0
    log("host leaves the game (EchoLeave)")
    run("EchoLeave")
    ok = yield from wait_for(lambda: map_name() == "TitleScreen" and net_mode() == "standalone", 30.0)
    check(ok, f"the host is back on the title screen ({map_name()}, {net_mode()})")
    yield 12.0  # give the guest time to report


def guest():
    if not (yield from common_login()):
        return
    ok = yield from wait_for(lambda: os.path.exists(CODE_FILE) and open(CODE_FILE).read().strip() != "", 150.0)
    check(ok, "got the host's join code")
    if not ok:
        return
    code = open(CODE_FILE).read().strip()

    # Play > Join Game
    log(f"Play -> {top().click_button('Play')}")
    yield from wait_for(lambda: top_is("EchoPlayMenuScreen"), 5.0)
    yield 0.5
    log(f"Join Game -> {top().click_button('Join Game')}")
    ok = yield from wait_for(lambda: top_is("EchoJoinMenuScreen"), 5.0)
    check(ok, "the Join screen opens")
    yield 0.5

    # A wrong code fails with a message, and leaves us on the Join screen
    join_screen = top()
    join_screen.join_with_code("ZZZZZZ")
    ok = yield from wait_for(lambda: top_is("EchoDialog") and "No game" in top().get_message_text(), 30.0)
    check(ok, f"a wrong code says so ({top().get_message_text() if top_is('EchoDialog') else top().get_class().get_name() if top() else None})")
    if top_is("EchoDialog"):
        top().click_button("OK")
    ok = yield from wait_for(lambda: top_is("EchoJoinMenuScreen"), 5.0)
    check(ok, "... and the Join screen is still there")
    yield 0.5

    top().join_with_code(code.lower())
    ok = yield from wait_for(lambda: net_mode() == "client" and map_name() == "TitleScreen" and top_is("EchoLobbyScreen"), 60.0)
    check(ok, f"joined the host's lobby with the code ({net_mode()}, {map_name()})")
    if not ok:
        return
    yield 1.0
    log(f"Ready -> {queue_click('Ready')}")
    yield from in_game_checks(unreal.EchoRealm.SPIRIT, "past")
    with open(GUEST_DONE, "w") as f:
        f.write("done")

    ok = yield from wait_for(lambda: map_name() == "TitleScreen" and net_mode() == "standalone", 60.0)
    check(ok, f"when the host leaves, the guest goes back to the title screen ({map_name()}, {net_mode()})")
    ok = yield from wait_for(lambda: top_is("EchoDialog"), 10.0)
    message = top().get_message_text() if top_is("EchoDialog") else ""
    check("host left" in message, f"... and is told why ('{message}')")
    yield 2.0


def scenario():
    log(f"started ({ROLE})")
    yield from (host() if ROLE == "host" else guest())


def on_tick(delta):
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
    log("PASS" if T["failures"] == 0 else f"FAIL ({T['failures']} failures)")
    w = world()
    if w:
        unreal.SystemLibrary.execute_console_command(w, "quit")


T["gen"] = scenario()
T["handle"] = unreal.register_slate_post_tick_callback(on_tick)
