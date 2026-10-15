"""Test browser UI for the static, serverless v0.23.13 Web Edition.

The CI sandbox restricts localhost navigation, so this uses an in-memory DOM.
Audio rendering is tested separately by test_web_wasm.mjs and test_web_worklet.mjs.
"""
import pathlib
from playwright.sync_api import sync_playwright

root=pathlib.Path(__file__).resolve().parent.parent / "web"
html=(root/'index.html').read_text()
js=(root/'web-adapter.js').read_text()
assert '<script src="./web-adapter.js"></script>' in html
html=html.replace('<script src="./web-adapter.js"></script>',f'<script>{js}</script>')
with sync_playwright() as p:
    browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox','--disable-dev-shm-usage'])
    page=browser.new_page(viewport={'width':1440,'height':850})
    errors=[]
    page.on('pageerror',lambda e:errors.append(str(e)))
    page.on('console',lambda m:errors.append(m.text) if m.type=='error' else None)
    page.set_content(html,wait_until='load',timeout=30000)
    assert 'v0.23.13' in page.title(), page.title()
    assert page.locator('#webAudioEnable').count()==1
    assert page.locator('.macro-knob').count()==8
    assert page.locator('#teensyPads button').count()==16
    assert page.locator('#macro-pitch').count()==1
    assert page.locator('#macro-liquid').count()==0
    assert page.locator('.action-pad[data-action="DIRECTION"]').count()==1
    assert page.locator('.action-pad[data-action="HOLD"]').count()==0
    page.click('#enterTeensy')
    assert page.locator('#teensyPage').is_visible()
    assert page.locator('#macro-pitch output').inner_text()=='0 st'
    page.locator('#macro-pitch').focus()
    page.keyboard.press('End')
    assert page.locator('#macro-pitch output').inner_text()=='+12 st'
    assert page.evaluate('vals.pitch')==12
    page.keyboard.press('Home')
    assert page.locator('#macro-pitch output').inner_text()=='-12 st'
    assert page.evaluate('vals.pitch')==-12
    page.locator('#macro-color').focus()
    page.keyboard.press('Home')
    assert page.evaluate('vals.attack')==0, 'COLOR must map attack to 0% at minimum'
    for direction in range(1,7):
        page.locator('.action-pad[data-action="DIRECTION"]').click()
        assert page.evaluate('vals.direction')==direction%6
    assert page.locator('#direction').input_value()=='0'
    page.locator('#macro-motion').focus()
    page.keyboard.press('Home')
    assert page.evaluate('vals.speed')==.25, 'MOTION must retain original speed mapping'
    page.click('#exitTeensy')
    assert page.locator('#fullPage').is_visible()
    assert not errors, errors
    browser.close()
print('PASS v0.23.13 UI: 8 macros, 16 pads, PITCH, COLOR 0%, DIRECTION cycle, MOTION, mode switching; no page errors')
