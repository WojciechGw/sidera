"""Translate host paths when the SDK selects a Windows emulator under WSL.

ROM arguments after -- are machine paths and must remain unchanged.
Used only as the process boundary; the SDK still reads Linux .sym paths.
"""
import subprocess
import sys

emulator, *args = sys.argv[1:]
for i, value in enumerate(args):
    if value == '--':
        break
    if value.startswith('/'):
        args[i] = subprocess.check_output(['wslpath', '-w', value], text=True).strip()
raise SystemExit(subprocess.call([emulator, *args]))
