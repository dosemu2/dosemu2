
def memory_ems_borland(self):
    self.unTarOrSkip("VARIOUS.tar", [
        ("emstest.com", "d0a07e97905492a5cb9d742513cefeb36d09886d"),
    ])
    # Patch out wait for keypress
    self.patch("emstest.com", [
        (0x824 - 0x100,
        # xor ah, ah
        # int 16
        b'\x32\xe4\xcd\x16',
        # mov ax, 0x1c0d
        # nop
        b'\xb8\x0d\x1c\x90')
    ])

    self.mkfile("testit.bat", """\
c:\\emstest
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""", timeout=10)

    try:
        pt1start = results.index("  PART ONE")
        pt2start = results.index("  PART TWO")
        pt3start = results.index("  PART THREE")
    except ValueError:
        raise self.failureException("Parse Error:\n" + results) from None

    pt1 = results[pt1start:pt2start-1]
    pt2 = results[pt2start:pt3start-1]
    pt3 = results[pt3start:-1]

#  PART ONE - EMS DETECTION
#  ------------------------
#
#Expanded Memory Manager version 64.
#
#INT 67 Handler Address: C102:007B
#Window Segment Address: E000
#Total no. of Pages    :  536 (8576k)
#
#Process ID   Pages allocated
#----------------------------
#   0000         24 ( 384k)
#   0001          4 (  64k)
#   Free        508 (8128k)
#----------------------------
#Press Esc to abort or any other key to continue:

    self.assertRegex(pt1, r"Expanded Memory Manager version \d+")
    self.assertRegex(pt1, r"INT 67 Handler Address: [0-9A-F]{4}:[0-9A-F]{4}")
    self.assertIn("Window Segment Address: E000", pt1)
    self.assertRegex(pt1, r"Free\s+\d+ \(\d+k\)")

#  PART TWO - BASIC EMS FUNCTIONS
#  ------------------------------
#
#     Allocating 128 pages EMS memory: OK.
#             EMS handle (Process ID): 0002
#      Saving page map and map window: OK.
#              Initializing 128 pages: OK.
#                  Checking 128 pages: OK.
#                  Restoring page map: OK.
#  De-allocating 128 pages EMS memory: OK.
#Press Esc to abort or any other key to continue:

    self.assertIn("Allocating 128 pages EMS memory: OK", pt2)
    self.assertRegex(pt2, r"EMS handle \(Process ID\): \d+")
    self.assertIn("Saving page map and map window: OK", pt2)
    self.assertRegex(pt2, r"Initializing 128 pages: ([\s\d\x08]{6})+OK")
    self.assertRegex(pt2, r"Checking 128 pages: ([\s\d\x08]{6})+OK")
    self.assertIn("Restoring page map: OK", pt2)
    self.assertIn("De-allocating 128 pages EMS memory: OK", pt2)


#  PART THREE - EMS I/O FUNCTIONS
#  ------------------------------
#
#     Allocating 128 pages EMS memory: OK.
#             EMS handle (Process ID): 0002
#      Saving page map and map window: OK.
#              Initializing 128 pages: OK.
#      Creating temp file EMSTEST.$$$: OK.
# 250 random EMS I/O with disk access: OK.
#       Erasing temp file EMSTEST.$$$: OK.
#                  Restoring page map: OK.
#  De-allocating 128 pages EMS memory: OK.

    self.assertIn("Allocating 128 pages EMS memory: OK", pt3)
    self.assertRegex(pt3, r"EMS handle \(Process ID\): \d+")
    self.assertIn("Saving page map and map window: OK", pt3)
    self.assertRegex(pt3, r"Initializing 128 pages: ([\s\d\x08]{6})+OK")
    self.assertIn("Creating temp file EMSTEST.$$$: OK", pt3)
    self.assertRegex(pt3, r"250 random EMS I/O with disk access: ([\s\d/\x08]{20})+OK")
    self.assertIn("Erasing temp file EMSTEST.$$$: OK", pt3)
    self.assertIn("Restoring page map: OK", pt3)
    self.assertIn("De-allocating 128 pages EMS memory: OK", pt3)
