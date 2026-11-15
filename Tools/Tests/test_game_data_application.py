"""실제 게임 EXE의 dat 필수 로드·cwd 독립·무재컴파일 데이터 반영 검사."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import time

from application_observers import FileActivityWatch

ROOT = Path(__file__).resolve().parents[2]


class Coord(c.Structure):
    _fields_ = [("X", w.SHORT), ("Y", w.SHORT)]


class Rect(c.Structure):
    _fields_ = [("Left", w.SHORT), ("Top", w.SHORT), ("Right", w.SHORT), ("Bottom", w.SHORT)]


class Character(c.Union):
    _fields_ = [("unicode", w.WCHAR), ("ascii", c.c_char)]


class Cell(c.Structure):
    _fields_ = [("character", Character), ("attributes", w.WORD)]


class Info(c.Structure):
    _fields_ = [("size", Coord), ("cursor", Coord), ("attributes", w.WORD), ("window", Rect), ("maximum", Coord)]


class Font(c.Structure):
    _fields_ = [("cbSize", w.ULONG), ("index", w.DWORD), ("size", Coord), ("family", w.UINT), ("weight", w.UINT), ("face", w.WCHAR * 32)]


class Key(c.Structure):
    _fields_ = [("down", w.BOOL), ("repeat", w.WORD), ("vk", w.WORD), ("scan", w.WORD), ("char", w.WCHAR), ("modifiers", w.DWORD)]


class Event(c.Union):
    _fields_ = [("key", Key), ("bytes", c.c_byte * 16)]


class Record(c.Structure):
    _fields_ = [("kind", w.WORD), ("event", Event)]


def generated_rows(raw):
    """Read only the generator's JSON-quoted mapping/scalar YAML dialect."""
    rows = []
    decoder = json.JSONDecoder()
    for line in raw.decode("utf-8-sig").splitlines():
        if not line or line.startswith("#"):
            continue
        key, end = decoder.raw_decode(line.lstrip())
        tail = line.lstrip()[end:]
        assert isinstance(key, str) and tail.startswith(":"), "generated mapping expected"
        if not line.startswith(" "):
            assert tail == ":", "generated row expected"
            rows.append({})
        else:
            assert line.startswith("  ") and not line.startswith("   ") and rows
            assert key not in rows[-1], "duplicate generated field"
            rows[-1][key] = json.loads(tail[1:].strip())
    return rows


def scene_labels(client=None, changed=False):
    client = client or ROOT / "GameData/Data/Client"
    options = generated_rows((client / "SceneOption.yaml").read_bytes())
    localized = {row["TextKey"]: row["KoKR"]
                 for row in generated_rows((client / "Localization.yaml").read_bytes())}
    if changed:
        start = [row for row in options if row["SceneType"] == "MainMenu"
                 and row["ActionType"] == "StartGame"]
        assert len(start) == 1
        localized[start[0]["OptionTextKey"]] = "DAT_RELOAD_OK"
    labels = {}
    for scene in ("MainMenu", "Town", "BattleField"):
        selected = sorted((row for row in options if row["SceneType"] == scene),
                          key=lambda row: row["OptionOrder"])
        assert [row["OptionOrder"] for row in selected] == [1, 2, 3, 4]
        labels[scene] = [f'{row["OptionOrder"]}. {localized[row["OptionTextKey"]]}'
                         for row in selected]
    labels["MainMenuTitle"] = localized["Scene.MainMenu.Title"]
    return labels


def physical_text(text, width, fill=" "):
    """FFrameBuffer 셀 폭/CHAR_INFO 복제와 UI DrawText의 전체 glyph clip을 따른다."""
    cells = ""
    for glyph in text:
        point = ord(glyph)
        if (0x0300 <= point <= 0x036F or 0x1AB0 <= point <= 0x1AFF
                or 0x1DC0 <= point <= 0x1DFF or 0xFE00 <= point <= 0xFE0F
                or 0xFE20 <= point <= 0xFE2F or 0xE0100 <= point <= 0xE01EF
                or point == 0x200D):
            continue
        wide = (0x1100 <= point <= 0x11FF or 0x2E80 <= point <= 0xA4CF
                or 0xAC00 <= point <= 0xD7A3 or 0xF900 <= point <= 0xFAFF
                or 0xFE10 <= point <= 0xFE6F or 0xFF01 <= point <= 0xFF60
                or 0xFFE0 <= point <= 0xFFE6 or 0x1F300 <= point <= 0x1FAFF)
        count = 2 if wide else 1
        if len(cells) + count > width:
            break
        glyph = " " if glyph in "\r\n" else glyph
        if point > 0xFFFF:
            units = glyph.encode("utf-16-le")
            physical = "".join(chr(int.from_bytes(units[i:i + 2], "little"))
                               for i in range(0, count * 2, 2))
        else:
            physical = glyph * count
        cells += physical
    return cells + fill * (width - len(cells))


def scene_matches(value, scene, labels, changed=False, previous_menu=None):
    # FDataScene: 132x43, optionPanel content origin=(87,1)/(87,12).
    # FUiPanel: content inset=1. FUiButton: local x=2,w=38,h=3,
    # text inset=1 => x=90, 36 physical text cells (including trailing blanks).
    rows = value.split("\n")
    if len(rows) != 43 or any(len(row) != 132 for row in rows):
        return False
    if scene not in ("MainMenu", "Town") or len(labels.get(scene, [])) != 4:
        return False

    def box_matches(x, y, width, height, title=""):
        # SUiRenderContext::DrawBorder: keep x+1 dash, title x+2,width-3.
        top = "┌─" + physical_text(title, width - 3, "─") + "┐"
        return (rows[y][x:x + width] == top
                and rows[y + height - 1][x:x + width] == "└" + "─" * (width - 2) + "┘"
                and all(rows[line][x] == rows[line][x + width - 1] == "│"
                        for line in range(y + 1, y + height - 1)))

    if scene == "MainMenu":
        if not box_matches(86, 0, 46, 43, labels["MainMenuTitle"]):
            return False
        button_rows = (4, 13, 22, 31)
    else:
        panels = [(x, y, 42, height) for x in (0, 43)
                  for y, height in ((0, 5), (5, 38))]
        panels.extend(((86, 0, 46, 11), (86, 11, 46, 32)))
        if not all(box_matches(*rect) for rect in panels):
            return False
        button_rows = (14, 21, 28, 35)
    # changed/previous_menu remain API-compatible, never serve as Scene identity.
    return all(box_matches(89, y, 38, 3)
               and rows[y + 1][90:126] == physical_text(label, 36)
               for y, label in zip(button_rows, labels[scene]))


def progress_matches(value, expected, labels):
    """진행 값은 production fixture에서, 문구는 패킹한 Excel YAML에서 공급한다."""
    rows = value.split("\n")
    if len(rows) != 43 or any(len(row) != 132 for row in rows):
        return False
    def number(key, name):
        return f'{labels[key]}: {expected[name]}'
    fields = [
        (2, 2, number("Hp", "hp") + "/" + str(expected["maxHp"])),
        (2, 3, number("Attack", "attack") + "  " + number("Defense", "defense")),
        (45, 1, number("Gold", "gold")),
        (45, 2, number("Inventory", "inventoryCount")),
        (45, 3, number("Achievements", "achievementCount")),
    ]
    return all(rows[y][x:x + 38] == physical_text(text, 38) for x, y, text in fields)


def progress_acceptance(fixture, game, dat, cwd, env, work, options, app, runtime_temp,
                        save_root, results, save_watch, assert_no_extraction):
    """별도 프로세스의 실제 저장/복원과 기존 정밀 Scene 오라클을 함께 검사한다."""
    localized = {row["TextKey"]: row["KoKR"] for row in
                 generated_rows((ROOT / "GameData/Data/Client/Localization.yaml").read_bytes())}
    labels = {name: localized["InGameData.Ui." + name] for name in
              ("Hp", "Attack", "Defense", "Gold", "Inventory", "Achievements")}

    def invoke(mode, name):
        completed = subprocess.run([str(fixture.resolve()), mode, str(dat), str(save_root)],
                                   cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        (work / (name + ".log")).write_bytes(completed.stdout)
        assert completed.returncode == 0, name + " production fixture failed"
        value = json.loads(completed.stdout.decode("utf-8").strip())
        assert set(value) == {"hp", "gold", "maxHp", "attack", "defense", "inventoryCount", "achievementCount"}
        return value

    defaults = invoke("--inspect-save", "newgame-inspect")
    expected = invoke("--prepare-save", "changed-progress-prepare")
    assert expected["hp"] != defaults["hp"] and expected["gold"] != defaults["gold"], "fixture must differ from new-game defaults"
    assert expected["hp"] > 0 and expected["maxHp"] >= expected["hp"]
    key_before = (save_root / "SaveData.key").read_bytes()

    def run(name, **kwargs):
        directory = work / name
        directory.mkdir()
        watch = FileActivityWatch([app, cwd, runtime_temp])
        try:
            with watch:
                console_smoke(game, cwd, env, directory, expected_options=options, **kwargs)
        finally:
            report = save_watch(name, watch)
        watch.assert_clean()
        assert_no_extraction()
        assert {entry.name for entry in save_root.iterdir()} == {"InGameData.sav", "SaveData.key"}
        return report

    report = run("restart-continue", scenario="continue", expected_progress=expected, progress_labels=labels,
                 verify_newgame_save=True)
    restored = invoke("--inspect-save", "continued-quit-inspect")
    assert restored == expected, "Continue/Quit changed saved progress or used new-game fallback"
    assert (save_root / "SaveData.key").read_bytes() == key_before, "dedicated key was regenerated"
    results.append({"name": "restart-continue-restores-changed-progress-and-quit-roundtrip", "passed": True,
                    "newGameDefaults": defaults, "restored": restored, "fileActivityReport": report})
    valid = (save_root / "InGameData.sav").read_bytes()
    corrupt = bytearray(valid)
    corrupt[-1] ^= 1
    (save_root / "InGameData.sav").write_bytes(corrupt)
    report = run("corrupt-save", scenario="corrupt", expected_error=localized["InGameData.Ui.LoadFailed"])
    assert (save_root / "InGameData.sav").read_bytes() == corrupt, "corrupt Continue/Quit overwrote previous bytes"
    assert (save_root / "SaveData.key").read_bytes() == key_before
    results.append({"name": "corrupt-save-keeps-exact-menu-localized-error-and-old-bytes", "passed": True,
                    "ciphertextSha256": hashlib.sha256(corrupt).hexdigest(), "fileActivityReport": report})
    (save_root / "InGameData.sav").write_bytes(valid)
    report = run("quit-save-failure", scenario="quit-failure", verify_newgame_save=True,
                 expected_error=localized["InGameData.Ui.SaveFailed"])
    results.append({"name": "quit-save-failure-localized-nonzero-preserves-ciphertext", "passed": True,
                    "fileActivityReport": report})


def screen_oracle_controls(work):
    """Pure physical-cell fixtures; never launch the supplied game or packer."""
    labels = scene_labels()
    changed_labels = scene_labels(changed=True)

    def physical(text):
        # Independent fixture writer for the current generated Korean/ASCII text.
        return "".join(ch * (2 if 0xAC00 <= ord(ch) <= 0xD7A3 else 1) for ch in text)

    def fixture(scene, expected):
        grid = [[" "] * 132 for _ in range(43)]

        def put(x, y, text):
            grid[y][x:x + len(text)] = text

        def box(x, y, width, height):
            put(x, y, "┌" + "─" * (width - 2) + "┐")
            put(x, y + height - 1, "└" + "─" * (width - 2) + "┘")
            for row in range(y + 1, y + height - 1):
                grid[row][x] = grid[row][x + width - 1] = "│"

        if scene == "MainMenu":
            box(86, 0, 46, 43)
            put(88, 0, physical(expected["MainMenuTitle"]))
            button_rows = (4, 13, 22, 31)
        else:
            for x in (0, 43):
                box(x, 0, 42, 5)
                box(x, 5, 42, 38)
            box(86, 0, 46, 11)
            box(86, 11, 46, 32)
            button_rows = (14, 21, 28, 35)
        for row, label in zip(button_rows, expected[scene]):
            box(89, row, 38, 3)
            put(90, row + 1, physical(label))
        return "\n".join("".join(row) for row in grid)

    def edit(grid, x, y, text):
        rows = grid.splitlines()
        rows[y] = rows[y][:x] + text + rows[y][x + len(text):]
        assert len(rows) == 43 and all(len(row) == 132 for row in rows)
        return "\n".join(rows)

    menu = fixture("MainMenu", labels)
    town = fixture("Town", labels)
    changed_menu = fixture("MainMenu", changed_labels)
    (work / "positive-main-menu.txt").write_text(menu, encoding="utf-8")
    (work / "positive-town.txt").write_text(town, encoding="utf-8")
    (work / "positive-changed-menu.txt").write_text(changed_menu, encoding="utf-8")
    numeric = edit("\n".join([" " * 132] * 43), 3, 2, "1. 2. 3. 4.")
    outside = numeric
    for index, label in enumerate(labels["Town"]):
        outside = edit(outside, 2, 7 + index * 7, physical(label))
    cases = [
        ("positive-main-menu", menu, "MainMenu", labels, False, True),
        ("positive-town", town, "Town", labels, False, True),
        ("positive-changed-menu", changed_menu, "MainMenu", changed_labels, True, True),
        ("menu-single-irrelevant-cell-is-not-town", edit(menu, 85, 1, "!"), "Town", labels, False, False),
        ("numeric-substrings-are-not-town", numeric, "Town", labels, False, False),
        ("numeric-substrings-are-not-menu", numeric, "MainMenu", labels, False, False),
        ("town-labels-outside-option-cells", outside, "Town", labels, False, False),
        ("main-menu-is-not-town", menu, "Town", labels, False, False),
        ("town-is-not-main-menu", town, "MainMenu", labels, False, False),
        ("battlefield-labels-are-not-town", fixture("BattleField", labels), "Town", labels, False, False),
        ("town-shifted-label", edit(edit(town, 90, 15, " " * 36), 91, 15, physical(labels["Town"][0])), "Town", labels, False, False),
        ("town-layout-split-missing", edit(town, 86, 11, " "), "Town", labels, False, False),
        ("town-duplicate-trailing-cell-wrong", edit(town, 94, 15, " "), "Town", labels, False, False),
        ("menu-wrong-full-label", edit(menu, 93, 5, "X"), "MainMenu", labels, False, False),
        ("menu-shifted-label", edit(edit(menu, 90, 5, " " * 36), 91, 5, physical(labels["MainMenu"][0])), "MainMenu", labels, False, False),
        ("baseline-labels-reject-repacked-menu", changed_menu, "MainMenu", labels, True, False),
        ("repacked-labels-reject-baseline-menu", menu, "MainMenu", changed_labels, False, False),
    ]
    for index, y in enumerate((15, 22, 29, 36), 1):
        cases.append((f"town-option-{index}-full-label-missing", edit(town, 93, y, " " * 33),
                      "Town", labels, False, False))
    results = []
    for name, grid, scene, expected, changed, accepted in cases:
        actual = scene_matches(grid, scene, expected, changed, previous_menu=menu)
        results.append({"name": name, "scene": scene, "expectedAccepted": accepted,
                        "actualAccepted": actual, "passed": actual == accepted,
                        "gridSha256": hashlib.sha256(grid.encode("utf-8")).hexdigest()})
    summary = {"passed": all(row["passed"] for row in results), "cases": len(results),
               "results": results, "scope": "pure physical-cell screen oracle; no child processes"}
    if not summary["passed"]:
        raise AssertionError("screen oracle false accepts/rejects")
    return summary


def console_smoke(game, cwd, env, directory, changed=False, expected_logo=None, expected_options=None,
                  verify_newgame_save=False, scenario="newgame", expected_progress=None, progress_labels=None, expected_error=None):
    """사용자 콘솔에 붙지 않고 새로 만든 소유 최소화 콘솔만 제어한다."""
    env = env.copy()
    env.setdefault("TEXT_RPG_SAVE_ROOT", str(directory.resolve() / "save-root"))
    kernel = c.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p, w.DWORD, w.DWORD, w.HANDLE]
    kernel.CreateFileW.restype = w.HANDLE
    kernel.GetConsoleScreenBufferInfo.argtypes = [w.HANDLE, c.POINTER(Info)]
    kernel.SetConsoleScreenBufferSize.argtypes = [w.HANDLE, Coord]
    kernel.SetConsoleWindowInfo.argtypes = [w.HANDLE, w.BOOL, c.POINTER(Rect)]
    kernel.SetCurrentConsoleFontEx.argtypes = [w.HANDLE, w.BOOL, c.POINTER(Font)]
    kernel.ReadConsoleOutputW.argtypes = [w.HANDLE, c.POINTER(Cell), Coord, Coord, c.POINTER(Rect)]
    kernel.WriteConsoleInputW.argtypes = [w.HANDLE, c.POINTER(Record), w.DWORD, c.POINTER(w.DWORD)]
    kernel.CloseHandle.argtypes = [w.HANDLE]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 7
    process = subprocess.Popen([str(game)], cwd=cwd, env=env, startupinfo=startup,
                               creationflags=subprocess.CREATE_NEW_CONSOLE)
    handles = []
    read_only_save = None

    def require(value, label):
        if not value:
            raise OSError(c.get_last_error(), label)

    def key(handle, vk):
        records = (Record * 2)()
        for index, down in enumerate((1, 0)):
            records[index].kind = 1
            records[index].event.key = Key(down, 1, vk, 0, "\0", 0)
        written = w.DWORD()
        require(kernel.WriteConsoleInputW(handle, records, 2, c.byref(written)), "owned console key")
        assert written.value == 2

    def read_grid(name):
        info = Info()
        require(kernel.GetConsoleScreenBufferInfo(handles[0], c.byref(info)), "owned console info")
        cells = (Cell * (info.size.X * info.size.Y))()
        area = Rect(0, 0, info.size.X - 1, info.size.Y - 1)
        require(kernel.ReadConsoleOutputW(handles[0], cells, info.size, Coord(0, 0), c.byref(area)), "owned console cell read")
        assert (area.Left, area.Top, area.Right, area.Bottom) == (0, 0, info.size.X - 1, info.size.Y - 1)
        # CharacterW는 한글의 trailing 셀을 생략한다. 물리 좌표 검사는 CHAR_INFO의 셀 간격을 보존한다.
        value = "\n".join("".join(cells[y * info.size.X + x].character.unicode or " "
                                  for x in range(info.size.X)) for y in range(info.size.Y))
        (directory / (name + ".txt")).write_text(value, encoding="utf-8")
        return value

    def wait_grid(name, ready):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            assert process.poll() is None, "game exited before " + name + " frame"
            value = read_grid(name)
            assert process.poll() is None, "game exited during " + name + " frame read"
            if ready(value):
                return value
            time.sleep(min(0.05, max(0, deadline - time.monotonic())))
        raise AssertionError("timed out waiting for " + name + " frame")

    try:
        kernel.FreeConsole()
        for _ in range(60):
            if kernel.AttachConsole(process.pid):
                break
            assert process.poll() is None, "game exited before owned-console attach"
            time.sleep(0.05)
        else:
            raise OSError(c.get_last_error(), "owned console attach")
        for name in ("CONOUT$", "CONIN$"):
            handle = kernel.CreateFileW(name, 0xC0000000, 3, None, 3, 0, None)
            require(handle != c.c_void_p(-1).value, "owned " + name)
            handles.append(handle)
        font = Font()
        font.cbSize = c.sizeof(Font)
        font.size = Coord(8, 16)
        font.face = "Consolas"
        font.weight = 400
        kernel.SetCurrentConsoleFontEx(handles[0], False, c.byref(font))
        require(kernel.SetConsoleWindowInfo(handles[0], True, c.byref(Rect(0, 0, 79, 24))), "shrink owned viewport")
        require(kernel.SetConsoleScreenBufferSize(handles[0], Coord(132, 43)), "owned buffer")
        require(kernel.SetConsoleWindowInfo(handles[0], True, c.byref(Rect(0, 0, 131, 42))), "owned viewport")
        expected_lines = None
        if expected_logo is not None:
            expected_lines = expected_logo.decode("utf-8-sig").splitlines()
            assert len(expected_lines) == 41 and all(len(line) == 83 for line in expected_lines)
        labels = expected_options or scene_labels(changed=changed)
        menu = wait_grid("main-menu", lambda value:
                         scene_matches(value, "MainMenu", labels, changed)
                         and (expected_lines is None
                              or [line[1:84] for line in value.splitlines()[1:42]] == expected_lines))
        assert all(f"{n}." in menu for n in range(1, 5)), "four main-menu choices missing"
        assert ("DAT_RELOAD_OK" in menu) == changed, "runtime value not read from adjacent dat"
        assert scene_matches(menu, "MainMenu", labels, changed), "MainMenu exact option cells/layout/title"
        if expected_logo is not None:
            actual_lines = menu.splitlines()
            assert [line[1:84] for line in actual_lines[1:42]] == expected_lines, "packaged logo pixels differ"
        if scenario == "menu-only":
            # Scene-only 레거시 데이터에는 새 게임 진행 정의가 없다. 메뉴의 선택적 아트 계약만 검사한다.
            key(handles[1], 0x1B)
            assert process.wait(timeout=10) == 0
            return
        if scenario == "corrupt":
            key(handles[1], 0x32)
            failure = wait_grid("corrupt-save-menu", lambda value: scene_matches(value, "MainMenu", labels)
                                and value.split("\n")[40][88:130] == physical_text(expected_error, 42))
            assert scene_matches(failure, "MainMenu", labels), "corrupt Continue replaced MainMenu"
            key(handles[1], 0x1B)
            assert process.wait(timeout=10) == 0
            return
        key(handles[1], 0x32 if scenario == "continue" else 0x0D)
        town = wait_grid("town", lambda value:
                         scene_matches(value, "Town", labels, previous_menu=menu))
        assert town != menu and all(f"{n}." in town for n in range(1, 5)), "Town transition/four choices"
        assert scene_matches(town, "Town", labels), "Town exact four option cells/layout"
        if expected_progress is not None:
            assert progress_matches(town, expected_progress, progress_labels), "restored progress readout differs from production fixture"
        before_quit = None
        if verify_newgame_save:
            save_root = Path(env["TEXT_RPG_SAVE_ROOT"])
            assert (save_root / "InGameData.sav").is_file(), "new-game autosave missing after exact Town frame"
            assert (save_root / "SaveData.key").is_file(), "dedicated save key missing"
            before_quit = (save_root / "InGameData.sav").read_bytes()
        if scenario == "quit-failure":
            assert before_quit is not None, "quit-failure requires existing new-game autosave"
            path = save_root / "InGameData.sav"
            read_only_save = (path, path.stat().st_mode)
            path.chmod(0o444)
        key(handles[1], 0x1B)
        exit_code = process.wait(timeout=10)
        if scenario == "quit-failure":
            assert exit_code != 0, "failed Quit autosave silently reported success"
            failure = read_grid("quit-save-failure")
            assert failure.split("\n")[40][88:130] == physical_text(expected_error, 42), "Quit save failure is not localized/visible"
            assert (save_root / "InGameData.sav").read_bytes() == before_quit, "failed Quit overwrote old ciphertext"
        else:
            assert exit_code == 0
            if before_quit is not None:
                assert (save_root / "InGameData.sav").read_bytes() != before_quit, "normal Quit did not autosave"
    finally:
        try:
            try:
                if process.poll() is None and len(handles) == 2:
                    key(handles[1], 0x1B)
                    process.wait(timeout=5)
            finally:
                try:
                    if process.poll() is None:
                        # 이 스크립트가 새로 생성한 테스트 프로세스만 회수한다.
                        process.terminate()
                finally:
                    process.wait(timeout=5)
        finally:
            try:
                for handle in handles:
                    kernel.CloseHandle(handle)
            finally:
                kernel.FreeConsole()
                if read_only_save is not None:
                    read_only_save[0].chmod(read_only_save[1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", required=True, type=Path)
    parser.add_argument("--packer", required=True, type=Path)
    parser.add_argument("--scratch-root", required=True, type=Path)
    parser.add_argument("--only", choices=["missing-package", "screen-oracle", "ingamedata-newgame", "ingamedata"])
    parser.add_argument("--progress-fixture", type=Path, help="existing SceneUiRuntime native test executable")
    args = parser.parse_args()
    if args.only == "ingamedata" and not (args.progress_fixture and args.progress_fixture.is_file()):
        # 패킹/게임 실행과 작업 디렉터리 생성보다 먼저 검사하며 -O에서도 생략하지 않는다.
        parser.error("--progress-fixture native executable required for --only ingamedata")
    assert args.game.is_file() and args.packer.is_file()
    work = args.scratch_root.resolve()
    work.mkdir(parents=True, exist_ok=False)
    if args.only == "screen-oracle":
        summary = screen_oracle_controls(work)
        publish_report(work / "results.json", summary,
                       {key: value for key, value in summary.items() if key != "results"})
        return
    results = []
    progress_scope = args.only in ("ingamedata-newgame", "ingamedata")
    if progress_scope:
        (work / "results.json").write_text(json.dumps({
            "passed": False, "status": "incomplete", "cases": 0, "results": results,
        }, indent=2), encoding="utf-8")
    try:
        summary, output = run_application(args, work, results)
    except BaseException as exception:
        if progress_scope:
            # 예외 문자열에는 subprocess 환경의 비밀키가 들어갈 수 있다. 타입만 기록한다.
            write_report(work / "results.json", {
                "passed": False, "status": "failed", "cases": len(results), "results": results,
                "error": {"type": type(exception).__name__},
            })
        raise
    publish_report(work / "results.json", summary, output)


def run_application(args, work, results):
    unrelated = work / "unrelated-cwd"
    unrelated.mkdir()
    (unrelated / "GameData.dat").write_bytes(b"a cwd package must never be used")
    app = work / "app"
    app.mkdir()
    game = app / "TextRPG.exe"
    shutil.copy2(args.game, game)
    identity = hashlib.sha256(game.read_bytes()).hexdigest()
    env = os.environ.copy()
    save_root = work / "save-root"
    env["TEXT_RPG_SAVE_ROOT"] = str(save_root)
    # 배포본은 exe+dat뿐이며 loose root 환경변수가 설정돼도 사용하지 않는다.
    env.pop("TEXTRPG_GAMEDATA_ROOT", None)
    runtime_temp = work / "runtime-temp"
    runtime_temp.mkdir()
    for variable in ("TMP", "TEMP", "TMPDIR"):
        env[variable] = str(runtime_temp)

    def assert_no_extraction():
        assert {p.name for p in app.iterdir()} <= {"TextRPG.exe", "GameData.dat"}
        assert all(p.is_file() for p in app.iterdir()), "runtime created an extraction directory"
        assert list(runtime_temp.iterdir()) == [], "runtime extracted temporary content"
        assert not (work / "GameData").exists() and not (work / "nonexistent-loose-root").exists()
        assert {p.name for p in unrelated.iterdir()} == {"GameData.dat"}, "runtime extracted content into cwd"
        assert (unrelated / "GameData.dat").read_bytes() == b"a cwd package must never be used"

    def save_watch(name, watch):
        # 보고서는 감시하는 app/cwd/tmp 밖에 남긴다. 실패 경로도 원시 알림을 보존한다.
        path = work / (name + "-file-activity.json")
        path.write_text(json.dumps(watch.report(), indent=2), encoding="utf-8")
        return str(path)

    def positive_console(name, directory, changed=False, expected_logo=None, expected_options=None,
                         verify_newgame_save=False, scenario="newgame"):
        watch = FileActivityWatch([app, unrelated, runtime_temp])
        try:
            with watch:
                # console_smoke의 finally가 자식을 완전히 회수한 뒤 health/drain한다.
                console_smoke(game, unrelated, env, directory, changed, expected_logo, expected_options,
                              verify_newgame_save, scenario=scenario)
        finally:
            report = save_watch(name, watch)
        watch.assert_clean()
        assert_no_extraction()
        return report

    def reject(name, expected, selected_env=None):
        watch = FileActivityWatch([app, unrelated, runtime_temp])
        try:
            with watch:
                # subprocess.run은 timeout 시에도 자식을 kill/wait한 뒤 예외를 던진다.
                result = subprocess.run([str(game)], cwd=unrelated, env=selected_env or env,
                                        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
        finally:
            report = save_watch(name, watch)
        watch.assert_clean()
        (work / (name + ".log")).write_bytes(result.stdout)
        text = result.stdout.decode("utf-8", errors="replace")
        assert result.returncode != 0, name + " unexpectedly entered game"
        assert any(term.lower() in text.lower() for term in expected), (name, text)
        assert_no_extraction()
        results.append({"name": name, "passed": True, "exitCode": result.returncode, "fileActivityReport": report})

    reject("missing-package", ["package", "GameData.dat"])
    if args.only == "missing-package":
        return results, {"passed": True, "cases": len(results), "directory": str(work)}
    source = work / "Client"
    shutil.copytree(ROOT / "GameData/Data/Client", source)
    dat = app / "GameData.dat"

    logo_path = "Logo/Logo_Text_Rpg.txt"
    logo_bytes = (ROOT / "GameData/Asset" / logo_path).read_bytes()

    def pack(name, change=None, omit=None, asset_change=None, asset_omit=None, capture_options=False):
        fixture = work / (name + "-client")
        shutil.copytree(source, fixture)
        if change:
            file_name, edit = change
            path = fixture / file_name
            path.write_bytes(edit(path.read_bytes()))
        for omitted in ([omit] if isinstance(omit, str) else (omit or ())):
            (fixture / omitted).unlink()
        asset_fixture = work / (name + "-assets")
        shutil.copytree(ROOT / "GameData/Asset", asset_fixture)
        if asset_change:
            file_name, edit = asset_change
            path = asset_fixture / file_name
            path.write_bytes(edit(path.read_bytes()))
        if asset_omit:
            (asset_fixture / asset_omit).unlink()
        expected_options = scene_labels(fixture) if capture_options else None
        if capture_options:
            (work / (name + "-expected-options.json")).write_text(
                json.dumps(expected_options, ensure_ascii=False, indent=2), encoding="utf-8")
        result = subprocess.run([str(args.packer), "pack", str(fixture), str(dat), "--asset-root", str(asset_fixture)], env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        (work / (name + "-pack.log")).write_bytes(result.stdout)
        assert result.returncode == 0, name + " fixture pack failed"
        shutil.rmtree(asset_fixture)
        shutil.rmtree(fixture)
        assert_no_extraction()
        return expected_options

    baseline_options = pack("baseline", capture_options=True)
    good = dat.read_bytes()
    if args.only in ("ingamedata-newgame", "ingamedata"):
        directory = work / "ingamedata-newgame"
        directory.mkdir()
        report = positive_console("ingamedata-newgame", directory, expected_logo=logo_bytes,
                                  expected_options=baseline_options, verify_newgame_save=True)
        results.append({"name": "ingamedata-newgame-autosave", "passed": True, "fileActivityReport": report})
        if args.only == "ingamedata":
            progress_acceptance(args.progress_fixture, game, dat, unrelated, env, work, baseline_options,
                                app, runtime_temp, save_root, results, save_watch, assert_no_extraction)
        required = ["missing-package", "ingamedata-newgame-autosave"]
        if args.only == "ingamedata":
            required += ["restart-continue-restores-changed-progress-and-quit-roundtrip",
                         "corrupt-save-keeps-exact-menu-localized-error-and-old-bytes",
                         "quit-save-failure-localized-nonzero-preserves-ciphertext"]
        if ([row.get("name") for row in results] != required
                or not all(row.get("passed") is True for row in results)):
            raise RuntimeError("required InGameData acceptance cases incomplete or failed")
        # 전체 요청 범위가 반환한 뒤에만 성공을 게시한다. NewGame 단독 CLI는 작은 범위를 유지한다.
        summary = {"passed": True, "status": "complete", "cases": len(results),
                   "results": results, "exeSha256": identity}
        return summary, summary
    expected_options = baseline_options
    for name, changed in (("baseline-console", False), ("changed-console", True)):
        if changed:
            env["TEXTRPG_GAMEDATA_ROOT"] = str(work / "nonexistent-loose-root")
            def replace_localization(raw):
                before = '"KoKR": "새 게임"'.encode("utf-8")
                assert raw.count(before) == 1
                return raw.replace(before, b'"KoKR": "DAT_RELOAD_OK"')
            expected_options = pack("changed", ("Localization.yaml", replace_localization), capture_options=True)
        directory = work / name
        directory.mkdir()
        report = positive_console(name, directory, changed, logo_bytes, expected_options)
        assert_no_extraction()
        results.append({"name": name, "passed": True, "fileActivityReport": report})
    assert hashlib.sha256(game.read_bytes()).hexdigest() == identity, "EXE changed during data-only repack"
    # 데이터 변경과 별개로 자산만 바꾼 패키지를 같은 EXE가 즉시 표시한다.
    changed_logo = (b"#" if logo_bytes[:1] != b"#" else b"@") + logo_bytes[1:]
    expected_options = pack("asset-only", asset_change=(logo_path, lambda _: changed_logo), capture_options=True)
    directory = work / "asset-only-console"
    directory.mkdir()
    report = positive_console("asset-only-console", directory, expected_logo=changed_logo, expected_options=expected_options)
    assert_no_extraction()
    assert hashlib.sha256(game.read_bytes()).hexdigest() == identity, "EXE changed during asset-only repack"
    results.append({"name": "asset-only-repack-same-exe", "passed": True, "fileActivityReport": report})
    pack("missing-referenced-asset", asset_omit=logo_path)
    reject("missing-referenced-asset", ["TextArt", "AssetPath", "missing"])
    pack("corrupt-referenced-asset", asset_change=(logo_path, lambda _: b"\xff"))
    reject("corrupt-referenced-asset", ["TextArt", "UTF-8"])
    # 실패 후보 뒤에도 동일 EXE와 원래 dat 조합으로 복구한다.
    dat.write_bytes(good)
    directory = work / "asset-failure-recovery-console"
    directory.mkdir()
    report = positive_console("asset-failure-recovery-console", directory, expected_logo=logo_bytes,
                              expected_options=baseline_options)
    assert_no_extraction()
    results.append({"name": "asset-failure-recovery", "passed": True, "fileActivityReport": report})
    dat.write_bytes(good[:12])
    reject("truncated-package", ["package", "header", "truncated"])
    tampered = bytearray(good)
    tampered[-1] ^= 1
    dat.write_bytes(tampered)
    reject("tampered-package", ["auth", "decrypt", "package"])
    version = bytearray(good)
    assert version[:4] == b"TGDP"
    version[4:8] = (99).to_bytes(4, "little")
    dat.write_bytes(version)
    reject("unsupported-package-version", ["version"])
    dat.write_bytes(good)
    wrong = env.copy()
    wrong["TEXT_RPG_GAMEDATA_KEY"] = secrets.token_hex(32)
    reject("wrong-key", ["auth", "decrypt", "package"], wrong)
    empty = env.copy()
    empty["TEXT_RPG_GAMEDATA_KEY"] = ""
    reject("empty-key-no-development-fallback", ["key", "TEXT_RPG"], empty)
    absent = env.copy()
    absent.pop("TEXT_RPG_GAMEDATA_KEY", None)
    absent["LOCALAPPDATA"] = str(work / "unprovisioned-user")
    reject("unprovisioned-key-no-runtime-creation", ["key", "development"], absent)
    assert not (work / "unprovisioned-user/TextRPG/Development/GameData.key").exists()
    fixtures = [
        ("unsupported-data-version", "Scene.yaml", lambda raw: raw.replace(b"# TextRPG.GameData 1\n", b"# TextRPG.GameData 99\n"), ["version"]),
        ("required-null", "Scene.yaml", lambda raw: raw.replace(b'"IsStartupScene": true', b'"IsStartupScene": null', 1), ["IsStartupScene", "bool", "null"]),
        ("unknown-scene-field", "Scene.yaml", lambda raw: raw.replace(b'  "IsStartupScene": true', b'  "PlayerArtId": "illegal"\n  "IsStartupScene": true', 1), ["PlayerArtId", "field"]),
        ("invalid-enum", "Scene.yaml", lambda raw: raw.replace(b'"SceneType": "MainMenu"', b'"SceneType": "NotRegistered"', 1), ["SceneType", "enum"]),
        ("broken-reference", "SceneOption.yaml", lambda raw: raw.replace(b'"TargetSceneId": "Town"', b'"TargetSceneId": "NotRegistered"', 1), ["TargetSceneId", "reference", "target"]),
        ("invalid-utf8", "Localization.yaml", lambda raw: raw + b"\xff", ["UTF", "encoding", "YAML"]),
        ("duplicate-primary-key", "Scene.yaml", lambda raw: raw + b'"SceneAnotherRoot":\n  "SceneId": "MainMenu"\n  "SceneType": "MainMenu"\n  "IsStartupScene": false\n', ["duplicate", "SceneId", "key", "root"]),
    ]
    for name, file_name, edit, expected in fixtures:
        pack(name, (file_name, edit))
        reject(name, expected)
    pack("missing-required-table", omit="SceneTypeConfig.yaml")
    reject("missing-required-table", ["SceneTypeConfig", "missing", "required"])
    has_progress_catalog = (source / "CharacterDefinition.yaml").is_file()
    progress_tables = ("CharacterDefinition.yaml", "LevelDefinition.yaml", "NewGameProfile.yaml",
                       "ItemDefinition.yaml", "AchievementDefinition.yaml")
    if has_progress_catalog:
        # 진행 정의의 필수 ArtId만 끊은 전체 콘텐츠는 더 이상 유효한 optional fixture가 아니다.
        pack("required-character-art-catalog", omit="TextArt.yaml")
        reject("required-character-art-catalog", ["ArtId reference is missing"])
    omitted = ("TextArt.yaml", *progress_tables) if has_progress_catalog else "TextArt.yaml"
    expected_options = pack("optional-textart-absent", omit=omitted, capture_options=True)
    directory = work / "optional-textart-console"
    directory.mkdir()
    report = positive_console("optional-textart-console", directory, expected_options=expected_options,
                              scenario="menu-only" if has_progress_catalog else "newgame")
    assert_no_extraction()
    results.append({"name": "optional-textart-absent", "passed": True, "fileActivityReport": report,
                    "scope": "legacy Scene-only menu" if has_progress_catalog else "legacy Scene transition"})
    assert hashlib.sha256(game.read_bytes()).hexdigest() == identity
    summary = {"passed": all(x["passed"] for x in results), "cases": len(results), "exeSha256": identity,
               "sourceExecutable": str(args.game), "directory": str(work), "results": results,
               "scope": "actual EXE, isolated minimized console; text-buffer/input behavior, not manual visual certification"}
    return summary, {k: v for k, v in summary.items() if k != "results"}


def write_report(path, report):
    """부분 쓰기는 canonical 성공 보고서를 만들지 못한다."""
    pending = path.with_name("." + path.name + ".pending")
    try:
        pending.write_text(json.dumps(report, indent=2), encoding="utf-8")
        pending.replace(path)
    except BaseException:
        try:
            pending.unlink(missing_ok=True)
        except OSError:
            # 실패는 계속 전파한다. 임시 파일 정리 실패를 성공으로 바꾸지 않는다.
            pass
        raise


def publish_report(path, report, output=None):
    """기존 출력 drain -> 최종 출력/drain -> 원자적 게시가 유일한 성공 경계다."""
    try:
        flush_cli_streams()
        try:
            if sys.stdout is None:
                raise OSError("stdout unavailable")
            print(json.dumps(report if output is None else output))
        except BaseException:
            sys.stdout = None
            raise
        flush_cli_streams()
        write_report(path, report)
    except BaseException as exception:
        failed = dict(report) if isinstance(report, dict) else {"results": report}
        failed.update(passed=False, status="failed", error={"type": type(exception).__name__})
        write_report(path, failed)
        raise


def flush_cli_streams():
    for name in ("stdout", "stderr"):
        stream = getattr(sys, name)
        if stream is not None:
            try:
                stream.flush()
            except BaseException:
                # 인터프리터 종료 flush가 원래 예외/chain을 다시 출력하지 못하게 한다.
                setattr(sys, name, None)
                raise


def cli(main_function=None, failure_code="APPLICATION_FAILED"):
    """import용 main은 예외를 보존하고 실제 CLI만 타입/고정 코드로 종료한다."""
    try:
        # main의 publish_report 뒤에는 성공을 뒤집을 수 있는 출력/flush가 없다.
        result = (main_function or main)()
        return 0 if result is None else int(result)
    except SystemExit as exception:
        # argparse의 help/usage 종료 코드와 사전 검증 exit 2를 보존한다.
        if exception.code is None or isinstance(exception.code, int):
            return 0 if exception.code is None else exception.code
        error_type = type(exception).__name__
    except BaseException as exception:
        error_type = type(exception).__name__
    # main의 finally와 실패 산출물 기록이 끝난 뒤만 이 경계에 도달한다.
    # 실패한 sink만 폐기한다. 출력 실패를 성공으로 바꾸거나 예외 문자열을 쓰지 않는다.
    for name in ("stdout", "stderr"):
        stream = getattr(sys, name)
        if stream is not None:
            try:
                stream.flush()
            except BaseException:
                setattr(sys, name, None)
    if sys.stderr is not None:
        try:
            sys.stderr.write(f"{failure_code} type={error_type}\n")
            sys.stderr.flush()
        except BaseException:
            sys.stderr = None
    return 1


if __name__ == "__main__":
    raise SystemExit(cli())
