from os import access, R_OK, W_OK

# Note: The Borland/Turbo Pascal binaries are created using the script at
# the end of this file as I don't want to have to download and install all
# these compilers for this test, so I'm afraid that we have to just grab
# my precompiled binaries, and trust that they are from the source below.

def comcom_r200fix(self, mode):

    # Since the issue that r200fix attempts to fixup is a timing
    # problem, running with cpu emulation is slow enough to not
    # trigger the problem code and can give us an unexpected success.
    if not access("/dev/kvm", W_OK|R_OK):
        self.skipTest("requires KVM")

    self.unTarOrSkip("TEST_R200.tar", [
        ("tp40.exe", "f34a8baff122ef865d6869a04eeb5d16777073bb"),
        ("tp55.exe", "3bc0eb626bb150680cce6dcf20dd19e18df29962"),
        ("tp60.exe", "bbed181798d2d997e383e2e3a5f1308d66cf8eb3"),
        ("tp700.exe", "4c60cf0896c6c7c8fba5d6096c6f474fae5abe7e"),
        ("tp701.exe", "78c2d114ea0aef2f3c92dfdac5cc2418a8ae30a1"),
        ("bp700_rm.exe", "0acd5214a5208d1323c8561d433d2374d35d1143"),
        ("bp701_rm.exe", "8512f112a946d56452082ec6727fd89874851af0"),
        ("bp700_pm.exe", "637a966ad7dd35eda4d0b458e68f2b7443bec6f6"),
        ("bp701_pm.exe", "c84d50fcf352e6d539db658a940b0c0737a7756d"),
        ("rtm700.exe", "d104b1c8250175438eced9af5efda13181d7334a"),
        ("rtm701.exe", "0db290e1843688dbd802d7516321feafe30f3c1f"),
    ])

    if mode == 'REAL':
        self.mkfile("testit.bat", """\
tp40
tp55
tp60
r200fix tp700
r200fix tp701
r200fix bp700_rm
r200fix bp701_rm
rem end
""", newline="\r\n")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        self.assertIn('Turbo Pascal 4.00', results)
        self.assertIn('Turbo Pascal 5.50', results)
        self.assertIn('Turbo Pascal 6.00', results)
        self.assertIn('Turbo Pascal 7.00', results)
        self.assertIn('Turbo Pascal 7.01', results)
        self.assertIn('Borland Pascal 7.00 Real Mode', results)
        self.assertIn('Borland Pascal 7.01 Real Mode', results)

    elif mode == 'PROTECTED':
        self.mkfile("testit.bat", """\
copy rtm700.exe rtm.exe
r200fix bp700_pm
copy rtm701.exe rtm.exe
r200fix bp701_pm
rem end
""", newline="\r\n")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        self.assertIn('Borland Pascal 7.01 Prot Mode', results)
        self.assertIn('Borland Pascal 7.00 Prot Mode', results)

# ======================= compilation script ================================

# from pathlib import Path
#
#
# PROG = """
# program Hello;
# { Minimal program }
#
# uses crt;
#
# begin
#   Assign(Output, 'CON');
#   Rewrite(Output);
#
#   Writeln(Output, REPLACEMENT);
#
#   Close(Output);
# end.
# """
#
# def mkpgm(name, text):
#     tgt = Path(name + '.pas')
#     tgt.write_text(PROG.replace("REPLACEMENT", text), newline='\r\n')
#
# tgts = (
#     ('bp700_pm', "'Borland Pascal 7.00 Prot Mode'", r'c:\bp700\bin\bpc', '-CP'),
#     ('bp700_rm', "'Borland Pascal 7.00 Real Mode'", r'c:\bp700\bin\bpc', '-CD'),
#     ('bp701_pm', "'Borland Pascal 7.01 Prot Mode'", r'c:\bp701\bin\bpc', '-CP'),
#     ('bp701_rm', "'Borland Pascal 7.01 Real Mode'", r'c:\bp701\bin\bpc', '-CD'),
#     ('tp40', "'Turbo Pascal 4.00'", r'c:\tp40\tpc', ''),
#     ('tp55', "'Turbo Pascal 5.50'", r'c:\tp55\tpc', ''),
#     ('tp60', "'Turbo Pascal 6.00'", r'c:\tp60\tpc', ''),
#     ('tp700', "'Turbo Pascal 7.00'", r'c:\tp70\bin\tpc', ''),
#     ('tp701', "'Turbo Pascal 7.01'", r'c:\tp701\bin\tpc', ''),
# )
#
# txt = ''
# for t in tgts:
#     mkpgm(t[0], t[1]);
#     txt += ('%s %s %s.pas\n' % (t[2], t[3], t[0]))
# bat = Path('r200.bat')
# bat.write_text(txt, newline='\r\n')
