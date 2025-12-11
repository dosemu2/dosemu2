from os import environ
from subprocess import CalledProcessError, run


def build_pcmos(self):
    if environ.get("SKIP_EXPENSIVE"):
        self.skipTest("expensive test")

    mosrepo = 'https://github.com/roelandjansen/pcmos386v501.git'
    mosroot = self.imagedir / 'pcmos.git'

    try:
        run(["git", "clone", "-q", "--depth=1", mosrepo, mosroot], check=True)
    except CalledProcessError:
        self.skipTest("GIT repository problem")

    outfiles = [mosroot / 'SOURCES/src/latest' / x for x in [
        '$286n.sys', '$386.sys', '$all.sys', '$arnet.sys',
        '$charge.sys', '$ems.sys', '$gizmo.sys', '$kbbe.sys',
        '$kbcf.sys', '$kbdk.sys', '$kbfr.sys', '$kbgr.sys',
        '$kbit.sys', '$kbla.sys', '$kbnl.sys', '$kbno.sys',
        '$kbpo.sys', '$kbsf.sys', '$kbsg.sys', '$kbsp.sys',
        '$kbsv.sys', '$kbuk.sys', 'minbrdpc.sys', 'mosdd7f.sys',
        '$$mos.sys', '$mouse.sys', '$netbios.sys', '$pipe.sys',
        '$ramdisk.sys', '$serial.sys', '$$shell.sys']]

    for outfile in outfiles:
        outfile.unlink(missing_ok=True)

    # Run the equivalent of the MOSROOT/build.sh script from MOSROOT
    # Note:
    #     We have to avoid runDosemu() as this test is non-interactive
    args = ["-q", "-K", r".:SOURCES\src", "-E", "MAKEMOS.BAT", r"path=%D\bin;%O"]
    results = self.runDosemuCmdline(args, cwd=mosroot, timeout=300)

    self.assertNotIn('Timeout', results)
    self.assertNotIn('NonZeroReturn', results)

    missing = [str(x.relative_to(mosroot)) for x in outfiles if not x.exists()]
    if len(missing):
        msg = "Output file(s) missing %s\n" % str(missing)
        raise self.failureException(msg)
