#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Apply this repository's UWP patch to your own checkout of DKC3Recomp.

There is one patch and it does not go to the game project: it goes to its `snesrecomp/`
submodule, which is the engine. This script knows that, checks before it writes, and tells you
what it found either way.

    python tools/apply_uwp.py <path-to-game-checkout>
    python tools/apply_uwp.py <path-to-game-checkout> --check     # dry run, writes nothing
    python tools/apply_uwp.py <path-to-game-checkout> --revert    # undo
    python tools/apply_uwp.py --list                              # what the patch changes

It is idempotent: run it twice and the second run reports "already applied" and changes nothing.
That is tested by asking git itself (`git apply --reverse --check`), not by looking for a marker of
ours, so it stays true even if upstream edits that file around our hunks.

Unlike the rest of this port, the patch is **not** behind an `#ifdef`: it replaces two calls
outright. They are the only two things in the engine that pull in `user32`, and a desktop build
does not miss them — it loses two message boxes that print to stderr anyway. Keeping them behind a
define would mean threading a UWP flag through the engine's build for no gain.
"""
import argparse
import subprocess
import sys
from pathlib import Path

for stream in (sys.stdout, sys.stderr):
    if hasattr(stream, 'reconfigure'):
        stream.reconfigure(encoding='utf-8', errors='replace')

HERE = Path(__file__).resolve().parent
PATCHES = HERE.parent / 'patches'

STEPS = (
    ('01-engine-no-user32.patch', 'snesrecomp',
     'The engine: runner/src/rom_image_verify.c, two lines.\n'
     '      Both are MessageBoxA() calls on a wrong-ROM error. MessageBoxA lives in user32,\n'
     '      which does not exist inside an AppContainer, and a UWP package that merely imports\n'
     '      user32 installs and then refuses to activate. They become OutputDebugStringA(), so\n'
     '      the same message still reaches a debugger or the console log, and the ROM check\n'
     '      keeps working exactly as before.'),
)


def run(cmd, cwd):
    return subprocess.run([str(c) for c in cmd], cwd=str(cwd), capture_output=True,
                          text=True, errors='replace')


def git_ok(root):
    p = run(['git', 'rev-parse', '--is-inside-work-tree'], root)
    return p.returncode == 0 and p.stdout.strip() == 'true'


def state(root, patch):
    """'applied', 'pending', or a string explaining why neither is possible."""
    forward = run(['git', 'apply', '--check', str(patch)], root)
    if forward.returncode == 0:
        return 'pending'
    reverse = run(['git', 'apply', '--reverse', '--check', str(patch)], root)
    if reverse.returncode == 0:
        return 'applied'
    return 'stuck: ' + (forward.stderr.strip().splitlines() or ['git apply refused it'])[-1]


def sane(patch):
    """A patch file that is not a patch any more.

    Worth checking, because when it happens `git apply` fails and the natural reading of the
    failure is that your checkout is wrong. It is not. A `.patch` written through a shell
    pipeline can arrive with its newlines stripped (every hunk on one line) or with CRLF where
    the tree has LF, and neither is something you could see by eyeing the file.
    """
    raw = patch.read_bytes()
    if not raw:
        return 'it is empty'
    if b'@@ -' not in raw:
        return 'it has no hunk header (@@), so it is not a unified diff'
    if b'\n' not in raw:
        return ('it has no line breaks at all -- %d bytes on a single line. Something rewrote it '
                'after git produced it' % len(raw))
    if not raw.endswith(b'\n'):
        return 'it does not end in a newline, and git apply needs that'
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('checkout', nargs='?', type=Path,
                    help='root of your DKC3Recomp checkout')
    ap.add_argument('--check', action='store_true', help='report only, write nothing')
    ap.add_argument('--revert', action='store_true', help='remove the patch')
    ap.add_argument('--list', action='store_true', help='print what the patch changes')
    a = ap.parse_args()

    if a.list:
        for name, where, what in STEPS:
            print('%s  ->  %s' % (name, '<checkout>/' + where))
            print('      ' + what + '\n')
        return 0

    if a.checkout is None:
        ap.error('pass the path to your DKC3Recomp checkout '
                 '(there is no default: this script never guesses a path)')

    root = a.checkout.expanduser().resolve()
    if not root.is_dir():
        sys.exit('not a directory: %s' % root)
    if not (root / 'snesrecomp').is_dir():
        sys.exit('that does not look like a DKC3Recomp checkout: expected a snesrecomp/ folder\n'
                 'inside %s. Clone it with its submodules; see docs/INSTALL.md section 1.' % root)
    if not (root / 'generated' / 'snesrecomp').is_dir():
        print('note: generated/snesrecomp/ is not there yet, so the recompilation has not been\n'
              '      generated. That is section 2 of docs/INSTALL.md and it is not needed to\n'
              '      patch, only to build.\n')

    targets = []
    for name, where, _what in STEPS:
        patch = PATCHES / name
        if not patch.is_file():
            sys.exit('missing patch: %s' % patch)
        mal = sane(patch)
        if mal:
            sys.exit('%s is damaged: %s.\nRe-download the repository; this is not a problem '
                     'with your checkout.' % (patch.name, mal))
        target = root / where
        if not git_ok(target):
            sys.exit('%s is not a git work tree, so the patch cannot be checked before it is\n'
                     'written. The submodule has to be initialised: see docs/INSTALL.md '
                     'section 1.' % target)
        targets.append((name, patch, target))

    states = [(name, patch, target, state(target, patch)) for name, patch, target in targets]
    for name, _patch, target, st in states:
        print('%-30s %-14s %s' % (name, st.split(':')[0], target))
    if any(st.startswith('stuck') for *_x, st in states):
        for name, _p, _t, st in states:
            if st.startswith('stuck'):
                print('\n%s: %s' % (name, st), file=sys.stderr)
        sys.exit('\nNothing was written. Either the checkout is not the version this recipe was\n'
                 'written against (see docs/DEPENDENCIES.md) or it has local changes in the file\n'
                 'the patch touches.')

    want = 'applied' if a.revert else 'pending'
    todo = [(name, patch, target) for name, patch, target, st in states if st == want]
    if not todo:
        print('\nNothing to do: everything is already %s.'
              % ('removed' if a.revert else 'applied'))
        return 0
    if a.check:
        print('\n--check: %d patch(es) would be %s. Nothing was written.'
              % (len(todo), 'reverted' if a.revert else 'applied'))
        return 0

    for name, patch, target in todo:
        cmd = ['git', 'apply']
        if a.revert:
            cmd.append('--reverse')
        cmd.append(str(patch))
        p = run(cmd, target)
        if p.returncode:
            print(p.stdout, p.stderr, file=sys.stderr)
            sys.exit('%s failed on %s.' % (name, target))
        print('%s %s' % ('reverted' if a.revert else 'applied ', name))

    if not a.revert:
        print('\nDone. Next: generate the recompilation if you have not (docs/INSTALL.md '
              'section 2)\nand build it (section 3).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
