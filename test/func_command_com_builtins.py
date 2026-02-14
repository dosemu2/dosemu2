
def command_com_copy(self):
    self.mkfile("testit.bat", r"""
copy version.bat c:\tmp
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat")

    self.assertRegex(results,
            r"1 [fF]ile\(s\) copied"
            r"|"
            r"version.bat =>+ c:\\tmp\\version.bat")


def command_com_keyword_exist(self):
    self.mkfile("testit.bat", r"""
rem X: is a non-existent drive
if not exist X:\ANYTHING.EXE       echo INFO:00_True
if not exist X:\NUL                echo INFO:01_True
if not exist X:\FAKEDIR\NUL        echo INFO:02_True

rem D: is a FAT(local) drive
D:
cd \
mkdir ISDIR
echo hello > ISDIR\EXIST.TRU
if exist D:\NUL                    echo INFO:03_True
if not exist D:\EXIST.NOT          echo INFO:04_True
if not exist D:\NODIR\NUL          echo INFO:05_True
if not exist D:\NODIR\ANYTHING.EXE echo INFO:06_True
if exist D:\ISDIR\EXIST.TRU        echo INFO:07_True
if not exist D:\ISDIR\EXIST.NOT    echo INFO:08_True

rem C: is an MFS(network redirected) drive
C:
cd \
mkdir ISDIR
echo hello > ISDIR\EXIST.TRU
if exist C:\NUL                    echo INFO:09_True
if not exist C:\EXIST.NOT          echo INFO:10_True
if not exist C:\NODIR\NUL          echo INFO:11_True
if not exist C:\NODIR\ANYTHING.EXE echo INFO:12_True
if exist C:\ISDIR\EXIST.TRU        echo INFO:13_True
if not exist C:\ISDIR\EXIST.NOT    echo INFO:14_True

rem end
""", newline="\r\n")

    testdir = self.mkworkdir('d')
    name = self.mkimage("12", cwd=testdir)

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
""" % name)

    for i in range(15):
        self.assertRegex(results, r"(?m)^INFO:%02d_True.*" % i)
