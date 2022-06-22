from datetime import datetime
from subprocess import (
    check_call,
    check_output,
    CalledProcessError,
    DEVNULL,
    STDOUT,
    TimeoutExpired,
)


def install_freedos(self):
    repo = "https://github.com/dosemu2/install-freedos.git"
    root = self.imagedir / "root.git"

    starttime = datetime.utcnow()

    # Clone the repo
    args = ["git", "clone", "-q", repo, str(root)]
    check_call(args, stdout=DEVNULL, stderr=DEVNULL)

    # Truncate the logfile
    self.logfiles["xpt"][0].write_text("")
    self.logfiles["xpt"][1] = "output.log"

    # Do the downloads to check paths and naming
    for typ in [
        "freedos11",
        "freedos12",
        "freedos13",
        "freedos11userspace",
        "freedos12userspace",
        "freedos13userspace",
    ]:
        with self.subTest(typ=typ):
            try:
                args = [
                    str(root) + "/src/dosemu-downloaddos",
                    "-d", str(self.imagedir) + "/" + typ,
                    "-o", typ,
                ]
                check_output(args, stderr=STDOUT, timeout=600)
            except CalledProcessError as e:
                with self.logfiles["xpt"][0].open("a") as f:
                    f.write(e.output)
                raise self.failureException("Test error") from None
            except TimeoutExpired:
                raise self.failureException("Test timeout") from None

    self.duration = datetime.utcnow() - starttime
