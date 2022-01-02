## pre9

There are over ~3300 commits since the last preview release (pre8)
and a very large chunks of dosemu2 architecture were written and
stabilized. Hopefully it would be possible to do more frequent
releases in the future and avoid such a long development cycles.

Summary of user-visible changes:
* fdpp is now our default OS. It is a 64bit DOS core that boots under
  dosemu2 and allows to work without installing any DOS. Of course
  you can still install any DOS if you want, but that will in most
  cases only cause the features reduction.
* comcom32 is our default shell. comcom32 is a command.com variant that
  runs in 32bit space. Eventually we aim for the 64bit shell, but so
  far we only have this. You can still use your favourite shell if you
  want.
* Worked out the security model. We hope that dosemu2 now provides
  a relatively secure sandbox environment, and yet exposes all the
  needed features. Exposing features in a secure manner (rather than
  to simply declare them insecure and disable) was a challenge.
* Added KVM-assisted acceleration to DPMI (@bartoldeman).
* Implemented region locking and share support in MFS. One of the
  most requested/missed feature of dosemu1 times.
* Implemented $_trace_mmio option (@dos4boss)
* Lots of speed-ups to simx86 (@bartoldeman)
* musl support
* Added lots of CI tests (@andrewbird)
