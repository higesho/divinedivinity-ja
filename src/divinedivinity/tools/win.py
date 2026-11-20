# Drive the test game.  The game window is found by its unique title "DDJP TEST" (never the foreground window);
# input goes through the test DLL's shared memory (virtual mouse/keyboard), screenshots via PrintWindow.
#   python tools/win.py start [ENV=VAL..]   copy build/version.dll into the game, launch div.exe with DDJP_TEST=1
#   python tools/win.py shot NAME           -> capture/NAME.png
#   python tools/win.py key K [K..]         tap keys (esc enter space tab up down left right f1.. a..z 0..9 or 0xVK)
#   python tools/win.py click X Y [right]   move the virtual cursor (client coords) and click
#   python tools/win.py move X Y
#   python tools/win.py type TEXT           WM_CHAR
#   python tools/win.py wait MS
#   python tools/win.py stop                kill the game, remove the DLL, restore the renderer config
import sys, os, time, subprocess, shutil, mmap, struct
sys.path.insert(0, os.path.dirname(__file__))
from wl import wins
from cap import cap
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
GAME = os.path.abspath(os.path.join(ROOT, '..'))
PIDF = os.path.join(ROOT, 'work', 'pid.txt')
TITLE = 'DDJP TEST'
VK = {'esc': 0x1b, 'enter': 0x0d, 'space': 0x20, 'tab': 0x09, 'back': 0x08, 'up': 0x26, 'down': 0x28, 'left': 0x25,
      'right': 0x27, 'shift': 0x10, 'ctrl': 0x11, 'alt': 0x12, 'pgup': 0x21, 'pgdn': 0x22, 'home': 0x24, 'end': 0x23, 'del': 0x2e}
for i in range(12): VK['f%d' % (i + 1)] = 0x70 + i
for c in 'abcdefghijklmnopqrstuvwxyz': VK[c] = ord(c.upper())
for c in '0123456789': VK[c] = ord(c)

def hwnd():
    w = [x for x in wins(title=TITLE) if x[5]]
    if not w: raise SystemExit('window "%s" not found' % TITLE)
    return w[0][0]

def shm():
    m = mmap.mmap(-1, 20 + 8 + 256 * 16, tagname='DDJP_TESTIO')
    if struct.unpack_from('<I', m, 0)[0] != 0x504a4444: raise SystemExit('test DLL not running')
    return m

def push(m, *cmds):
    for c in cmds:
        while True:
            wr, rd = struct.unpack_from('<II', m, 4)
            if wr - rd < 250: break
            time.sleep(0.02)
        c = list(c) + [0] * (4 - len(c))
        struct.pack_into('<4i', m, 28 + (wr & 255) * 16, *c)
        struct.pack_into('<I', m, 4, wr + 1)

def settle(m, timeout=60):
    t = time.time()
    while time.time() - t < timeout:
        wr, rd, busy = struct.unpack_from('<III', m, 4)
        if wr == rd and not busy: return
        time.sleep(0.03)
    print('settle timeout')

def set_windowed(on, renderer=None):
    """test runs: chosen renderer in a window; stop restores the user's files"""
    renderer = renderer or os.environ.get('DDJP_RENDERER', 'd3d6')
    names = ['slashed.cfg', 'slashed-d3d6.cfg', 'slashed-software.cfg', 'slashed-directx.cfg']
    for n in names:
        shutil.copy(os.path.join(ROOT, 'work', 'orig', n), os.path.join(GAME, n))
    if on:
        cfg = {'d3d6': 'slashed-d3d6.cfg', 'software': 'slashed-software.cfg', 'directx': 'slashed-directx.cfg'}[renderer]
        p = os.path.join(GAME, cfg)
        d = bytearray(open(p, 'rb').read()); d[0x14] = 0; open(p, 'wb').write(d)
        open(os.path.join(GAME, 'slashed.cfg'), 'wb').write(bytes(d[:4]))

def start():
    set_windowed(True)
    # sinfo.000 = (1,1) marks an unclean exit and makes the game ask to reset its configuration
    try: os.remove(os.path.join(GAME, 'sinfo.000'))
    except OSError: pass
    shutil.copy(os.path.join(ROOT, 'build', 'version.dll'), os.path.join(GAME, 'version.dll'))
    env = dict(os.environ)
    env['DDJP_TEST'] = '1'
    env['__COMPAT_LAYER'] = 'DWM8And16BitMitigation'   # 16-bit display modes for DirectDraw
    env['DDJP_LOG'] = os.path.join(ROOT, 'work', 'ddjp.log')
    for a in sys.argv[2:]:
        k, v = a.split('=', 1); env[k] = v
    p = subprocess.Popen([os.path.join(GAME, 'div.exe')], cwd=GAME, env=env)
    open(PIDF, 'w').write(str(p.pid))
    print('pid', p.pid)

def stop():
    try:
        subprocess.run(['taskkill', '/PID', open(PIDF).read().strip(), '/F'], capture_output=True)
    except Exception as e: print(e)
    time.sleep(1.5)
    try: os.remove(os.path.join(GAME, 'version.dll'))
    except OSError: pass
    set_windowed(False)
    try: os.remove(os.path.join(GAME, 'sinfo.000'))
    except OSError: pass

def main():
    a = sys.argv[1:]
    if not a: print(__doc__); return
    c = a[0]
    if c == 'start': start()
    elif c == 'stop': stop()
    elif c == 'shot':
        m = shm(); settle(m)
        os.makedirs(os.path.join(ROOT, 'capture'), exist_ok=True)
        out = os.path.join(ROOT, 'capture', a[1] + '.png')
        print(cap(hwnd(), out), out)
    elif c == 'key':
        m = shm()
        for k in a[1:]:
            vk = VK[k] if k in VK else int(k, 0)
            push(m, (1, vk), (6, 80), (2, vk), (6, 120))
        settle(m)
    elif c == 'combo':
        # combo ctrl s : hold the first keys, tap the last
        m = shm(); ks = [VK[k] if k in VK else int(k, 0) for k in a[1:]]
        for k in ks[:-1]: push(m, (1, k), (6, 60))
        push(m, (1, ks[-1]), (6, 100), (2, ks[-1]), (6, 60))
        for k in reversed(ks[:-1]): push(m, (2, k), (6, 60))
        settle(m)
    elif c == 'hold':
        m = shm(); vk = VK.get(a[1]) or int(a[1], 0)
        push(m, (1, vk), (6, int(a[2])), (2, vk), (6, 80)); settle(m)
    elif c == 'move':
        m = shm(); push(m, (3, int(a[1]), int(a[2])), (6, 100)); settle(m)
    elif c == 'click':
        m = shm(); b = 1 if len(a) > 3 and a[3] == 'right' else 0
        push(m, (3, int(a[1]), int(a[2])), (6, 120), (4, b), (6, 100), (5, b), (6, 200)); settle(m)
    elif c == 'dclick':
        m = shm()
        push(m, (3, int(a[1]), int(a[2])), (6, 120), (4, 0), (6, 50), (5, 0), (6, 60), (4, 0), (6, 50), (5, 0), (6, 200)); settle(m)
    elif c == 'type':
        m = shm()
        for ch in ' '.join(a[1:]): push(m, (7, ord(ch)), (6, 60))
        settle(m)
    elif c == 'wait':
        m = shm(); push(m, (6, int(a[1]))); settle(m)
    else: print(__doc__)

if __name__ == '__main__': main()
