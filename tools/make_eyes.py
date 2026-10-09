# Generates the eye textures of Icon Mayhem: for each type a sclera, an iris (with its
# highlights) and the lashes. Right eye, the outer corner on the right; the game mirrors
# it for the left eye. Drawn 4x bigger and scaled down for smooth edges.
import math
import os
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops

SS = 4
W, H = 256, 176          # eye canvas
IRIS_CANVAS = 128        # iris canvas, the iris centered
OUT = sys.argv[1] if len(sys.argv) > 1 else 'eyes-out'

# Every type has its own eye shape, the mood is in the shape: the inner and outer corners, the
# control points of the upper and lower lid, where the iris rests. The game keeps the same numbers
# (src/hair/Face.cpp, eyeShape())
SHAPES = {
    # big and round, a high arch: soft and cute
    'onyx': dict(I=(38, 104), O=(216, 90), C_UP=(118, -54), C_LO=(130, 184), IRIS_C=(126, 80)),
    # long, the outer corner up, a flatter lower lid: sharp and elegant
    'sapphire': dict(I=(44, 110), O=(222, 64), C_UP=(98, -14), C_LO=(146, 152), IRIS_C=(128, 84)),
    # wide, a heavy flat upper lid: menacing
    'crimson': dict(I=(28, 98), O=(228, 100), C_UP=(128, 14), C_LO=(128, 180), IRIS_C=(128, 100)),
}
I = O = C_UP = C_LO = IRIS_C = None


# Expressions: the shape of the lids says the mood, any design can wear any of them. "Default" is
# the design's own shape above. `iris_dy` lowers the iris for the lids that sit low on it
EXPRESSIONS = {
    'default': None,
    # the cheek pushes the lower lid up into a crescent: a teasing, flirty look
    'flirty': dict(I=(40, 98), O=(216, 84), C_UP=(116, -24), C_LO=(130, 78), iris_dy=-16),
    # heavy half-closed lids: sultry, intimate
    'sultry': dict(I=(40, 100), O=(216, 88), C_UP=(118, 36), C_LO=(130, 150), iris_dy=10),
    # the upper lid cut down towards the nose: angry
    'angry': dict(I=(50, 104), O=(214, 64), C_UP=(178, 20), C_LO=(124, 156), iris_dy=6),
    # round, the outer corners down: kind
    'kind': dict(I=(42, 94), O=(214, 106), C_UP=(112, -44), C_LO=(130, 162), iris_dy=2),
    # wide open, the lower lid lifted a bit by a smile: cheerful
    'cheerful': dict(I=(40, 104), O=(216, 94), C_UP=(118, -60), C_LO=(130, 118), iris_dy=-6),
    # a flat lid straight across the iris: judging
    'judging': dict(I=(40, 92), O=(216, 88), C_UP=(128, 62), C_LO=(128, 170), iris_dy=8),
    # the inner corners raised: sad
    'sad': dict(I=(42, 78), O=(214, 108), C_UP=(70, -16), C_LO=(130, 162), iris_dy=6),
    # both lids far apart: surprised
    'surprised': dict(I=(38, 100), O=(218, 92), C_UP=(120, -64), C_LO=(130, 196), iris_dy=0),
}
EXPRESSION_IRIS_Y = 84 # where the iris rests in the expression shapes, before iris_dy
EXPR = ''        # '' for the default shape, '-flirty' and so on: part of the sclera and lashes names
SHAPE_TABLE = {} # (design, expression) -> sclera width, iris rest, for the game


def use_shape(name, expression='default'):
    global I, O, C_UP, C_LO, IRIS_C, EXPR
    shape = SHAPES[name]
    I, O, C_UP, C_LO, IRIS_C = shape['I'], shape['O'], shape['C_UP'], shape['C_LO'], shape['IRIS_C']
    EXPR = ''
    expr = EXPRESSIONS[expression]
    if expr:
        I, O, C_UP, C_LO = expr['I'], expr['O'], expr['C_UP'], expr['C_LO']
        IRIS_C = (IRIS_C[0], EXPRESSION_IRIS_Y + expr['iris_dy'])
        EXPR = '-' + expression
    SHAPE_TABLE[(name, expression)] = (O[0] - I[0], IRIS_C[0] - W / 2, H / 2 - IRIS_C[1])


def s(p):
    return (p[0] * SS, p[1] * SS)


def bez(a, c, b, t):
    u = 1 - t
    return (u * u * a[0] + 2 * u * t * c[0] + t * t * b[0], u * u * a[1] + 2 * u * t * c[1] + t * t * b[1])


def curve(a, c, b, n=40, offset=0.0, t0=0.0, t1=1.0):
    pts = []
    for i in range(n + 1):
        t = t0 + (t1 - t0) * i / n
        p = bez(a, c, b, t)
        if offset:
            # offset along the normal (upwards for the upper lid when offset < 0)
            e = 1e-3
            q = bez(a, c, b, min(1, t + e))
            r = bez(a, c, b, max(0, t - e))
            dx, dy = q[0] - r[0], q[1] - r[1]
            L = math.hypot(dx, dy) or 1
            nx, ny = -dy / L, dx / L
            p = (p[0] + nx * offset, p[1] + ny * offset)
        pts.append(p)
    return pts


def upper(n=40, **k):
    return curve(I, C_UP, O, n, **k)


def lower(n=40, **k):
    return curve(I, C_LO, O, n, **k)


def sclera_poly():
    return upper() + list(reversed(lower()))[1:-1]


def layer(w=W, h=H):
    return Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))


def finish(img, w=W, h=H):
    return img.resize((w, h), Image.LANCZOS)


def mask_of(poly, w=W, h=H):
    m = Image.new('L', (w * SS, h * SS), 0)
    ImageDraw.Draw(m).polygon([s(p) for p in poly], fill=255)
    return m


def thick(dr, pts, width, color):
    # a polyline of varying width: width is a function of t in 0..1 or a number
    n = len(pts) - 1
    for i in range(n):
        wv = width(i / n) if callable(width) else width
        dr.line([s(pts[i]), s(pts[i + 1])], fill=color, width=max(1, int(wv * SS)))
        r = wv * SS / 2
        x, y = s(pts[i + 1])
        dr.ellipse([x - r, y - r, x + r, y + r], fill=color)


def taper_spike(dr, base, direction, length, width, color, bend=0.0):
    # a lash: a thin curved triangle
    dx, dy = direction
    L = math.hypot(dx, dy) or 1
    dx, dy = dx / L, dy / L
    nx, ny = -dy, dx
    pts = []
    steps = 8
    for i in range(steps + 1):
        t = i / steps
        cx = base[0] + dx * length * t + nx * bend * length * t * t
        cy = base[1] + dy * length * t + ny * bend * length * t * t
        wv = width * (1 - t) * .5
        pts.append(((cx + nx * wv, cy + ny * wv), (cx - nx * wv, cy - ny * wv)))
    poly = [p[0] for p in pts] + [p[1] for p in reversed(pts)]
    dr.polygon([s(p) for p in poly], fill=color)


def gradient_circle(r, stops, w=IRIS_CANVAS, h=IRIS_CANVAS):
    # vertical gradient through color stops [(t, (r,g,b,a))], clipped to a circle of radius r at the center
    img = layer(w, h)
    dr = ImageDraw.Draw(img)
    cy = h / 2
    for y in range(int((cy - r) * SS), int((cy + r) * SS) + 1):
        t = (y / SS - (cy - r)) / (2 * r)
        for k in range(len(stops) - 1):
            if stops[k][0] <= t <= stops[k + 1][0]:
                a, b = stops[k], stops[k + 1]
                u = (t - a[0]) / max(1e-6, b[0] - a[0])
                col = tuple(int(a[1][j] + (b[1][j] - a[1][j]) * u) for j in range(4))
                dr.line([(0, y), (w * SS, y)], fill=col)
                break
    m = Image.new('L', img.size, 0)
    ImageDraw.Draw(m).ellipse([(w / 2 - r) * SS, (cy - r) * SS, (w / 2 + r) * SS, (cy + r) * SS], fill=255)
    img.putalpha(ImageChops.multiply(img.getchannel('A'), m))
    return img


def blurred(img, radius):
    return img.filter(ImageFilter.GaussianBlur(radius * SS))


def over(base, top):
    return Image.alpha_composite(base, top)


def clip(img, m):
    img = img.copy()
    img.putalpha(ImageChops.multiply(img.getchannel('A'), m))
    return img


# ! --- Common parts --- !

def light_sclera(shadow=(170, 160, 200)):
    poly = sclera_poly()
    m = mask_of(poly)
    img = layer()
    ImageDraw.Draw(img).polygon([s(p) for p in poly], fill=(250, 248, 252, 255))
    # the lid casts a soft shadow on the top of the eye
    sh = layer()
    thick(ImageDraw.Draw(sh), upper(offset=6), 22, shadow + (200,))
    img = over(img, clip(blurred(sh, 5), m))
    # a hint of shade at the corners
    corners = layer()
    d = ImageDraw.Draw(corners)
    for c in (I, O):
        d.ellipse([s((c[0] - 18, c[1] - 18)), s((c[0] + 18, c[1] + 18))], fill=shadow + (90,))
    return over(img, clip(blurred(corners, 6), m))


def save(img, name, w=W, h=H):
    # The iris doesn't depend on the shape: written once. The sclera and lashes get the expression
    if '-iris' in name:
        if EXPR:
            return
    else:
        parts = name.split('-', 2)
        name = f'{parts[0]}-{parts[1]}{EXPR}-{parts[2]}'
    finish(img, w, h).save(os.path.join(OUT, name))


def iris_center():
    return (IRIS_CANVAS / 2, IRIS_CANVAS / 2)


def ring(dr, c, r, width, color):
    dr.ellipse([s((c[0] - r, c[1] - r)), s((c[0] + r, c[1] + r))], outline=color, width=max(1, int(width * SS)))


def disc(dr, c, r, color, ry=None):
    ry = ry if ry is not None else r
    dr.ellipse([s((c[0] - r, c[1] - ry)), s((c[0] + r, c[1] + ry))], fill=color)


def heart(dr, c, size, color):
    # pupil heart, the point down
    pts = []
    for i in range(48):
        t = 2 * math.pi * i / 48
        x = math.sin(t) ** 3
        y = (13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)) / 16
        pts.append((c[0] + x * size, c[1] - (y + .1) * size))
    dr.polygon([s(p) for p in pts], fill=color)


# ! --- Onyx: big dark eyes with a red liner flick --- !

def onyx(out):
    save(light_sclera((175, 168, 196)), 'eye-onyx-sclera.png')

    R = 54
    c = iris_center()
    img = gradient_circle(R, [(0, (16, 14, 20, 255)), (.6, (30, 27, 36, 255)), (1, (72, 66, 84, 255))])
    d = ImageDraw.Draw(img)
    # soft lighter bottom glow, darker top
    glow = layer(IRIS_CANVAS, IRIS_CANVAS)
    disc(ImageDraw.Draw(glow), (c[0], c[1] + R * .6), R * .65, (110, 104, 126, 110), R * .3)
    img = over(img, clip(blurred(glow, 4), gradient_circle(R, [(0, (0, 0, 0, 255)), (1, (0, 0, 0, 255))]).getchannel('A')))
    top = layer(IRIS_CANVAS, IRIS_CANVAS)
    disc(ImageDraw.Draw(top), (c[0], c[1] - R * .55), R * .95, (12, 10, 16, 150), R * .5)
    img = over(img, clip(blurred(top, 4), gradient_circle(R, [(0, (0, 0, 0, 255)), (1, (0, 0, 0, 255))]).getchannel('A')))
    d = ImageDraw.Draw(img)
    disc(d, (c[0], c[1] + 2), R * .4, (8, 7, 10, 255), R * .46)
    ring(d, c, R - 1.5, 3, (16, 14, 20, 255))
    # highlights: a big soft oval up left, small dots down right
    disc(d, (c[0] - R * .38, c[1] - R * .42), R * .3, (255, 255, 255, 250), R * .25)
    disc(d, (c[0] + R * .34, c[1] + R * .38), R * .1, (255, 255, 255, 235))
    disc(d, (c[0] - R * .12, c[1] + R * .5), R * .05, (255, 255, 255, 200))
    save(img, 'eye-onyx-iris.png', IRIS_CANVAS, IRIS_CANVAS)

    lash = (38, 36, 58, 255)
    img = layer()
    d = ImageDraw.Draw(img)
    # crease above the lid
    thick(d, upper(offset=-17, t0=.18, t1=.86), 2.2, (196, 150, 150, 200))
    # the upper lash band, thicker towards the outer corner
    thick(d, upper(offset=-2), lambda t: 4 + 9 * t ** 1.4, lash)
    # red liner flick under the outer lashes
    e = O
    taper_spike(d, (e[0] - 6, e[1] - 4), (1, -.45), 34, 7, (200, 40, 60, 255), bend=-.1)
    # outer lashes
    for k, (dx, dy, ln) in enumerate([(1, -.9, 20), (1, -.45, 26), (1, -.1, 22)]):
        taper_spike(d, (e[0] - 8 - k * 5, e[1] - 6 - k * 2), (dx, dy), ln, 7, lash, bend=-.25)
    # lower lash along the outer part with tiny spikes
    thick(d, lower(offset=2, t0=.45, t1=1), lambda t: 1.5 + 2.5 * t, (52, 48, 74, 230))
    for t in (.75, .86, .95):
        p = bez(I, C_LO, O, t)
        taper_spike(d, (p[0], p[1] + 1), (.35, 1), 7, 3, (52, 48, 74, 220))
    save(img, 'eye-onyx-lashes.png')



# ! --- Sapphire: blue to violet, streaks and sparkles --- !

def sapphire(out, heart_pupil=False):
    name = 'eye-sapphire-heart' if heart_pupil else 'eye-sapphire'
    if not heart_pupil:
        save(light_sclera((168, 168, 214)), 'eye-sapphire-sclera.png')

    R = 48
    c = iris_center()
    img = gradient_circle(R, [(0, (30, 16, 78, 255)), (.35, (52, 54, 180, 255)), (.72, (72, 132, 236, 255)), (1, (140, 222, 255, 255))])
    d = ImageDraw.Draw(img)
    # streaks from the pupil out
    streaks = layer(IRIS_CANVAS, IRIS_CANVAS)
    sd = ImageDraw.Draw(streaks)
    for k in range(28):
        a = 2 * math.pi * k / 28 + (.05 if k % 2 else 0)
        r0, r1 = R * .42, R * (.86 if k % 3 else .95)
        sd.line([s((c[0] + math.cos(a) * r0, c[1] + math.sin(a) * r0)), s((c[0] + math.cos(a) * r1, c[1] + math.sin(a) * r1))],
                fill=(170, 210, 255, 90 if k % 2 else 130), width=int(1.2 * SS))
    mask = gradient_circle(R, [(0, (0, 0, 0, 255)), (1, (0, 0, 0, 255))]).getchannel('A')
    img = over(img, clip(blurred(streaks, .4), mask))
    # violet shade under the lid
    top = layer(IRIS_CANVAS, IRIS_CANVAS)
    disc(ImageDraw.Draw(top), (c[0], c[1] - R * .6), R, (24, 14, 70, 170), R * .5)
    img = over(img, clip(blurred(top, 4), mask))
    d = ImageDraw.Draw(img)
    if heart_pupil:
        heart(d, (c[0], c[1] + 2), R * .36, (14, 14, 52, 255))
    else:
        disc(d, (c[0], c[1] + 2), R * .3, (14, 14, 52, 255), R * .36)
    ring(d, c, R - 1.5, 3, (18, 20, 64, 255))
    # highlights
    disc(d, (c[0] - R * .32, c[1] - R * .45), R * .2, (255, 255, 255, 250), R * .17)
    disc(d, (c[0] + R * .38, c[1] - R * .1), R * .07, (255, 255, 255, 240))
    disc(d, (c[0] + R * .2, c[1] + R * .58), R * .09, (255, 255, 255, 220))
    disc(d, (c[0] - R * .08, c[1] + R * .62), R * .05, (255, 255, 255, 190))
    glare = layer(IRIS_CANVAS, IRIS_CANVAS)
    ImageDraw.Draw(glare).arc([s((c[0] - R * .8, c[1] - R * .8)), s((c[0] + R * .8, c[1] + R * .8))], 20, 80, fill=(230, 245, 255, 150),
                              width=int(2 * SS))
    img = over(img, clip(blurred(glare, .8), mask))
    save(img, name + '-iris.png', IRIS_CANVAS, IRIS_CANVAS)
    if heart_pupil:
        return

    lash = (28, 32, 82, 255)
    lash_hi = (66, 86, 170, 255)
    img = layer()
    d = ImageDraw.Draw(img)
    thick(d, upper(offset=-18, t0=.15, t1=.85), 2, (200, 160, 170, 200))
    thick(d, upper(offset=-24, t0=.3, t1=.75), 1.5, (205, 170, 178, 150))
    thick(d, upper(offset=-2), lambda t: 4 + 10 * t ** 1.3, lash)
    thick(d, upper(offset=-6, t0=.35, t1=.95), lambda t: 1 + 2 * t, lash_hi)
    e = O
    for k, (dx, dy, ln, b) in enumerate([(1, -1.2, 18, -.3), (1, -.75, 26, -.3), (1, -.35, 30, -.2), (1, -.02, 22, -.15)]):
        taper_spike(d, (e[0] - 10 - k * 4, e[1] - 7 - k * 1.5), (dx, dy), ln, 8, lash, bend=b)
    thick(d, lower(offset=2, t0=.35, t1=1), lambda t: 1.5 + 3 * t, (58, 74, 160, 235))
    for t in (.7, .82, .93):
        p = bez(I, C_LO, O, t)
        taper_spike(d, (p[0], p[1] + 1), (.4, 1), 9, 3.5, (58, 74, 160, 230))
    save(img, 'eye-sapphire-lashes.png')



# ! --- Crimson: black sclera, red ringed iris, red veins --- !

def crimson(out):
    poly = sclera_poly()
    m = mask_of(poly)
    img = layer()
    ImageDraw.Draw(img).polygon([s(p) for p in poly], fill=(14, 14, 18, 255))
    sheen = layer()
    disc(ImageDraw.Draw(sheen), (I[0] + 50, IRIS_C[1] - 4), 46, (70, 70, 80, 150), 30)
    img = over(img, clip(blurred(sheen, 10), m))
    veins = layer()
    vd = ImageDraw.Draw(veins)
    import random
    rnd = random.Random(7)
    for k in range(14):
        a = 2 * math.pi * k / 14 + rnd.uniform(-.15, .15)
        x, y = IRIS_C[0] + math.cos(a) * 34, IRIS_C[1] + math.sin(a) * 34
        width = 2.2
        for step in range(9):
            a += rnd.uniform(-.35, .35)
            nx, ny = x + math.cos(a) * 9, y + math.sin(a) * 9
            vd.line([s((x, y)), s((nx, ny))], fill=(190, 20, 32, 230), width=max(1, int(width * SS)))
            if rnd.random() < .35:
                b = a + rnd.choice((-1, 1)) * rnd.uniform(.5, 1)
                vd.line([s((nx, ny)), s((nx + math.cos(b) * 10, ny + math.sin(b) * 10))], fill=(170, 16, 28, 200), width=max(1, int(width * .6 * SS)))
            x, y = nx, ny
            width *= .85
    img = over(img, clip(blurred(veins, .3), m))
    save(img, 'eye-crimson-sclera.png')

    R = 40
    c = iris_center()
    img = gradient_circle(R, [(0, (150, 10, 22, 255)), (.5, (226, 34, 44, 255)), (1, (240, 60, 60, 255))])
    mask = gradient_circle(R, [(0, (0, 0, 0, 255)), (1, (0, 0, 0, 255))]).getchannel('A')
    tex = layer(IRIS_CANVAS, IRIS_CANVAS)
    td = ImageDraw.Draw(tex)
    for k in range(40):
        a = 2 * math.pi * k / 40
        td.line([s((c[0] + math.cos(a) * R * .35, c[1] + math.sin(a) * R * .35)), s((c[0] + math.cos(a) * R * .95, c[1] + math.sin(a) * R * .95))],
                fill=(120, 6, 16, 120 if k % 2 else 70), width=int(1.4 * SS))
    img = over(img, clip(blurred(tex, .3), mask))
    d = ImageDraw.Draw(img)
    ring(d, c, R * .72, 2.4, (255, 120, 110, 150))
    ring(d, c, R * .55, 3, (120, 6, 16, 230))
    ring(d, c, R * .28, 3.4, (12, 4, 6, 255))
    disc(d, c, R * .1, (12, 4, 6, 255))
    ring(d, c, R - 2, 4, (70, 0, 8, 255))
    disc(d, (c[0] - R * .52, c[1] + R * .42), R * .13, (255, 255, 255, 245))
    save(img, 'eye-crimson-iris.png', IRIS_CANVAS, IRIS_CANVAS)

    dark = (10, 10, 14, 255)
    img = layer()
    d = ImageDraw.Draw(img)
    thick(d, upper(offset=-2), lambda t: 6 + 6 * math.sin(math.pi * t), dark)
    rnd = random.Random(3)
    for k in range(9):
        t = .1 + .8 * k / 8
        p = bez(I, C_UP, O, t)
        taper_spike(d, (p[0], p[1] - 2), (rnd.uniform(-.4, .5), -1), rnd.uniform(10, 20), rnd.uniform(5, 9), dark, bend=rnd.uniform(-.3, .3))
    thick(d, lower(offset=2, t0=.2, t1=.95), 1.6, (40, 40, 46, 200))
    save(img, 'eye-crimson-lashes.png')



if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for expression in EXPRESSIONS:
        use_shape('onyx', expression)
        onyx(OUT)
        use_shape('sapphire', expression)
        sapphire(OUT)
        if expression == 'default':
            sapphire(OUT, heart_pupil=True)
        use_shape('crimson', expression)
        crimson(OUT)

    # The shape table for the game, so the numbers never drift apart
    designs = ['onyx', 'sapphire', 'crimson']
    lines = ['// Generated by tools/make_eyes.py, do not edit: for each design (Onyx, Sapphire, Crimson) and',
             '// expression (' + ', '.join(EXPRESSIONS) + '): the white of the eye in texture pixels',
             '// and where the iris rests, in pixels from the middle of the canvas, y up',
             'constexpr EyeShape kEyeShapes[3][' + str(len(EXPRESSIONS)) + '] = {']
    for d in designs:
        row = ', '.join('{%.1ff, %.1ff, %.1ff}' % SHAPE_TABLE[(d, e)] for e in EXPRESSIONS)
        lines.append('    {' + row + '},')
    lines.append('};')
    table = os.environ.get('EYE_TABLE')
    if table:
        with open(table, 'w', encoding='utf-8', newline=chr(10)) as f:
            f.write(chr(10).join(lines) + chr(10))
    print('written to', OUT)
