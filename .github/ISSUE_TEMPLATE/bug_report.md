---
name: Bug report
about: Create a bug report
title: ''
labels: ''
assignees: ''

---

**Describe the bug**
A description of what the problem is.

**To Reproduce**
Steps to reproduce the behaviour:

**Attach program/game binaries or provide an URL**
If the problem needs specific DOS files to reproduce, please attach
or provide the download URL.

**Attach the log**
It is located in ~/.dosemu/boot.log
Unless you get the plain crash of dosemu2,
you may need to enable some logging flags
to make your report more useful.
See description of -D option in `man dosemu.bin`.

**A regression?**
If you happened to know this problem didn't
exist on dosemu1 or some earlier versions of
dosemu2, please write.

**dosemu2 origins**
Please describe where do you get dosemu2
from (PPA, COPR, git sources). In case of a
source build, please describe any configure-time
customizations. In case of binary packages,
please specify your distribution and make sure
to install the debuginfo packages before creating
the log file.

**Additional info**
Please write here if you did any dosemu2 setup
customizations, like installing any custom DOS
or command.com, or altering the dosemu2
configuration settings. Note that freedos, even
from dosemu-freedos package of dosemu1, counts
as a custom DOS and should be mentioned here.
Also make a note if it is a protected or real-mode
program. If you suspect any particular dosemu2
component (DPMI, PIC, PIT etc) to be responsible
for the problem, please write.
