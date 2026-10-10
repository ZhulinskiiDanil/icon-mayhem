# Button icons of the mod: white glyphs with a dark outline like the icons of the game, drawn 4x
# and scaled down. Run: python tools/make_icons.py resources/icons
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

S = 384          # drawing canvas, 4x the 96 px icon
OUT = 96
OUTLINE = (43, 31, 58, 255)
INK = (43, 31, 58, 255)  # details drawn inside a glyph
FILL = (255, 255, 255, 255)
SHADE = 222      # the fill darkens to this at the bottom
RADIUS = 15      # outline thickness on the canvas


def canvas():
    return Image.new('L', (S, S), 0)


def finish(mask, details=None, colors=None):
    """mask: the white glyph; details: (mask, color) pairs drawn on top of the fill"""
    outline = mask.filter(ImageFilter.MaxFilter(RADIUS * 2 + 1))
    img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    img.paste(Image.new('RGBA', (S, S), OUTLINE), (0, 0), outline)

    # A soft shade towards the bottom, like the buttons of the game
    shade = Image.new('L', (1, S))
    for y in range(S):
        shade.putpixel((0, y), int(255 - (255 - SHADE) * y / S))
    shade = shade.resize((S, S))
    fill = Image.merge('RGBA', (shade, shade, shade, Image.new('L', (S, S), 255)))
    img.paste(fill, (0, 0), mask)

    for detail, color in (details or []):
        img.paste(Image.new('RGBA', (S, S), color), (0, 0), detail)
    return img.resize((OUT, OUT), Image.LANCZOS)


def thick_arc(draw, box, start, end, width):
    draw.arc(box, start, end, fill=255, width=width)


def arrow_head(draw, tip, direction, size):
    dx, dy = math.cos(direction), math.sin(direction)
    px, py = -dy, dx
    base = (tip[0] - dx * size, tip[1] - dy * size)
    draw.polygon([tip, (base[0] + px * size * .8, base[1] + py * size * .8), (base[0] - px * size * .8, base[1] - py * size * .8)], fill=255)


# ! --- Glyphs --- !

def undo():
    m = canvas(); d = ImageDraw.Draw(m)
    thick_arc(d, (80, 96, 304, 320), 200, 400, 46)
    # the arrow at the left end, pointing back
    a = math.radians(200)
    end = (192 + 112 * math.cos(a), 208 + 112 * math.sin(a))
    arrow_head(d, (end[0] - 8, end[1] + 70), math.radians(100), 78)
    return finish(m)


def reset():
    m = canvas(); d = ImageDraw.Draw(m)
    thick_arc(d, (72, 72, 312, 312), 320, 610, 44)
    a = math.radians(320)
    end = (192 + 120 * math.cos(a), 192 + 120 * math.sin(a))
    arrow_head(d, (end[0] + 46, end[1] + 40), math.radians(60), 76)
    return finish(m)


def dice():
    m = canvas(); d = ImageDraw.Draw(m)
    d.rounded_rectangle((70, 70, 314, 314), radius=56, fill=255)
    pips = canvas(); p = ImageDraw.Draw(pips)
    for x, y in [(130, 130), (254, 254), (192, 192), (254, 130), (130, 254)]:
        p.ellipse((x - 24, y - 24, x + 24, y + 24), fill=255)
    return finish(m, [(pips, INK)])


def palette():
    m = canvas(); d = ImageDraw.Draw(m)
    d.ellipse((56, 70, 328, 314), fill=255)
    hole = canvas(); ImageDraw.Draw(hole).ellipse((210, 214, 266, 270), fill=255)
    m = ImageChops.subtract(m, hole)
    dots = []
    for (x, y), color in [((126, 150), (255, 111, 145, 255)), ((192, 118), (255, 210, 74, 255)), ((262, 150), (79, 163, 224, 255)), ((122, 228), (120, 214, 120, 255))]:
        dot = canvas(); ImageDraw.Draw(dot).ellipse((x - 26, y - 26, x + 26, y + 26), fill=255)
        dots.append((dot, color))
    return finish(m, dots)


def drop(d, cx, cy, r):
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
    d.polygon([(cx - r * .92, cy - r * .35), (cx, cy - r * 2.1), (cx + r * .92, cy - r * .35)], fill=255)


def match():
    # Two drops of the same color, the front one over the back one: every part like the hair
    back = canvas(); drop(ImageDraw.Draw(back), 150, 210, 78)
    front = canvas(); drop(ImageDraw.Draw(front), 246, 262, 62)
    edge = front.filter(ImageFilter.MaxFilter(RADIUS * 2 + 1))
    m = ImageChops.lighter(back, front)
    return finish(m, [(ImageChops.subtract(edge, front), OUTLINE)])


def jump():
    m = canvas(); d = ImageDraw.Draw(m)
    d.polygon([(192, 56), (316, 196), (68, 196)], fill=255)
    d.rounded_rectangle((150, 180, 234, 320), radius=14, fill=255)
    return finish(m)


def run():
    m = canvas(); d = ImageDraw.Draw(m)
    d.polygon([(110, 70), (318, 192), (110, 314)], fill=255)
    return finish(m)


def stop():
    m = canvas(); d = ImageDraw.Draw(m)
    d.rounded_rectangle((92, 92, 292, 292), radius=26, fill=255)
    return finish(m)


def floppy(d, box):
    x0, y0, x1, y1 = box
    w = x1 - x0
    d.polygon([(x0, y0), (x1 - w * .2, y0), (x1, y0 + w * .2), (x1, y1), (x0, y1)], fill=255)


def floppy_details(box):
    x0, y0, x1, y1 = box
    w = x1 - x0
    det = canvas(); p = ImageDraw.Draw(det)
    p.rectangle((x0 + w * .22, y0 + w * .02, x1 - w * .3, y0 + w * .3), fill=255)  # the shutter
    p.rounded_rectangle((x0 + w * .16, y1 - w * .42, x1 - w * .16, y1 - w * .08), radius=8, fill=255)  # the label
    lab = canvas(); ImageDraw.Draw(lab).rounded_rectangle((x0 + w * .2, y1 - w * .38, x1 - w * .2, y1 - w * .12), radius=6, fill=255)
    return [(det, INK), (lab, (255, 255, 255, 255))]


def save():
    box = (74, 74, 310, 310)
    m = canvas(); floppy(ImageDraw.Draw(m), box)
    return finish(m, floppy_details(box))


def save_as():
    box = (52, 52, 270, 270)
    m = canvas(); d = ImageDraw.Draw(m)
    floppy(d, box)
    # a plus at the corner
    d.rectangle((262, 200, 302, 330), fill=255)
    d.rectangle((217, 245, 347, 285), fill=255)
    details = floppy_details(box)
    plus = canvas(); q = ImageDraw.Draw(plus)
    q.rectangle((268, 206, 296, 324), fill=255)
    q.rectangle((223, 251, 341, 279), fill=255)
    cut = canvas(); ImageDraw.Draw(cut).rectangle((200, 180, 360, 350), fill=255)
    details = [(ImageChops.subtract(det, cut), color) for det, color in details]
    return finish(m, details + [(plus, (120, 230, 120, 255))])


def hitboxes():
    m = canvas(); d = ImageDraw.Draw(m)
    # a dashed square with a dot in the middle
    for i in range(4):
        t0 = 70 + i * 64
        d.rectangle((t0, 70, t0 + 36, 98), fill=255)
        d.rectangle((t0, 286, t0 + 36, 314), fill=255)
        d.rectangle((70, t0, 98, t0 + 36), fill=255)
        d.rectangle((286, t0, 314, t0 + 36), fill=255)
    d.ellipse((160, 160, 224, 224), fill=255)
    return finish(m)


def page(d, box, fold=60):
    x0, y0, x1, y1 = box
    d.polygon([(x0, y0), (x1 - fold, y0), (x1, y0 + fold), (x1, y1), (x0, y1)], fill=255)


def empty():
    m = canvas(); d = ImageDraw.Draw(m)
    page(d, (92, 56, 292, 328))
    sparkle = canvas(); s = ImageDraw.Draw(sparkle)
    cx, cy = 192, 200
    s.polygon([(cx, cy - 66), (cx + 18, cy - 18), (cx + 66, cy), (cx + 18, cy + 18), (cx, cy + 66), (cx - 18, cy + 18), (cx - 66, cy), (cx - 18, cy - 18)], fill=255)
    return finish(m, [(sparkle, (255, 170, 200, 255))])


def paste():
    m = canvas(); d = ImageDraw.Draw(m)
    d.rounded_rectangle((84, 82, 300, 330), radius=26, fill=255)
    d.rounded_rectangle((140, 48, 244, 112), radius=18, fill=255)
    det = canvas(); p = ImageDraw.Draw(det)
    p.rounded_rectangle((152, 60, 232, 100), radius=12, fill=255)
    for y in (160, 210, 260):
        p.rounded_rectangle((126, y, 258, y + 20), radius=8, fill=255)
    return finish(m, [(det, INK)])


def tray(d):
    d.rectangle((60, 236, 96, 318), fill=255)
    d.rectangle((288, 236, 324, 318), fill=255)
    d.rectangle((60, 290, 324, 324), fill=255)


def import_():
    m = canvas(); d = ImageDraw.Draw(m)
    tray(d)
    d.rectangle((166, 48, 218, 180), fill=255)
    d.polygon([(110, 160), (274, 160), (192, 256)], fill=255)
    return finish(m)


def export():
    m = canvas(); d = ImageDraw.Draw(m)
    tray(d)
    d.rectangle((166, 130, 218, 262), fill=255)
    d.polygon([(110, 150), (274, 150), (192, 54)], fill=255)
    return finish(m)


def folder():
    m = canvas(); d = ImageDraw.Draw(m)
    d.rounded_rectangle((56, 86, 186, 150), radius=20, fill=255)
    d.rounded_rectangle((56, 118, 328, 306), radius=26, fill=255)
    det = canvas(); ImageDraw.Draw(det).rectangle((70, 150, 314, 160), fill=255)
    return finish(m, [(det, INK)])


def copy():
    m = canvas(); d = ImageDraw.Draw(m)
    d.rounded_rectangle((70, 60, 246, 260), radius=22, fill=255)
    back = m.copy()
    front = canvas(); ImageDraw.Draw(front).rounded_rectangle((138, 124, 314, 324), radius=22, fill=255)
    # the front sheet has its own outline over the back one
    edge = front.filter(ImageFilter.MaxFilter(RADIUS * 2 + 1))
    m = ImageChops.lighter(back, front)
    return finish(m, [(ImageChops.subtract(edge, front), OUTLINE)])


def rename():
    m = canvas(); d = ImageDraw.Draw(m)
    # a pencil from the bottom left to the top right
    def rot(points, angle=-45):
        a = math.radians(angle)
        return [(192 + x * math.cos(a) - y * math.sin(a), 192 + x * math.sin(a) + y * math.cos(a)) for x, y in points]
    d.polygon(rot([(-150, -34), (90, -34), (90, 34), (-150, 34)]), fill=255)
    d.polygon(rot([(98, -34), (150, 0), (98, 34)]), fill=255)
    det = canvas(); p = ImageDraw.Draw(det)
    p.polygon(rot([(-150, -34), (-110, -34), (-110, 34), (-150, 34)]), fill=255)
    tip = canvas(); ImageDraw.Draw(tip).polygon(rot([(128, -14), (150, 0), (128, 14)]), fill=255)
    return finish(m, [(det, (255, 143, 177, 255)), (tip, INK)])


def link():
    # Two links of a chain, the second over the first with its own outline
    def ring(cx, cy):
        r = canvas(); q = ImageDraw.Draw(r)
        q.rounded_rectangle((cx - 100, cy - 46, cx + 100, cy + 46), radius=46, fill=255)
        inner = canvas(); ImageDraw.Draw(inner).rounded_rectangle((cx - 66, cy - 14, cx + 66, cy + 14), radius=14, fill=255)
        return ImageChops.subtract(r, inner).rotate(45, center=(cx, cy), resample=Image.BICUBIC)
    first = ring(132, 252)
    second = ring(252, 132)
    edge = second.filter(ImageFilter.MaxFilter(RADIUS * 2 + 1))
    m = ImageChops.lighter(first, second)
    return finish(m, [(ImageChops.subtract(ImageChops.multiply(edge, first), second), OUTLINE)])


def target():
    m = canvas(); d = ImageDraw.Draw(m)
    d.ellipse((64, 64, 320, 320), fill=255)
    det = canvas(); p = ImageDraw.Draw(det)
    p.ellipse((104, 104, 280, 280), fill=255)
    ring = canvas(); ImageDraw.Draw(ring).ellipse((140, 140, 244, 244), fill=255)
    det = ImageChops.subtract(det, ring)
    dot = canvas(); ImageDraw.Draw(dot).ellipse((168, 168, 216, 216), fill=255)
    return finish(m, [(det, INK), (dot, (255, 111, 145, 255))])


def trash():
    m = canvas(); d = ImageDraw.Draw(m)
    d.rounded_rectangle((60, 84, 324, 124), radius=14, fill=255)
    d.rounded_rectangle((150, 54, 234, 96), radius=14, fill=255)
    d.polygon([(86, 130), (298, 130), (276, 324), (108, 324)], fill=255)
    det = canvas(); p = ImageDraw.Draw(det)
    for x in (146, 192, 238):
        p.rounded_rectangle((x - 10, 160, x + 10, 296), radius=8, fill=255)
    return finish(m, [(det, INK)])


def wind():
    # Three gusts: the top one curls up at its end, the bottom one down
    m = canvas(); d = ImageDraw.Draw(m)
    w = 34
    d.rounded_rectangle((44, 108, 236, 108 + w), radius=w // 2, fill=255)
    thick_arc(d, (184, 22, 288, 142), 180, 450, w)
    d.rounded_rectangle((44, 175, 320, 175 + w), radius=w // 2, fill=255)
    d.rounded_rectangle((84, 242, 256, 242 + w), radius=w // 2, fill=255)
    thick_arc(d, (204, 242, 308, 362), 270, 540, w)
    return finish(m)


def people():
    # Two friends: heads and shoulders, the front one over the back one
    back = canvas(); b = ImageDraw.Draw(back)
    b.ellipse((206, 70, 300, 164), fill=255)
    b.pieslice((170, 176, 336, 342), 180, 360, fill=255)
    b.rectangle((170, 258, 336, 300), fill=255)
    front = canvas(); f = ImageDraw.Draw(front)
    f.ellipse((92, 96, 200, 204), fill=255)
    f.pieslice((52, 212, 240, 400), 180, 360, fill=255)
    f.rectangle((52, 304, 240, 326), fill=255)
    edge = front.filter(ImageFilter.MaxFilter(RADIUS * 2 + 1))
    return finish(ImageChops.lighter(back, front), [(ImageChops.subtract(ImageChops.multiply(edge, back), front), OUTLINE)])


def heart():
    m = canvas(); d = ImageDraw.Draw(m)
    d.ellipse((60, 84, 204, 228), fill=255)
    d.ellipse((180, 84, 324, 228), fill=255)
    d.polygon([(70, 180), (314, 180), (192, 330)], fill=255)
    tint = canvas(); ImageDraw.Draw(tint).ellipse((96, 116, 150, 170), fill=255)
    return finish(m, [(tint, (255, 255, 255, 255))])


def gift():
    m = canvas(); d = ImageDraw.Draw(m)
    d.rounded_rectangle((70, 160, 314, 330), radius=18, fill=255)
    d.rounded_rectangle((56, 118, 328, 170), radius=16, fill=255)
    # the bow on top
    d.ellipse((110, 56, 196, 132), fill=255)
    d.ellipse((188, 56, 274, 132), fill=255)
    ribbon = canvas(); r = ImageDraw.Draw(ribbon)
    r.rectangle((172, 118, 212, 330), fill=255)
    r.ellipse((136, 78, 180, 118), fill=255)
    r.ellipse((204, 78, 248, 118), fill=255)
    return finish(m, [(ribbon, (255, 111, 145, 255))])


def globe():
    m = canvas(); d = ImageDraw.Draw(m)
    d.ellipse((56, 56, 328, 328), fill=255)
    lines = canvas(); l = ImageDraw.Draw(lines)
    l.ellipse((132, 64, 252, 320), outline=255, width=16)
    l.line((64, 192, 320, 192), fill=255, width=16)
    l.arc((40, 110, 344, 200), 200, 340, fill=255, width=14)
    l.arc((40, 184, 344, 274), 20, 160, fill=255, width=14)
    mask = canvas(); ImageDraw.Draw(mask).ellipse((70, 70, 314, 314), fill=255)
    return finish(m, [(ImageChops.multiply(lines, mask), (79, 163, 224, 255))])


def camera():
    m = canvas(); d = ImageDraw.Draw(m)
    d.rounded_rectangle((44, 122, 340, 318), radius=38, fill=255)
    d.rounded_rectangle((128, 82, 256, 150), radius=22, fill=255)
    lens = canvas(); ImageDraw.Draw(lens).ellipse((124, 150, 260, 286), fill=255)
    glass = canvas(); ImageDraw.Draw(glass).ellipse((154, 180, 230, 256), fill=255)
    flash = canvas(); ImageDraw.Draw(flash).ellipse((272, 146, 308, 182), fill=255)
    return finish(m, [(lens, INK), (glass, (120, 190, 255, 255)), (flash, (255, 200, 90, 255))])


ICONS = {
    'undo': undo, 'reset': reset, 'dice': dice, 'palette': palette, 'match': match, 'jump': jump, 'run': run,
    'stop': stop, 'save': save, 'save-as': save_as, 'hitboxes': hitboxes, 'empty': empty, 'paste': paste,
    'import': import_, 'export': export, 'folder': folder, 'copy': copy, 'rename': rename, 'link': link,
    'target': target, 'trash': trash, 'wind': wind, 'people': people, 'heart': heart, 'gift': gift, 'globe': globe,
    'camera': camera,
}

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'resources/icons'
    os.makedirs(out, exist_ok=True)
    for name, draw in ICONS.items():
        draw().save(os.path.join(out, f'icon-{name}.png'))
    print(len(ICONS), 'icons')
