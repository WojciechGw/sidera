"""Rebuild the checked-in assets offline, into a separate output directory."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, default=ROOT/'build/generated')
p.add_argument('--check', action='store_true', help='compare against shipped assets')
a = p.parse_args()
out = a.output.resolve()
def source_state():
    return {str(p.relative_to(ROOT/'src')): (p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest())
            for p in (ROOT/'src').rglob('*') if p.is_file()}
before=source_state() if a.check else None
commands = [
    ('constellations.py', '--output', out/'tables/constellations.h'),
    ('assets.py', '--output', out/'rom/sidera.dat', '--tables'),
    ('solar_assets.py', '--output', out/'rom/sidera-solar.dat'),
    ('cities.py', '--output', out/'rom/sidera.cty'),
    ('world_map.py', '--output', out/'rom/sidera.map'),
    ('find_catalogue.py', '--output', out/'rom/sidera.find'),
    ('timezones.py', '--output-dir', out/'rom'),
]
for script, *args in commands:
    subprocess.run([sys.executable, '-B', str(ROOT/'scripts'/script), *map(str,args)], check=True)
if a.check:
    if source_state()!=before:
        raise SystemExit('A generator changed the source tree')
    for folder in ('rom', 'tables'):
        for generated in sorted((out/folder).iterdir()):
            shipped = ROOT/'src'/folder/generated.name
            if generated.suffix in ('.h', '.txt'):
                equal = generated.read_text(encoding='utf-8') == shipped.read_text(encoding='utf-8')
            else:
                equal = generated.read_bytes() == shipped.read_bytes()
            if not equal:
                raise SystemExit(f'Generated asset differs: {folder}/{generated.name}')
    print('PASS: every regenerated asset matches the shipped version')
