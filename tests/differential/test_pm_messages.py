"""The port's message ids against Pirate's Moon's own table: the Instant Action
crash was MW3's arguments handed to a message PM changed. Every call that formats
a message by a literal id has the arguments PM's text takes, and no id MW3 had and
PM has not is used, but those in WAITING, each with its reason (pirates-moon.md).
Calls that pick the id at run time are not seen here.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'analysis'))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import msgtable  # noqa: E402
import pm_floors  # noqa: E402

# id: the file it may stay in until its screen comes.
WAITING = {0xfe0: 'src/mech3/mp_lobby.cpp'}
# Same literal, not a message: keys in another table, network ids, PM's own use.
NOT_MESSAGES = {'src/mech3/mech_names.cpp', 'src/gamez/znetwork/znet_session.h'}
CALLS = (r'\b(format_message_ints|format_message_f|format_message|message_format_isi|message_format_is|format_ints|'
         r'set_text_id|message_box3)\s*\(')


def balanced(text, i):
    """What is inside the bracket opened just before i."""
    depth = 0
    for k in range(i, len(text)):
        if text[k] in '({[':
            depth += 1
        elif text[k] in ')}]':
            if depth == 0:
                return text[i:k]
            depth -= 1
    return text[i:]


def spec(text):
    return {int(n) for n in re.findall(r'%([1-9])', text or '')}


def split_args(text):
    out, cur, depth = [], '', 0
    for c in text:
        if c in '({[':
            depth += 1
        if c in ')}]':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def main():
    mw = msgtable.messages((ROOT / 'game/mw3-1.2/Mech3Msg.dll').read_bytes())
    pm = msgtable.messages((ROOT / 'game/pm/Mech3Msg.dll').read_bytes())
    gone = {i for i in mw if i not in pm}
    bad = checked = 0
    for path in sorted((ROOT / 'src').rglob('*')):
        if path.suffix not in ('.cpp', '.h', '.inc') or path.name.startswith('patch12_text'):
            continue
        rel = path.relative_to(ROOT).as_posix()
        text = path.read_text(errors='replace')
        for m in re.finditer(CALLS, text):
            args = split_args(balanced(text, m.end()))
            if len(args) > 1 and args[1].startswith('{'):
                args = [args[0]] + split_args(args[1][1:-1])
            if m.group(1) == 'message_box3':
                args = [a for a in args[2:] if a != 'nullptr']
            if not args or not re.fullmatch(r'0x[0-9a-fA-F]+', args[0]):
                continue
            i = int(args[0], 16)
            if i not in pm or (i in WAITING and WAITING[i] == rel):
                continue
            checked += 1
            want = len(spec(pm[i]))
            if want != len(args) - 1:
                # A text may use only the later of what it is given: its highest %N is what counts.
                if max(spec(pm[i]), default=0) != len(args) - 1:
                    print(f'{rel}: {hex(i)} takes {want}, given {len(args) - 1}: {pm[i][:50]!r}')
                    bad += 1
        if rel in NOT_MESSAGES:
            continue
        for ln, line in enumerate(text.splitlines(), 1):
            code = line.split('//')[0]
            for lit in re.findall(r'(?<![\w+\[])0x0*([0-9a-f]{3,4})\b', code):
                i = int(lit, 16)
                if i not in gone or WAITING.get(i) == rel:
                    continue
                if re.search(r'message|msg|text_of|add_item|set_text', code):
                    print(f'{rel}:{ln}: gone id {hex(i)}: {line.strip()[:80]}')
                    bad += 1
    pm_floors.floors(calls=(checked, 40), gone=(len(gone), 30))
    print(f'{checked} format calls against PM\'s table, {len(gone)} ids gone: {bad} bad')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
