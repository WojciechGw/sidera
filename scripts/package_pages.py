"""Stage only SIDERA's player, preview and matching Git sources for Pages.

Run after the Release build and browser smoke test, from a clean CI checkout.
The output directory must be empty to prevent publishing stale or unrelated files.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    repo = Path(__file__).resolve().parents[1]
    if output.exists() and any(output.iterdir()):
        parser.error('output directory must be empty')
    preview = build / 'test/sidera_web/solar.png'
    if not preview.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'):
        parser.error('browser preview is not a PNG')
    with zipfile.ZipFile(build / 'web/sidera.zip') as package:
        names = {'index.html', 'rp6502.js', 'rp6502.wasm', 'sidera.rp6502'}
        if set(package.namelist()) != names:
            parser.error('unexpected WEB package contents')
        files = {name: package.read(name) for name in names}
    if files['sidera.rp6502'] != (build / 'sidera/sidera.rp6502').read_bytes():
        parser.error('WEB package does not contain the current ROM')
    html = files['index.html'].decode('utf-8')
    marker = 'const config = {'
    if html.count(marker) != 1:
        parser.error('SDK page has no unique configuration block')
    html = html.replace(marker, marker + '\n'
        '    footer: \'SIDERA &middot; <a href="source.zip">Source (GPL-3.0-only)</a>\',', 1)
    player = output / 'sidera'
    player.mkdir(parents=True)
    for name, data in files.items():
        (player / name).write_bytes(data)
    (player / 'index.html').write_text(html, encoding='utf-8')
    shutil.copyfile(preview, player / 'screenshot.png')
    subprocess.run(['git', 'archive', '--format=zip',
        '--output=' + str(player / 'source.zip'), 'HEAD'], cwd=repo, check=True)
    (output / 'index.html').write_text('''<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta http-equiv="refresh" content="0;url=sidera/">
<title>SIDERA</title>
<a href="sidera/">Open SIDERA</a>
</html>
''', encoding='utf-8')
    print(f'Pages ready: {output} (player at sidera/, sources and preview included)')


if __name__ == '__main__':
    main()
