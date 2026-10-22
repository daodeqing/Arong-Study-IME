"""Small standalone Win32 edit host for IME acceptance; stores only its test text."""
from __future__ import annotations

import argparse
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import time

parser = argparse.ArgumentParser()
parser.add_argument("--report", type=Path, required=True)
args = parser.parse_args()
args.report.parent.mkdir(parents=True, exist_ok=True)
user = c.WinDLL("user32", use_last_error=True)
kernel = c.WinDLL("kernel32", use_last_error=True)
ole = c.WinDLL("ole32", use_last_error=True)
LRESULT = c.c_ssize_t
WNDPROC = c.WINFUNCTYPE(LRESULT, w.HWND, w.UINT, w.WPARAM, w.LPARAM)

class WNDCLASS(c.Structure):
    _fields_ = [("style", w.UINT), ("lpfnWndProc", WNDPROC),
                ("cbClsExtra", c.c_int), ("cbWndExtra", c.c_int),
                ("hInstance", w.HINSTANCE), ("hIcon", w.HICON),
                ("hCursor", w.HANDLE), ("hbrBackground", w.HBRUSH),
                ("lpszMenuName", w.LPCWSTR), ("lpszClassName", w.LPCWSTR)]

user.DefWindowProcW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
user.DefWindowProcW.restype = LRESULT
user.CreateWindowExW.argtypes = [w.DWORD, w.LPCWSTR, w.LPCWSTR, w.DWORD,
                               c.c_int, c.c_int, c.c_int, c.c_int,
                               w.HWND, w.HMENU, w.HINSTANCE, w.LPVOID]
user.CreateWindowExW.restype = w.HWND
user.GetWindowTextLengthW.argtypes = [w.HWND]
user.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
user.SetFocus.argtypes = [w.HWND]
user.MoveWindow.argtypes = [w.HWND, c.c_int, c.c_int, c.c_int, c.c_int, w.BOOL]
user.SetTimer.argtypes = [w.HWND, c.c_size_t, w.UINT, w.LPVOID]
user.KillTimer.argtypes = [w.HWND, c.c_size_t]
user.DestroyWindow.argtypes = [w.HWND]
kernel.GetModuleHandleW.argtypes = [w.LPCWSTR]
kernel.GetModuleHandleW.restype = w.HMODULE
edit = None
started = time.monotonic()
snapshots = []

def snapshot(final=False):
    if edit:
        size = user.GetWindowTextLengthW(edit)
        text = c.create_unicode_buffer(size + 1)
        user.GetWindowTextW(edit, text, size + 1)
        if not snapshots or snapshots[-1]["text"] != text.value:
            snapshots.append({"elapsed_ms": round((time.monotonic() - started) * 1000, 1),
                              "text": text.value})
    args.report.write_text(json.dumps({"standalone_win32_edit": True,
                                      "closed": final, "snapshots": snapshots},
                                     ensure_ascii=False, indent=2) + "\n",
                           encoding="utf-8")

@WNDPROC
def window_proc(hwnd, msg, wp, lp):
    if msg == 5 and edit:
        user.MoveWindow(edit, 12, 12, max(50, (lp & 65535) - 24),
                        max(50, ((lp >> 16) & 65535) - 24), True)
        return 0
    if msg == 7 and edit:
        user.SetFocus(edit)
        return 0
    if msg == 0x113:
        snapshot()
        return 0
    if msg == 2:
        snapshot(True)
        user.KillTimer(hwnd, 1)
        user.PostQuitMessage(0)
        return 0
    return user.DefWindowProcW(hwnd, msg, wp, lp)

ole.OleInitialize(None)
instance = kernel.GetModuleHandleW(None)
wc = WNDCLASS()
wc.lpfnWndProc = window_proc
wc.hInstance = instance
wc.hbrBackground = 6
wc.lpszClassName = "ArongStandaloneImeAcceptance"
assert user.RegisterClassW(c.byref(wc))
hwnd = user.CreateWindowExW(0, wc.lpszClassName,
                           "Arong IME isolated input test - close when finished",
                           0x10CF0000, 120, 120, 760, 380, None, None, instance, None)
assert hwnd
edit = user.CreateWindowExW(0x200, "EDIT", "", 0x50B010C4,
                           12, 12, 720, 310, hwnd, None, instance, None)
assert edit
user.SetTimer(hwnd, 1, 100, None)
user.SetFocus(edit)
snapshot()
msg = w.MSG()
while user.GetMessageW(c.byref(msg), None, 0, 0) > 0:
    user.TranslateMessage(c.byref(msg))
    user.DispatchMessageW(c.byref(msg))
ole.OleUninitialize()
