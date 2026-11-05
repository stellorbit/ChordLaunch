#!/usr/bin/env python3
"""ChordLaunch UI アイコンシート生成スクリプト。

白色＋透明の PNG スプライトシート（1行に全アイコン）を 3 サイズ分生成し、
C++ 側の添字定義ヘッダ (src/ui/ui_icon_sheet.h) を出力する。
アプリは PNG のアルファをマスクとして使い、テーマ色で着色して描画する。

使い方:
    python -I scripts/Generate-Icons.py

依存: Pillow (pip install pillow)
出力:
    resources/icons/ui_icons_24.png, ui_icons_48.png, ui_icons_96.png
    src/ui/ui_icon_sheet.h
"""
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT_PNG_DIR = ROOT / "resources" / "icons"
OUT_HEADER = ROOT / "src" / "ui" / "ui_icon_sheet.h"

CELL_SIZES = (24, 48, 96)   # 1アイコンの一辺(px)。描画先の大きさに応じて C++ 側で選択する
SUPERSAMPLE = 4             # アンチエイリアス用の超解像倍率
GRID = 96.0                 # 図形定義の座標系（96x96）

# シート内の並び順。C++ の UiIcon 列挙と一致させる（ヘッダを自動生成）
ICON_ORDER = (
    "play", "pause", "stop", "loop", "new", "open", "save", "midi",
    "link", "settings", "help", "template", "dawdrag", "keyboard", "zoom",
)


class Canvas:
    """96 座標系で描ける 'L'(アルファ) キャンバス。"""

    def __init__(self, cell: int):
        self.cell = cell
        self.size = cell * SUPERSAMPLE
        self.k = self.size / GRID
        # 小サイズでも線が細くなりすぎないよう、線幅の下限をピクセル基準で決める
        self.sw = max(6.0, 1.6 * GRID / cell)
        self.img = Image.new("L", (self.size, self.size), 0)
        self.d = ImageDraw.Draw(self.img)

    def p(self, v):
        return v * self.k

    def pts(self, pts):
        return [(self.p(x), self.p(y)) for x, y in pts]

    def dot(self, x, y, r):
        self.d.ellipse([self.p(x - r), self.p(y - r), self.p(x + r), self.p(y + r)], fill=255)

    def line(self, pts, w=None, closed=False):
        """丸端・丸継ぎ手の折れ線。"""
        w = self.sw if w is None else w
        pts = list(pts) + ([pts[0]] if closed else [])
        self.d.line(self.pts(pts), fill=255, width=max(1, round(self.p(w))), joint="curve")
        for x, y in pts:
            self.dot(x, y, w / 2)

    def rrect(self, box, r, fill=False, w=None):
        w = self.sw if w is None else w
        x0, y0, x1, y1 = box
        b = [self.p(x0), self.p(y0), self.p(x1), self.p(y1)]
        if fill:
            self.d.rounded_rectangle(b, radius=self.p(r), fill=255)
        else:
            self.d.rounded_rectangle(b, radius=self.p(r), outline=255, width=max(1, round(self.p(w))))

    def ellipse(self, box, fill=False, w=None):
        w = self.sw if w is None else w
        x0, y0, x1, y1 = box
        b = [self.p(x0), self.p(y0), self.p(x1), self.p(y1)]
        if fill:
            self.d.ellipse(b, fill=255)
        else:
            self.d.ellipse(b, outline=255, width=max(1, round(self.p(w))))

    def arc(self, box, start, end, w=None):
        w = self.sw if w is None else w
        x0, y0, x1, y1 = box
        self.d.arc([self.p(x0), self.p(y0), self.p(x1), self.p(y1)], start, end,
                   fill=255, width=max(1, round(self.p(w))))

    def rpoly(self, pts, rad):
        """角丸の塗りつぶし多角形（頂点に丸ペンを当てて膨らませる）。"""
        self.d.polygon(self.pts(pts), fill=255)
        self.line(pts, w=rad * 2, closed=True)

    def result(self) -> Image.Image:
        small = self.img.resize((self.cell, self.cell), Image.LANCZOS)
        out = Image.new("RGBA", (self.cell, self.cell), (255, 255, 255, 0))
        out.putalpha(small)
        return out


def note(c: Canvas, hx, hy):
    """8分音符（符頭中心 hx,hy）。"""
    c.ellipse((hx - 9, hy - 7, hx + 9, hy + 7), fill=True)
    c.line([(hx + 8, hy - 1), (hx + 8, hy - 36)])
    c.line([(hx + 8, hy - 36), (hx + 22, hy - 28)])


def page(c: Canvas):
    """折り返し付きドキュメントの外形。"""
    c.line([(24, 12), (58, 12), (76, 30), (76, 84), (24, 84)], closed=True)
    c.line([(58, 12), (58, 30), (76, 30)])


def draw_play(c):
    c.rpoly([(32, 22), (32, 74), (76, 48)], 4)


def draw_pause(c):
    c.rrect((26, 20, 42, 76), 5, fill=True)
    c.rrect((54, 20, 70, 76), 5, fill=True)


def draw_stop(c):
    c.rrect((24, 24, 72, 72), 8, fill=True)


def draw_loop(c):
    c.line([(26, 52), (26, 36), (62, 36)])
    c.d.polygon(c.pts([(60, 22), (82, 36), (60, 50)]), fill=255)
    c.line([(70, 44), (70, 60), (34, 60)])
    c.d.polygon(c.pts([(36, 46), (14, 60), (36, 74)]), fill=255)


def draw_new(c):
    page(c)
    c.line([(50, 50), (50, 72)])
    c.line([(39, 61), (61, 61)])


def draw_open(c):
    c.line([(12, 26), (36, 26), (44, 36), (84, 36), (84, 76), (12, 76)], closed=True)
    c.line([(12, 48), (84, 48)])


def draw_save(c):
    c.line([(16, 16), (68, 16), (82, 30), (82, 80), (16, 80)], closed=True)
    c.rrect((30, 16, 60, 36), 3, fill=True)
    c.rrect((28, 52, 68, 80), 3, fill=False)


def draw_midi(c):
    page(c)
    note(c, 40, 68)


def draw_link(c):
    layer = Canvas(c.cell)
    layer.rrect((6, 35, 58, 61), 13)
    layer.rrect((38, 35, 90, 61), 13)
    rot = layer.img.rotate(45, resample=Image.BICUBIC, center=(layer.size / 2, layer.size / 2))
    c.img.paste(rot, (0, 0), rot)
    c.d = ImageDraw.Draw(c.img)


def draw_settings(c):
    for y, kx in ((26, 34), (48, 62), (70, 44)):
        c.line([(14, y), (82, y)], w=c.sw * 0.8)
        c.ellipse((kx - 9, y - 9, kx + 9, y + 9), fill=True)


def draw_help(c):
    c.ellipse((10, 10, 86, 86))
    c.arc((35, 24, 61, 50), 180, 90)
    c.line([(48, 50), (48, 59)])
    c.dot(48, 71, c.sw * 0.75)


def draw_template(c):
    c.rrect((14, 14, 44, 44), 6, fill=True)
    c.rrect((52, 14, 82, 44), 6)
    c.rrect((14, 52, 44, 82), 6)
    c.rrect((52, 52, 82, 82), 6)


def draw_dawdrag(c):
    note(c, 28, 66)
    c.line([(54, 62), (80, 62)])
    c.d.polygon(c.pts([(72, 48), (90, 62), (72, 76)]), fill=255)


def draw_keyboard(c):
    c.rrect((10, 24, 86, 72), 7)
    for x in (29, 48, 67):
        c.line([(x, 24), (x, 72)], w=c.sw * 0.6)
    for x in (23, 42, 61):
        c.rrect((x, 24, x + 12, 50), 2, fill=True)


def draw_zoom(c):
    c.ellipse((14, 14, 60, 60))
    c.line([(54, 54), (82, 82)])


DRAWERS = {
    "play": draw_play, "pause": draw_pause, "stop": draw_stop, "loop": draw_loop,
    "new": draw_new, "open": draw_open, "save": draw_save, "midi": draw_midi,
    "link": draw_link, "settings": draw_settings, "help": draw_help,
    "template": draw_template, "dawdrag": draw_dawdrag, "keyboard": draw_keyboard,
    "zoom": draw_zoom,
}


def build_sheet(cell: int) -> Image.Image:
    sheet = Image.new("RGBA", (cell * len(ICON_ORDER), cell), (255, 255, 255, 0))
    for i, name in enumerate(ICON_ORDER):
        c = Canvas(cell)
        DRAWERS[name](c)
        sheet.paste(c.result(), (i * cell, 0))
    return sheet


def write_header():
    names = "".join(f"    {n.capitalize()},\n" for n in ICON_ORDER)
    sizes = ", ".join(str(s) for s in CELL_SIZES)
    OUT_HEADER.write_text(
        "#pragma once\n"
        "// scripts/Generate-Icons.py が自動生成。手で編集しない。\n"
        "namespace clp {\n\n"
        "enum class UiIcon : int {\n"
        f"{names}"
        "    Count\n"
        "};\n\n"
        f"// スプライトシートの1アイコンの一辺(px)。resource.h の IDR_UIICONS_* と同じ並び\n"
        f"inline constexpr int kUiIconCellSizes[] = {{ {sizes} }};\n\n"
        "} // namespace clp\n",
        encoding="utf-8",
    )


def main():
    OUT_PNG_DIR.mkdir(parents=True, exist_ok=True)
    for cell in CELL_SIZES:
        build_sheet(cell).save(OUT_PNG_DIR / f"ui_icons_{cell}.png", optimize=True)
    write_header()
    print(f"generated {len(ICON_ORDER)} icons x {len(CELL_SIZES)} sizes -> {OUT_PNG_DIR}")


if __name__ == "__main__":
    main()
