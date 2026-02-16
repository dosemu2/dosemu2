
def mfs_lredir_auto_hdc(self):
    self.mkfile("testit.bat", "lredir\r\nrem end\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
""")

# C:\>lredir
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE

    self.assertRegex(results, r"C: = /.*")


def mfs_lredir_command(self):
    self.mkfile("testit.bat", """\
lredir X: /tmp
lredir
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_lredir_paths = "/tmp"
""")

# A:\>lredir
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE
# X: = LINUX\FS\tmp\        attrib = READ/WRITE

    self.assertRegex(results, r"X: = /tmp")

def mfs_lredir_command_no_perm(self):
    self.mkfile("testit.bat", """\
lredir X: /tmp
lredir
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat")

# A:\>lredir
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE
# X: = LINUX\FS\tmp\        attrib = READ/WRITE

    self.assertRegex(results, r"Error 5 \(access denied\) while redirecting drive X:")
