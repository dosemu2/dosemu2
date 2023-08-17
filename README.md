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

Just type
```
dosemu
```
to run an emulator.

Use
```
dosemu -E <dos_cmd>
```
to run `<dos_cmd>` and exit (add `-T` to not exit).

Use
```
dosemu -K <unix_dir> -E <dos_cmd>
```
or
```
dosemu <unix_full_path> -- <dos_prog_args>
```
to run DOS programs from unix directory.

If you want to run the DOS program from a DOS directory, use this syntax:
```
dosemu -K :C:\\games\\carma -E carma.exe
```
This will run `carma.exe` from `c:\games\carma`. Note the leading colon
after `-K`: it means that the DOS path, rather than unix path, is specified.
You can actually specify both paths:
```
dosemu -K ~/dosgames:carma -E carma.exe
```
This creates the DOS drive for `~/dosgames`, then chdirs to `carma` and
runs `carma.exe`.

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
