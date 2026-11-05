"""An empty pass is worse than no test: what a test loaded or reached, against what it should.

The floors are for a default run. With arguments (fewer trials) only zero fails.
"""
import sys


def floors(**counts):
    """name=(got, least): raises SystemExit naming what fell short."""
    short = [(k, got, least) for k, (got, least) in counts.items() if got < (least if len(sys.argv) == 1 else min(least, 1))]
    if short:
        raise SystemExit('too little reached: ' + ', '.join(f'{k} {got} (at least {least})' for k, got, least in short))
