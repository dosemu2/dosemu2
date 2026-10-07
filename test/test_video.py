#!/usr/bin/env python3

"""Every video test that boots a probe of its own instead of DOS.

The one entry point for them, the way test_dosemu.py is the entry point for
the tests that run on a DOS distribution: the cases live in the
class_video_*.py next door, and this file only says which of them to run.

What they have in common is that they are about what dosemu2 puts on a
screen, which cannot be asked of a DOS program running on top of a
distribution, so each boots a probe of its own as the command interpreter.
VideoProbeTestCase in common_framework.py is what that takes.

The video tests that boot a probe but are not about the screen stay where
they are: test_term.py renders on a terminal, test_mouse.py needs a real
pointer.
"""

from common_framework import main, main_setup

from class_video_baseline import VideoBaselineTestCase

if __name__ == "__main__":
    cases = [
        VideoBaselineTestCase,
    ]
    main(main_setup(cases))
