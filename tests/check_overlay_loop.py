"""Exercise real overlay-enabled startup/UI/cleanup with host IO and ROM assets.

6502 instructions are executed separately by check_overlay_6502.py; this test
checks the application's call order, cache reuse, scratch and error cleanup.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

exe, payload_dir = sys.argv[1:]
app = (Path(__file__).resolve().parents[1] / 'src')
with tempfile.TemporaryDirectory(prefix='sidera-loop-') as directory:
    root = Path(directory)
    for path in (app / 'rom').iterdir():
        shutil.copyfile(path, root / path.name)
    assets = {}
    for name in ('init', 'cities', 'edit', 'cover', 'solar'):
        filename = f'sidera-{name}.ovl'
        data = bytearray((Path(payload_dir) / filename).read_bytes())
        # The native host executable is not stamped by the 6502 packer.
        data[12:20] = bytes(8)
        assets[filename] = bytes(data)
        (root / filename).write_bytes(data)
    command = [exe, str(root), str(root / 'settings.cfg')]
    subprocess.run(command, check=True)
    subprocess.run(command + ["solar-loop"], check=True)
    subprocess.run(command + ["solar-live"], check=True)
    for name, data in assets.items():
        (root / name).write_bytes(data[:-1])
        subprocess.run(command + [name], check=True)
        (root / name).write_bytes(data)
    extra = root / 'sidera-solar.dat'
    extra.write_bytes(extra.read_bytes()[:-1])
    subprocess.run(command + [extra.name], check=True)
print('PASS: overlay-enabled application, cache/scratch and all five failure paths and solar assets')
