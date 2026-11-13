"""Instrument only portal accesses; compile the unchanged drawing algorithms."""
import re
import sys
from pathlib import Path
source=((Path(__file__).resolve().parents[1] / 'src')/'graphics.c').read_text()
source=re.sub(r'RIA\.rw0\s*=\s*([^;]+);',r'xr_write0(\1);',source)
source=source.replace('RIA.rw0','xr_read0()').replace('RIA.rw1','xr_read1()')
Path(sys.argv[1]).write_text(source)

# Exercise the actual keyboard editor without the OS event loop.
main=((Path(__file__).resolve().parents[1] / 'src')/'main.c').read_text()
editor='unsigned char editing;\n#include "editor.c"\n'
start=main.index("case 'p':case 'P':")
case=main[start:main.index('break;',start)+6]
editor+='\nstatic int planet_key(unsigned k) { unsigned char refresh=0,scene=0; switch(k) {'+case+'} return refresh&&scene; }\n'
Path(sys.argv[1]).with_name('editor_host.inc').write_text(editor)
