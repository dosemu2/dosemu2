
def fat_img_d_writable(self, fat):
    self.mkfile("testit.bat", """\
D:
mkdir test
echo hello > hello.txt
DIR
rem end
""", newline="\r\n")

    testdir = self.mkworkdir('d')

    testfil = testdir / "there.txt"
    testfil.write_text('there')

    name = self.mkimage_vbr(fat, cwd=testdir)

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
""" % name)

    # Std DOS format
    # TEST         <DIR>
    # HELLO    TXT 8
    #
    # ComCom32 format
    # 2019-06-28 22:29 <DIR>         TEST
    # 2019-06-28 22:29             8 HELLO.TXT
    self.assertRegex(results,
            r"TEST[\t ]+<DIR>"
            r"|"
            r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s<DIR>\s+TEST")
    self.assertRegex(results,
            r"HELLO[\t ]+TXT[\t ]+8"
            r"|"
            r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s+8\s+HELLO.TXT")
    self.assertRegex(results,
            r"THERE[\t ]+TXT[\t ]+5"
            r"|"
            r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s+5\s+THERE.TXT")
