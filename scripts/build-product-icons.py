import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parent.parent
SHAPES = {
    "input": '<rect x="3" y="5" width="18" height="14" rx="3"/><path d="M6 9h1m4 0h1m4 0h1M6 12h1m4 0h1m4 0h1M8 15h8"/>',
    "appearance": '<path d="M12 3a9 9 0 1 0 0 18h1a3 3 0 0 0 0-6h-1a2 2 0 0 1 0-4h3a6 6 0 0 0 6-5c-2-2-5-3-9-3Z"/><circle cx="7" cy="10" r=".8"/><circle cx="10" cy="7" r=".8"/><circle cx="15" cy="6" r=".8"/>',
    "helpcode": '<circle cx="12" cy="12" r="9"/><path d="m8 16 4-10 4 10m-6.5-4h5"/>',
    "shortcut": '<path d="M9 7V5a2 2 0 1 0-2 2h10a2 2 0 1 0-2-2v14a2 2 0 1 0 2-2H7a2 2 0 1 0 2 2Z"/>',
    "dictionary": '<path d="M12 5C9 3 6 3 3 4v15c3-1 6-1 9 1 3-2 6-2 9-1V4c-3-1-6-1-9 1Zm0 0v15M6 8h3m-3 4h3m6-4h3m-3 4h3"/>',
    "skin": '<rect x="4" y="3" width="16" height="17" rx="2"/><path d="M8 8h8M8 12h5M8 16h8"/><path d="M2 7v14h14"/>',
    "voice_input": '<rect x="9" y="3" width="6" height="12" rx="3"/><path d="M5 11v1a7 7 0 0 0 14 0v-1M12 19v3m-4 0h8"/>',
    "screen_keyboard": '<rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8m-4-4v4M6 8h1m4 0h1m4 0h1M7 12h10"/>',
    "handwriting": '<path d="m5 16-1 4 4-1L20 7l-3-3Zm9-9 3 3M3 23h18"/>',
    "utilities": '<path d="M14 4a6 6 0 0 0-7 8L3 18a2 2 0 0 0 3 3l6-5a6 6 0 0 0 8-7l-4 4-4-4Z"/>',
    "ai": '<rect x="4" y="5" width="16" height="14" rx="3"/><path d="M9 9v6m0-3h6m0-3v6M8 2v3m8-3v3M8 19v3m8-3v3M1 9h3m-3 6h3m16-6h3m-3 6h3"/>',
    "floating_toolbar": '<rect x="2" y="6" width="20" height="12" rx="4"/><circle cx="7" cy="12" r="1"/><path d="M11 10v4m4-4v4m4-4v4"/>',
    "stats": '<path d="M4 3v18h17M8 16v-4m5 4V8m5 8V5"/>',
    "help": '<path d="M5 3h12l3 3v15H5Z"/><path d="M9 9a3 3 0 1 1 5 2c-1 1-2 1-2 3m0 3h.01"/>',
    "about": '<circle cx="12" cy="12" r="9"/><path d="M12 11v6m0-10h.01"/>',
    "feedback": '<path d="M7 4h10a4 4 0 0 1 4 4v7a4 4 0 0 1-4 4h-6l-6 3v-4a4 4 0 0 1-2-3V8a4 4 0 0 1 4-4Z"/><path d="M8 9h8m-8 5h5"/>',
    "general": '<path d="M4 6h16M4 12h16M4 18h16"/><circle cx="9" cy="6" r="2" fill="#fff"/><circle cx="15" cy="12" r="2" fill="#fff"/><circle cx="9" cy="18" r="2" fill="#fff"/>',
    "settings": '<path d="M4 6h16M4 12h16M4 18h16"/><circle cx="9" cy="6" r="2"/><circle cx="15" cy="12" r="2"/><circle cx="9" cy="18" r="2"/>',
    "emoji": '<circle cx="12" cy="12" r="9"/><path d="M8 9h.01M16 9h.01M7 13a5 5 0 0 0 10 0"/>',
    "check": '<path d="m5 12 4 4L19 6"/>',
    "swap": '<path d="M3 7h17m-4-4 4 4-4 4M21 17H4m4-4-4 4 4 4"/>',
    "close": '<path d="m6 6 12 12M6 18 18 6"/>',
    "minimize": '<path d="M5 12h14"/>',
    "maximize": '<rect x="5" y="5" width="14" height="14" rx="1"/>',
    "restore": '<path d="M8 5h11v11M5 8h11v11H5Z"/>',
    "arrow": '<path d="m9 5 7 7-7 7"/>',
}


def svg(shape, color="#ffffff"):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" '
            f'fill="none" stroke="{color}" stroke-width="1.75" stroke-linecap="round" '
            f'stroke-linejoin="round">{shape}</svg>')


def save(path, content):
    destination = ROOT / path
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(content + "\n", encoding="utf-8")


def brand(size, label=None, dark=False):
    scale = 4
    image = Image.new("RGBA", (size * scale, size * scale))
    draw = ImageDraw.Draw(image)
    unit = size * scale / 64
    if label:
        foreground = "#d8f4ee" if dark else "#104f4b"
        font = ImageFont.truetype("C:/Windows/Fonts/msyhbd.ttc", round(43 * unit))
        draw.text((size * scale / 2, size * scale / 2 - unit), label, font=font,
                  fill=foreground, anchor="mm", stroke_width=0)
    else:
        draw.rounded_rectangle((2 * unit, 2 * unit, 62 * unit, 62 * unit), radius=15 * unit, fill="#0b756b")
        draw.polygon([(13 * unit, 17 * unit), (29 * unit, 21 * unit), (29 * unit, 48 * unit),
                      (13 * unit, 44 * unit)], fill="#e6faf4")
        draw.polygon([(33 * unit, 21 * unit), (49 * unit, 17 * unit), (49 * unit, 44 * unit),
                      (33 * unit, 48 * unit)], fill="#b6e8dc")
        draw.line([(54 * unit, 24 * unit), (54 * unit, 45 * unit)], fill="#ffffff", width=max(1, round(3 * unit)))
    return image.resize((size, size), Image.Resampling.LANCZOS)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--all", action="store_true")
    parser.parse_args()
    assets = ROOT / "server/assets/settings"
    for name in SHAPES:
        if (assets / "sidebar" / f"{name}.svg").exists():
            save(f"server/assets/settings/sidebar/{name}.svg", svg(SHAPES[name]))
    for name in ["minimize", "maximize", "restore", "close", "arrow"]:
        save(f"server/assets/settings/{name}.svg", svg(SHAPES[name]))
    logo = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">'
            '<rect x="2" y="2" width="60" height="60" rx="15" fill="#0b756b"/>'
            '<path d="m13 17 16 4v27l-16-4Z" fill="#e6faf4"/>'
            '<path d="m33 21 16-4v27l-16 4Z" fill="#b6e8dc"/>'
            '<path d="M54 24v21" stroke="#fff" stroke-width="3" stroke-linecap="round"/></svg>')
    save("server/assets/settings/msime.svg", logo)
    for path in ["server/assets/settings/msime.png", "server/assets/settings/MetasequoiaIME.png"]:
        brand(256).save(ROOT / path, optimize=True)
    sizes = [(value, value) for value in [16, 20, 24, 32, 40, 48, 64, 128, 256]]
    icon_paths = ["server/src/resource/MetasequoiaIME.ico", "server/src/resource/Settings.ico",
                  "server/assets/settings/msime.ico"]
    icon_paths += [str(path.relative_to(ROOT)) for path in (ROOT / "windows/image").glob("*.ico")]
    for relative in icon_paths:
        name = Path(relative).stem
        language = {"cn": "中", "en": "EN", "jp": "日", "cap": "A"}
        label = next((text for code, text in language.items() if name.startswith(code + "-")), None)
        label = label or {"ImeModeOn": "中", "ImeModeOff": "EN", "DoubleSingleByteOn": "全",
                          "DoubleSingleByteOff": "半", "PunctuationOn": "。", "PunctuationOff": ","}.get(name)
        brand(256, label, name.endswith("-dark")).save(ROOT / relative, format="ICO", sizes=sizes)
    header = ["#pragma once", "", "namespace ProductIcons", "{"]
    for name in ["settings", "emoji", "screen_keyboard", "handwriting", "voice_input", "about",
                 "floating_toolbar", "input", "check", "swap"]:
        title = "".join(word.capitalize() for word in name.split("_"))
        header.append(f'inline constexpr char {title}[] = R"icon({svg(SHAPES[name], "currentColor")})icon";')
    header.append("}")
    save("server/src/window/product_icons.h", "\n".join(header))
    provenance = {"origin": "Authored geometric book-and-cursor mark and 24-unit line icons",
                  "generator": "scripts/build-product-icons.py", "stroke": 1.75,
                  "raster_assets": [*icon_paths, "server/assets/settings/msime.png",
                                    "server/assets/settings/MetasequoiaIME.png"]}
    save("server/assets/settings/icon-provenance.json", json.dumps(provenance, indent=2, ensure_ascii=False))
    print(f"Generated {len(icon_paths)} ICO files, sidebar SVGs, window controls and shared product icons")


if __name__ == "__main__":
    main()
