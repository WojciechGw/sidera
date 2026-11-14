"""Optional browser smoke test: python tests/web_smoke.py build/cc65/release.

Requires Playwright and its Chromium browser; uses an isolated browser context.
Screenshots are review artifacts, not reference-image comparisons.
"""
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import re
import sys
from threading import Thread
from time import monotonic

from playwright.sync_api import sync_playwright


READ_CONFIG = """() => new Promise((resolve, reject) => {
    const request = indexedDB.open('wojciechgw-sidera');
    request.onerror = () => reject(request.error);
    request.onsuccess = () => {
        const db = request.result;
        if (!db.objectStoreNames.contains('FILE_DATA')) {
            db.close(); resolve(''); return;
        }
        const tx = db.transaction('FILE_DATA');
        const file = tx.objectStore('FILE_DATA').get('sidera.cfg');
        file.onsuccess = () => resolve(file.result
            ? new TextDecoder().decode(file.result.contents) : '');
        file.onerror = () => reject(file.error);
        tx.oncomplete = () => db.close();
    };
})"""


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *_args):
        pass


def main():
    build = Path(sys.argv[1]).resolve()
    output = build / 'test/sidera_web'
    output.mkdir(parents=True, exist_ok=True)
    for name in ('result.json', 'save-failed.png'):
        (output / name).unlink(missing_ok=True)
    server = ThreadingHTTPServer(('127.0.0.1', 0),
        partial(QuietHandler, directory=str(build / 'web/sidera')))
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    errors = []
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(headless=True,
                args=['--enable-unsafe-swiftshader'])
            context = browser.new_context(viewport={'width': 960, 'height': 720})
            page = context.new_page()
            page.on('pageerror', lambda error: errors.append(str(error)))
            page.on('console', lambda message: errors.append(message.text)
                if message.type == 'error' and not message.location.get('url', '').endswith('/favicon.ico')
                else None)
            page.goto(f'http://127.0.0.1:{server.server_port}/')
            canvas = page.locator('canvas')
            canvas.wait_for()
            page.locator('.rp6502-screen').click()
            canvas.focus()
            page.wait_for_function("document.title === 'SIDERA - The Celestial Observatory'")
            # The normal five-second cover cannot be skipped. Allow preparation.
            page.wait_for_timeout(20000)

            def save(expected=None, control='ControlLeft'):
                page.keyboard.press(control + '+s', delay=100)
                deadline = monotonic() + 60
                while monotonic() < deadline:
                    config = page.evaluate(READ_CONFIG)
                    if (expected or 'View ') in config and 'Time ' in config:
                        return config
                    page.wait_for_timeout(100)
                page.screenshot(path=str(output / 'save-failed.png'))
                raise AssertionError(f'Settings not saved: {expected!r}; last={config!r}; errors={errors}')

            initial = save()
            canvas.screenshot(path=str(output / 'sky.png'))
            assert re.search(r'^View \d+,24 120 0 off off 0\r?$', initial, re.M), initial
            # Keep the physical HID modifier pressed until the 6502 reads it.
            page.keyboard.press('ControlLeft+ArrowLeft', delay=500)
            page.wait_for_timeout(500)
            changed = save(' 110 0 off off 0', control='ControlRight')
            page.keyboard.press('ControlRight+ArrowLeft', delay=500)
            page.wait_for_timeout(500)
            page.keyboard.press('ControlLeft+s', delay=100)
            page.wait_for_timeout(1000)
            assert page.evaluate(READ_CONFIG) == changed, 'Right Ctrl changed FOV'
            for key, name in [('k', 'cities'), ('ControlRight+f', 'find'), ('s', 'solar')]:
                page.keyboard.press(key, delay=100)
                page.wait_for_timeout(15000)
                canvas.screenshot(path=str(output / (name + '.png')))
                page.keyboard.press('Escape', delay=100)
                # Closing repaints the sky incrementally before the next panel.
                page.wait_for_timeout(5000)
            # Clear the saved entry after loading to prove that the ROM restored it.
            page.reload()
            page.locator('.rp6502-screen').click()
            canvas.focus()
            page.wait_for_timeout(20000)
            page.evaluate("""() => new Promise((resolve, reject) => {
                const request = indexedDB.open('wojciechgw-sidera');
                request.onsuccess = () => {
                    const db = request.result;
                    const tx = db.transaction('FILE_DATA', 'readwrite');
                    tx.objectStore('FILE_DATA').delete('sidera.cfg');
                    tx.oncomplete = () => { db.close(); resolve(); };
                    tx.onerror = () => reject(tx.error);
                };
            })""")
            restored = save(' 110 0 off off 0')
            assert restored == changed, 'Settings were not restored after reload'
            canvas.screenshot(path=str(output / 'restored.png'))
            assert not errors, errors
            (output / 'result.json').write_text(json.dumps({
                'browser': browser.version, 'initial': initial, 'restored': restored,
                'errors': errors}, indent=2), encoding='utf-8')
            browser.close()
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
    print('PASS: Chromium startup, physical Ctrl keys, IndexedDB save/reload; panel screenshots captured')


if __name__ == '__main__':
    main()
