"""Additional sprites using the existing SIDERA sphere/palette generator."""
from pathlib import Path
import argparse
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from assets import planet_sprite

earth, earth_pal = planet_sprite(6)
neptune, neptune_pal = planet_sprite(7)
app = (Path(__file__).resolve().parents[1] / 'src')
p=argparse.ArgumentParser()
p.add_argument('--output',type=Path,default=app.parent/'build/generated/rom/sidera-solar.dat')
output=p.parse_args().output
output.parent.mkdir(parents=True,exist_ok=True)
output.write_bytes(earth + neptune + earth_pal + neptune_pal)
