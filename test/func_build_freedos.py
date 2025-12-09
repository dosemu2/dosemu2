from os import environ
from subprocess import check_call, check_output, CalledProcessError, STDOUT


def build_freedos(self):
    if environ.get("SKIP_EXPENSIVE"):
        self.skipTest("expensive test")

    giturl = 'https://github.com/FDOS/kernel.git'
    root = self.imagedir / 'kernel.git'

    try:
        check_output(["git", "clone", "-q", "--depth=1", giturl, root], stderr=STDOUT)
        check_output(["git", "submodule", "update", "--init", "--recursive", "."], stderr=STDOUT, cwd=root)
    except CalledProcessError:
        self.skipTest("GIT repository problem")

    # Now need to download Watcom 1.9 packages and unpack
    # Path to watcom binaries will be 'c:/devel/watcomc/binw'
    pkgurl = 'https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.4/devel'
    try:
        for pkg in ['watcomc', 'nasm', 'dj_make', 'upx']:
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

        dosbin = self.workdir / 'bin'
        dosbin.mkdir()
        check_call(["cp", "-p", "devel/djgpp/bin/make.exe", dosbin], stderr=STDOUT, cwd=self.workdir)
        check_call(["cp", "-p", "devel/upx/upx.exe", dosbin], stderr=STDOUT, cwd=self.workdir)
    except CalledProcessError:
        self.skipTest("DOS pkgs unavailable")

    # Generate the config
    # Note nasty interaction with comcom64 means we have to switch to dos32a, see
    # https://github.com/dosemu2/comcom64/issues/97
    self.mkfile("config.bat", """\
set COMPILER=WATCOM
set WATCOM=C:\\devel\\watcomc
set DOS4GPATH=%WATCOM%\\binw\\dos32a.exe
set MAKE=wmake /ms
set XCPU=386
set XFAT=32
set XNASM=nasm
set XUPX=upx --8086 --best
set OLDPATH=%PATH%
set PATH=%WATCOM%\\binw;C:\\devel\\nasm;C:\\bin;%OLDPATH%
set DOS4G=QUIET
""", dname=root, newline="\r\n")

    # Run build.bat script from git repository root
    # Note:
    #     We have to avoid runDosemu() as this test is non-interactive
    args = ["-q", "-K", ".", "-E", "build.bat"]
    results = self.runDosemuCmdline(args, cwd=root, timeout=120)

    self.assertNotIn('Timeout', results)
    self.assertNotIn('NonZeroReturn', results)

    # Test for resultant kernel file
    self.assertTrue((root / 'kernel' / 'kernel.sys').exists(), "Resultant (kernel.sys) missing")
