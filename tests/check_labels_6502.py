"""Check native integer arithmetic and crowded label placements on 65C02."""
from pathlib import Path
import subprocess
import sys

compiler, simulator, directory = sys.argv[1:]
work = Path(directory).resolve()
work.mkdir(parents=True, exist_ok=True)
app = (Path(__file__).resolve().parents[1] / 'src')
subprocess.run([compiler, '-t', 'sim65c02', '-O', '-D', 'LABELS_6502_FIXED',
                '-I', str(app), '-o', str(work / 'check'),
                str(app.parent / 'tests/solar_labels_test.c'), str(app / 'solar_labels.c'),
                str(app / 'solar_math.c'), str(app / 'astro.c')], check=True)
subprocess.run([simulator, '-c', str(work / 'check')], check=True, timeout=60)
