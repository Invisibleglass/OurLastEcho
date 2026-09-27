"""
Milestone 6 performance check, in a running 2-player gameplay PIE session (turn background throttling off first; for
real numbers keep the editor focused). Measures the editor's frame rate (both PIE views render every frame) with:
  normal   - host in the present, guest in the past (each machine loads one era)
  present  - both views showing the present (EchoEra present on the guest)
  past     - both views showing the past
  both     - the guest loading BOTH eras (EchoShowBothEras), i.e. one extra era's cost
Prints ECHO_ERAPERF lines, including each phase's start and end so a process-memory log can be matched to it.
"""
import time

import unreal

gs = unreal.GameplayStatics
ui = unreal.EchoUITestLibrary
SETTLE, MEASURE = 6.0, 5.0
S = {"phase": 0, "t0": None, "frames": 0, "worst": 0.0, "last": None, "handle": None}


def log(msg):
    unreal.log(f"ECHO_ERAPERF: {msg}")


pie = list(unreal.EditorLevelLibrary.get_pie_worlds(False))
server = next(w for w in pie if unreal.SystemLibrary.is_server(w))
client = next(w for w in pie if not unreal.SystemLibrary.is_server(w))
host_pc, guest_pc = gs.get_player_controller(server, 0), gs.get_player_controller(client, 0)

PHASES = [
    ("normal", []),
    ("present", [(guest_pc, "EchoEra present")]),
    ("past", [(guest_pc, "EchoEra past"), (host_pc, "EchoEra past")]),
    ("both", [(host_pc, "EchoEra auto"), (guest_pc, "EchoEra auto"), (guest_pc, "EchoShowBothEras")]),
    ("normal again", [(guest_pc, "EchoShowBothEras")]),
]


def start_phase():
    name, commands = PHASES[S["phase"]]
    for pc, cmd in commands:
        ui.queue_console_command(pc, cmd)
    S["t0"], S["frames"], S["worst"], S["last"] = time.perf_counter(), 0, 0.0, None
    log(f"phase {name} start {time.time():.1f}")


def tick(delta):
    now = time.perf_counter()
    name = PHASES[S["phase"]][0]
    elapsed = now - S["t0"]
    if elapsed < SETTLE:
        return
    if S["last"] is not None:
        S["frames"] += 1
        S["worst"] = max(S["worst"], now - S["last"])
    S["last"] = now
    if elapsed >= SETTLE + MEASURE:
        fps = S["frames"] / (elapsed - SETTLE)
        eras = []
        for w in (server, client):
            d = gs.get_all_actors_of_class(w, unreal.EchoEraDirector)[0]
            eras.append("+".join(e for e, v in (("present", d.is_era_loaded(unreal.EchoEra.PRESENT)), ("past", d.is_era_loaded(unreal.EchoEra.PAST))) if v))
        log(f"phase {name} end {time.time():.1f}: {fps:.1f} fps, worst frame {S['worst'] * 1000:.0f} ms; host loads [{eras[0]}], guest loads [{eras[1]}]")
        S["phase"] += 1
        if S["phase"] >= len(PHASES):
            unreal.unregister_slate_post_tick_callback(S["handle"])
            log("done")
            return
        start_phase()


start_phase()
S["handle"] = unreal.register_slate_post_tick_callback(tick)
