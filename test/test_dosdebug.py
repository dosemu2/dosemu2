#!/usr/bin/python3

import pexpect

from os import environ
from ptyprocess import PtyProcessError
from shutil import copy
from subprocess import call
from time import sleep

from common_framework import BaseTestCase, main, main_setup, IPROMPT
from common_os import frdos130, ppdosgit


class OurTestCase(BaseTestCase):

    def runWithDosdebug(self, cmd,
                        init=None, init_args=None,
                        body=None, body_args=None,
                        fini=None, fini_args=None,
                        opts=None, config=None, timeout=15):

        def default_init(f_log, args):
            # 1/ The Dosemu has been started by the main process
            # 2/ The DOS has been booted and we are sitting at the
            #    command prompt.

            environ['READLINE_DISABLE'] = '1'
            self.dbgchild = pexpect.spawn(str(self.dosdebug), env=environ)
            self.dbgchild.logfile = f_log
            self.dbgchild.setecho(False)

            prompt = r'dosdebug: '
            self.dbgchild.expect([prompt], timeout=5)
            self.dbgchild.sendline('stop')

            self.dbgchild.expect([prompt], timeout=1)
            self.dbgchild.sendline('bpload')

            self.dbgchild.expect([prompt], timeout=1)
            self.dbgchild.sendline('g')

            self.dbgchild.expect([prompt], timeout=1)

        if not init:
            init = default_init

        def default_body(f_log, args):
            # 1/ the application has been started by the main process
            # 2/ the dosdebug instance is sitting at the prompt in a stopped
            #    state after displaying the bpload hit

            cmd = args[0]

            prompt = r'dosdebug: '
            self.dbgchild.expect(['bpload: INT3 caught at'], timeout=5)
            self.dbgchild.expect(['system state: stopped'], timeout=5)
            self.dbgchild.expect([prompt], timeout=5)
            self.dbgchild.sendline(cmd)

            self.dbgchild.expect([prompt], timeout=5)
            return self.dbgchild.before.decode('ASCII', 'replace')

        if not body:
            body = default_body

        def default_fini(f_log, args):
            self.dbgchild.sendline('g')
            self.dbgchild.expect(['program exited', pexpect.EOF, pexpect.TIMEOUT], timeout=5)

            try:
                self.dbgchild.close(force=True)
            except PtyProcessError:
                pass

        if not fini:
            fini = default_fini

        self.logfiles['dbg'] = [self.topdir / str(self.id() + ".dbg"), 'dosdebug.dbg']

        dbin = str(self.dosemu)
        args = ["-f", str(self.imagedir / "dosemu.conf"),
                "-n",
                "-o", str(self.topdir / self.logfiles['log'][0]),
                "-td",
                "--Fimagedir", str(self.imagedir)]
        if opts is not None:
            args.extend(["-I", opts])

        if not config:
            config = '$_hdimage = "dXXXXs/c:hdtype1 +1"\n$_floppy_a = ""\n'
        self.mkfile("dosemu.conf", config, dname=self.imagedir, mode="a")

        child = pexpect.spawn(dbin, args)
        ret = ''
        with open(self.logfiles['xpt'][0], "wb") as fout:
            child.logfile = fout
            child.setecho(False)
            try:
                prompt = r'(system -e|unix -e|' + IPROMPT + ')'
                child.expect([prompt + '[\r\n]*'], timeout=40)
                child.expect(['>[\r\n]*', pexpect.TIMEOUT], timeout=1)

                with open(self.logfiles['dbg'][0], "wb") as f:
                    init(f, init_args)          # startup dosdebug
                    child.send(cmd + '\r\n')    # start dos command
                    ret = body(f, body_args)    # interact using dosdebug
                    fini(f, fini_args)          # tidy up dosdebug

                child.expect(['rem end', pexpect.EOF], timeout=timeout)
            except pexpect.TIMEOUT:
                ret = 'Timeout'
                tlog = self.logfiles['log'][0].read_text()
                if '(gdb) Attaching to program' in tlog:
                    sleep(60)
                    self.shouldStop = True
            except pexpect.EOF:
                ret = 'EndOfFile'

        try:
            child.close(force=True)
        except PtyProcessError:
            pass

        return ret

    def test_ivec(self):
        """Ivec"""

        name = "simple"

        self.mkcom_with_ia16(name, r"""
int main(int argc, char *argv[])
{
  return 0;
}
""")

        self.mkfile("testit.bat", """\
c:\\%s
rem end
""" % name, newline="\r\n")

        results = self.runWithDosdebug("testit.bat", body_args=['ivec',])

        self.assertRegex(results, r"  33  [0-9A-Z]{4}:[0-9A-Z]{4}\(MOUSE_INT33_OFF\)")
        self.assertRegex(results, r"  41  [0-9A-Z]{4}:[0-9A-Z]{4}\(HD_parameter_table0\)")
        self.assertRegex(results, r"  46  [0-9A-Z]{4}:[0-9A-Z]{4}\(HD_parameter_table1\)")
        self.assertRegex(results, r"  61  [0-9A-Z]{4}:[0-9A-Z]{4}\(TCPDRV_OFF\)")


# The DOS variants we want get included here
FRDOS130TestCase = frdos130(OurTestCase, {})
PPDOSGITTestCase = ppdosgit(OurTestCase, {})

if __name__ == '__main__':

    # Dynamically created tests are added here

    argv = main_setup(OurTestCase)
    main(argv)
