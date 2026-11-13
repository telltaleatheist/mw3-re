"""pm_blocktrace.py OUT ADDRS_FILE TEST.py [args] (PM audit, "checked by test"): run a differential test with a block hook on every Unicorn instance;
OUT gets each listed PM function whose first block ran."""
import sys, runpy, unicorn
out, af, test = sys.argv[1], sys.argv[2], sys.argv[3]
want = {int(l.split()[0], 16) for l in open(af) if l.strip()}
hit = set()
orig = unicorn.Uc.__init__
def init(self, *a, **k):
    orig(self, *a, **k)
    self.hook_add(unicorn.UC_HOOK_BLOCK, lambda uc, addr, size, ud: hit.add(addr) if addr in want else None)
unicorn.Uc.__init__ = init
sys.argv = [test] + sys.argv[4:]
try:
    runpy.run_path(test, run_name='__main__')
except SystemExit:
    pass
finally:
    open(out, 'w').write('\n'.join('0x%08x' % a for a in sorted(hit)) + '\n')
    print('blocktrace:', len(hit), 'of', len(want), 'reached', file=sys.stderr)
