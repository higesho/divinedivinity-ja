# release/DivineDivinity_JP.zip = version.dll + 読んでください.txt + OFL.txt
import os, zipfile, shutil
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
REL = os.path.join(ROOT, 'release')
shutil.copy(os.path.join(ROOT, 'build', 'version.dll'), os.path.join(REL, 'version.dll'))
txt = os.path.join(REL, '読んでください.txt')
s = open(txt, encoding='utf-8').read().replace('\r\n', '\n')
open(txt, 'w', encoding='utf-8-sig', newline='\r\n').write(s.lstrip('\ufeff'))
out = os.path.join(REL, 'DivineDivinity_JP.zip')
if os.path.exists(out): os.remove(out)
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for f in ('version.dll', '読んでください.txt', 'OFL.txt'):
        z.write(os.path.join(REL, f), f)
with zipfile.ZipFile(out) as z:
    for i in z.infolist(): print(i.filename, i.file_size, hex(i.flag_bits))
print(os.path.getsize(out), 'bytes')
