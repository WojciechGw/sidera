"""Render all three required views from the host test's actual XRAM snapshots."""
from pathlib import Path
import subprocess
import shutil
import sys

snapshot=Path(sys.argv[1])
output=Path(sys.argv[2])
output.mkdir(parents=True,exist_ok=True)
renderer=Path(__file__).with_name('preview.py')
for view in ('A','B','C'):
    subprocess.run([sys.executable,str(renderer),str(snapshot)+f'.test{view}',
                    str(output/f'sidera-test{view}.png')],check=True)
    shutil.copyfile(str(snapshot)+f'.test{view}.cfg',output/f'sidera-test{view}.cfg')

for view in ('A','B','C'):
    subprocess.run([sys.executable,str(renderer),str(snapshot)+f'.solar{view}',
                    str(output/f'sidera-solar{view}.png'),'--solar'],check=True)

subprocess.run([sys.executable,str(renderer),str(snapshot)+'.cities-panel',
                str(output/'sidera-cities.png')],check=True)
