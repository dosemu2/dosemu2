def kernel_cmdline(self):
    # dosemu :FDCMDLINE=xxx hands xxx to the kernel as its command line.
    # The FreeDOS kernel takes CONFIG=file from there, so boot with a
    # config file of our own and check that it was the one processed.
    if self.prettyname != "FR-DOS-GIT":
        self.skipTest("needs FreeDOS kernel with command line support")

    contents = (self.workdir / self.confsys).read_text()
    self.mkfile("cmdline.sys", contents + "SET KCMDLINE=yes\n", newline="\r\n")

    self.mkfile("testit.bat", """\
echo KCL[%KCMDLINE%]
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat",
                             xargs=[":FDCMDLINE=CONFIG=CMDLINE.SYS"])

    self.assertIn("KCL[yes]", results)
