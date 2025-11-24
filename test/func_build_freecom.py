from os import environ
from subprocess import check_call, CalledProcessError, run, STDOUT


def build_freecom(self):
    if environ.get("SKIP_EXPENSIVE"):
        self.skipTest("expensive test")

    giturl = 'https://github.com/FDOS/freecom.git'
    root = self.imagedir / 'freecom.git'

    try:
        run(["git", "clone", "-q", "--depth=1", giturl, str(root)], check=True)
    except CalledProcessError:
        self.skipTest("repository unavailable")

    # Now need to download Watcom 1.9 packages and unpack
    # Path to watcom binaries will be 'c:/devel/watcomc/binw'
    pkgurl = 'https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3/devel'
    try:
        for pkg in ['watcomc', 'nasm']:
            check_call([
                "wget",
                "-q",
                pkgurl + '/%s.zip' % pkg,
            ], stderr=STDOUT, cwd=self.imagedir)
            check_call([
                "unzip",
                "-LL",
                "-q",
                str(self.imagedir) + '/%s.zip' % pkg,
            ], stderr=STDOUT, cwd=self.workdir)
    except:
        self.skipTest("DOS pkgs unavailable")

    # Generate the config
    # (note nasty interaction with comcom64, means switch to dos32a)
    self.mkfile("config.bat", """\
set COMPILER=WATCOM
set WATCOM=C:\\devel\\watcomc
set DOS4GPATH=%WATCOM%\\binw\\dos32a.exe
set XCPU=386
set XFAT=32
set XNASM=nasm
set OLDPATH=%PATH%
set PATH=C:\\devel\\watcomc\\binw;C:\\devel\\nasm;C:\\bin;%OLDPATH%
""", dname=root, newline="\r\n")

    check_call([
        "cp",
        "config.std",
        "config.mak",
    ], stderr=STDOUT, cwd=root)

    # Run build.bat script from git repository root
    # Note:
    #     We have to avoid runDosemu() as this test is non-interactive
    args = ["-q", "-K", ".", "-E", "build.bat"]
    results = self.runDosemuCmdline(args, cwd=root, timeout=450)

    self.assertNotIn('Timeout', results)
    self.assertNotIn('NonZeroReturn', results)

    # Test for resultant command.com file
    self.assertTrue((root / 'command.com').exists(), "Resultant (command.com) missing")
