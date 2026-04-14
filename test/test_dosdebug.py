#!/usr/bin/python3

from common_framework import BaseTestCase, CmdState, main, main_setup
from common_os import frdos130, msdos700, ppdosgit


class OurTestCase(BaseTestCase):

    def test_cga_text(self):
        """CGA text in video ram at startup"""

        dbg_script = f"""\
#!/bin/bash
set -e
sleep 1
echo 'd b800:0540 256' | {self.dosdebug}
sleep 1
echo 'd b800:0640 256' | {self.dosdebug}
sleep 1
echo 'd b800:0740 256' | {self.dosdebug}
sleep 1
echo 'd b800:0840 256' | {self.dosdebug}
sleep 1
echo 'd b800:0940 256' | {self.dosdebug}
sleep 1
echo 'd b800:0a40 256' | {self.dosdebug}
sleep 1
echo 'd b800:0b40 256' | {self.dosdebug}
sleep 1
echo 'd b800:0c40 256' | {self.dosdebug}
sleep 1
echo 'd b800:0d40 256' | {self.dosdebug}
sleep 1
echo 'd b800:0e40 256' | {self.dosdebug}
sleep 1
echo 'd b800:0f40 256' | {self.dosdebug}
"""

        dos_results, dbg_results = self.runDosemuWithDosdebug(
                CmdState.WaitAfterStartForSentinal,
                dbg_script,
                dostimeout=30, dbgtimeout=60)

        # Nothing we care about in dos output
#        self.assertIn("dosemu", dos_results)

        # Process the dbg output
        tlist = []
        for l in dbg_results.splitlines():
            # 'b800:0b40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................'
            w = l.split()
            if w and w[0].startswith('b800:'):
                tlist += w[1:17]
        tstr = ' '.join(tlist)
        self.assertIn("43 07 3A 07 5C 07 3E 07", tstr)  # C.:.\.>.

    def test_ivec_at_startup(self):
        """Use the IVEC command to show the interrupt vectors at startup"""

        dbg_script = f"""\
#!/bin/bash
set -e
echo 'ivec' | {self.dosdebug}
"""

        dos_results, dbg_results = self.runDosemuWithDosdebug(
                CmdState.WaitAtPromptForSentinal,
                dbg_script,
                dostimeout=30, dbgtimeout=60)

        self.assertRegex(dbg_results, r"  33  [0-9A-Z]{4}:[0-9A-Z]{4}\(MOUSE_INT33_OFF\)")
        self.assertRegex(dbg_results, r"  41  [0-9A-Z]{4}:[0-9A-Z]{4}\(HD_parameter_table0\)")
        self.assertRegex(dbg_results, r"  46  [0-9A-Z]{4}:[0-9A-Z]{4}\(HD_parameter_table1\)")
        self.assertRegex(dbg_results, r"  61  [0-9A-Z]{4}:[0-9A-Z]{4}\(TCPDRV_OFF\)")


# The DOS variants we want get included here
FRDOS130TestCase = frdos130(OurTestCase, {})
PPDOSGITTestCase = ppdosgit(OurTestCase, {})
MSDOS700TestCase = msdos700(OurTestCase, {})

if __name__ == '__main__':

    # Dynamically created tests are added here

    cases = [
        PPDOSGITTestCase,
        FRDOS130TestCase,
        MSDOS700TestCase,
    ]
    xargv = main_setup(cases)
    main(xargv)
