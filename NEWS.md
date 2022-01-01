## pre9

Well, there are ~2650 commits since the last pre-alpha release (pre8)
so tagging is a due. Initially I wanted to keep the alpha stage entirely
in git, but it simply doesn't work that way. Seems better to release
alpha software than nothing at all. :) Note that this is a quite late
alpha, i.e. we implemented most of the core parts of dosemu2. So this
alpha reflects the final dosemu2 architecture quite strongly, and yet
there are lots more work to do...

Summary of changes:
* Use fdpp as our boot OS. fdpp is a 64bit DOS core that boots under
  dosemu2 and allows to work without installing any DOS. Of course
  you can still install any DOS if you want.
* Use comcom32 as our shell. comcom32 is a command.com variant that
  runs in 32bit space. Eventually we aim for the 64bit shell, but so
  far we only have this. You can still use your favourite shell if you
  want.
* Worked out the security model. We hope that dosemu2 now provides
  a relatively secure sandbox environment, and yet exposes all the
  needed features. Exposing features in a secure manner (rather than
  to simply declare them insecure and disable) was a challenge.
* Add KVM-assisted acceleration to DPMI.
* Implemented region locking and share support in MFS. One of the
  most requested/missed feature of dosemu1 times.
