"""Check interleaved astronomy against the uncached variant A on host/65C02."""
import hashlib
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
if sys.argv[1] == '--host':
    command = [sys.argv[2]]
else:
    compiler, simulator, directory = sys.argv[1:4]
    work = Path(directory).resolve()
    work.mkdir(parents=True, exist_ok=True)
    binary = work / 'cache'
    subprocess.run([compiler, '-t', 'sim65c02', '-O', '-I', str(root / 'src'),
                    '-o', str(binary), str(root / 'tests/astro_cache.c'),
                    str(root / 'src/astro.c')], check=True)
    command = [simulator, str(binary)]
output = subprocess.check_output(command, text=True)
canonical = ('\n'.join(output.splitlines()) + '\n').encode('ascii')
expected = (root / 'tests/astro-cache.sha256').read_text().strip()
actual = hashlib.sha256(canonical).hexdigest()
assert len(output.splitlines()) == 72
assert actual == expected, f'Cached astronomy differs from variant A: {actual} != {expected}'
print('PASS: 72 interleaved date/observer cases match uncached variant A exactly')
