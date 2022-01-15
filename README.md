# dosemu2

dosemu2 is an emulator for running DOS programs under linux.
It can also serve as a VM to boot various DOSes.

Binary packages for ubuntu are available here:
https://code.launchpad.net/~dosemu2/+archive/ubuntu/ppa

Binary packages for fedora are here:
https://copr.fedorainfracloud.org/coprs/stsp/dosemu2

Binary packages for OpenSUSE are here:
https://download.opensuse.org/repositories/home:/stsp2/openSUSE_Tumbleweed

Please send bug reports to
https://github.com/dosemu2/dosemu2/issues

## Running

Just type `dosemu` to run an emulator.
Use `dosemu -E <dos_cmd>` to run `<dos_cmd>` and exit (add `-T` to not exit).
Use `dosemu -K <unix_dir> -E <dos_cmd>` or `dosemu -K <unix_full_path>`
to run DOS programs from unix directories.

## Configuring

Per-user configuration file can be created as `~/.dosemu/.dosemurc`.
Add your custom settings there.
Look into the global configuration file `/etc/dosemu/dosemu.conf` for
existing settings, their descriptions and default values, and modify
the local config accordingly. `$_hdimage` is probably the first setting
to look into, as it configures the host fs access.

Create `c:\userhook.sys` and/or `c:\userhook.bat` files to customize your
boot sequence. userhook.sys can contain the `config.sys` directives and
`userhook.bat` can contain custom boot commands.

Drive `C:` is usually located at `~/.dosemu/drive_c`. You can add DOS
programs there. Or you can run `dosemu -d <unix_dir>` to mount the
`<unix_dir>` as a new DOS drive.
