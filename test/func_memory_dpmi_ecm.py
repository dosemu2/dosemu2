from shutil import copy
from subprocess import call

def do_test_memory(self, name):
    ename = "%s.com" % name
    edir = self.topdir / "test" / "ecm" / "dpmitest"

    call(["make", "--quiet", "-C", str(edir), ename])
    copy(edir / ename, self.workdir)

    self.mkfile("testit.bat", """\
c:\\%s
rem end
""" % name, newline="\r\n")

    return self.runDosemu("testit.bat")

def memory_dpmi_ecm_alloc(self):
    results = do_test_memory(self, "dpmialoc")
# About to Execute : dpmialoc.com
# Protected mode breakpoint at 0221h.
# 32-bit code segment breakpoint at 0233h.
# Return from child process at 0403h.

# Welcome in 32-bit protected mode.
# DPMI allocation at 02121000h.
# DPMI allocation entrypoint at 00EFh:00000000h.
# Real mode procedure called at 00EFh:00000044h.
# DPMI allocation exit at 00EFh:0000006Ch.
# Hello from DPMI memory section!
# Calling real mode procedure which called callback successful.
# Child process terminated okay, back in real mode.

    self.assertRegex(results, r"Protected mode breakpoint at")
    self.assertRegex(results, r"32-bit code segment breakpoint at")
    self.assertRegex(results, r"Return from child process at")
    self.assertRegex(results, r"Welcome in 32-bit protected mode")
    self.assertRegex(results, r"Hello from DPMI memory section!")
    self.assertRegex(results, r"Calling real mode procedure which called callback successful")
    self.assertRegex(results, r"Child process terminated okay, back in real mode")
    self.assertNotIn("fail", results)

def memory_dpmi_ecm_mini(self):
    results = do_test_memory(self, "dpmimini")
# About to Execute : dpmimini.com
# Protected mode breakpoint at 015Ah.
# 32-bit code segment breakpoint at 016Ch.
#
# Welcome in 32-bit protected mode.

    self.assertRegex(results, r"Protected mode breakpoint at")
    self.assertRegex(results, r"32-bit code segment breakpoint at")
    self.assertRegex(results, r"Welcome in 32-bit protected mode")
    self.assertNotIn("fail", results)

def memory_dpmi_ecm_modeswitch(self):
    results = do_test_memory(self, "dpmims")
# About to Execute : dpmims.com
# Protected mode breakpoint at 015Eh.
#
# Welcome in protected mode.
# Mode-switched to real mode.
# In protected mode again.

    self.assertRegex(results, r"Protected mode breakpoint at")
    self.assertRegex(results, r"Welcome in protected mode")
    self.assertRegex(results, r"Mode-switched to real mode")
    self.assertRegex(results, r"In protected mode again")
    self.assertNotIn("fail", results)

def memory_dpmi_ecm_psp(self):
    results = do_test_memory(self, "dpmipsp")
# About to Execute : dpmipsp.com
# Protected mode breakpoint at 0221h.
# 32-bit code segment breakpoint at 0233h.
# Real mode procedure called at 0275h.
# Return from child process at 02FCh.
#
# Welcome in 32-bit protected mode.
# Calling real mode procedure which called callback successful.
# Child process terminated okay, back in real mode.

    self.assertRegex(results, r"Protected mode breakpoint at")
    self.assertRegex(results, r"32-bit code segment breakpoint at")
    self.assertRegex(results, r"Real mode procedure called at")
    self.assertRegex(results, r"Return from child process at")
    self.assertRegex(results, r"Welcome in 32-bit protected mode")
    self.assertRegex(results, r"Calling real mode procedure which called callback successful")
    self.assertRegex(results, r"Child process terminated okay, back in real mode")
    self.assertNotIn("fail", results)

