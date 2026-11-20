# Noto Sans JP / Noto Serif JP (OFL, Windows fonts folder) -> static instances subset to the characters
# the translation uses -> build/font_sans.ttf (DDJP/2), build/font_serif.ttf (DDJP/3)
import json, os, sys
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer
from fontTools import subset
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
tr = json.load(open(os.path.join(ROOT, 'build', 'tr.json'), encoding='utf-8'))
chars = set()
for v in tr.values(): chars.update(v)
for a, b in ((0x3000, 0x30ff), (0xff01, 0xff5e), (0x2010, 0x2027), (0x2190, 0x2193), (0x25a0, 0x25cf)):
    chars.update(chr(c) for c in range(a, b + 1))
if '--all' in sys.argv:
    # every JIS X 0208 character (for testing before the translation is complete)
    for hi in range(0xa1, 0xff):
        for lo in range(0xa1, 0xff):
            try: chars.add(bytes([hi, lo]).decode('euc_jp'))
            except UnicodeDecodeError: pass
chars = {c for c in chars if ord(c) > 0x7f}
print('chars', len(chars))
for src, wght, fam, out in ((r'C:\Windows\Fonts\NotoSansJP-VF.ttf', 500, 'DDJP Sans', 'font_sans.ttf'),
                            (r'C:\Windows\Fonts\NotoSerifJP-VF.ttf', 600, 'DDJP Serif', 'font_serif.ttf')):
    f = TTFont(src)
    f = instancer.instantiateVariableFont(f, {'wght': wght})
    opt = subset.Options()
    opt.name_IDs = ['*']; opt.name_languages = ['*']; opt.notdef_outline = True; opt.hinting = False
    opt.layout_features = []; opt.drop_tables += ['DSIG']
    s = subset.Subsetter(opt)
    s.populate(unicodes=[ord(c) for c in chars])
    s.subset(f)
    for rec in f['name'].names:
        if rec.nameID in (1, 4, 16): rec.string = fam
        elif rec.nameID == 6: rec.string = fam.replace(' ', '')
    f.save(os.path.join(ROOT, 'build', out))
    print(out, os.path.getsize(os.path.join(ROOT, 'build', out)), 'bytes')
