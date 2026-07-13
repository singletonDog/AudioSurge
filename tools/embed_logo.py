import base64
import io
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PNG = os.path.join(ROOT, "assets", "audiosurge.png")
UI = os.path.join(ROOT, "src", "common", "embedded_ui.h")

im = Image.open(PNG).convert("RGBA")
im = im.resize((48, 48), Image.LANCZOS)
buf = io.BytesIO()
im.save(buf, format="PNG", optimize=True)
b64 = base64.b64encode(buf.getvalue()).decode("ascii")
data_uri = "data:image/png;base64," + b64
print("base64 length:", len(b64))

with open(UI, "r", encoding="utf-8") as f:
    html = f.read()

old = '<div class="title-logo">&#9835;</div>'
new = '<div class="title-logo"><img src="' + data_uri + '" alt="AudioSurge"></div>'
if old not in html:
    raise SystemExit("marker not found: title-logo")
html = html.replace(old, new, 1)

old_css = ".title-logo{width:24px;height:24px;display:flex;align-items:center;justify-content:center;color:var(--accent2);font-size:18px;filter:drop-shadow(0 0 6px rgba(109,59,255,0.6))}"
new_css = ".title-logo{width:24px;height:24px;display:flex;align-items:center;justify-content:center;filter:drop-shadow(0 0 6px rgba(109,59,255,0.6))}\n.title-logo img{width:100%;height:100%;object-fit:contain;display:block}"
if old_css not in html:
    raise SystemExit("marker not found: title-logo css")
html = html.replace(old_css, new_css, 1)

with open(UI, "w", encoding="utf-8") as f:
    f.write(html)
print("embedded_ui.h updated")
