#!/usr/bin/python3

import unittest

from sys import argv

from common_framework import (BaseTestCase, main, main_setup, mark, acceptFailure,
                              KNOWNFAIL, UNSUPPORTED)

from common_os import (drdos701, frdos120, frdos130, frdosgit, msdos622,
                       msdos700, msdos710, ppdosgit)

from func_bpb_set import bpb_set
from func_command_com_r200fix import command_com_r200fix
from func_command_com_builtins import command_com_copy, command_com_keyword_exist
from func_command_com_cmdline_length import command_com_cmdline_length
from func_disk import three_drives_vfs, int21_disk_info
from func_ds2_dirops import (ds2_delete_common, ds2_find_common, ds2_find_first,
                             ds2_find_mixed_wild_plain, ds2_rename_common)
from func_ds2_file_seek_tell import ds2_file_seek_tell
from func_ds2_file_seek_read import ds2_file_seek_read
from func_ds2_ftime import ds2_get_ftime, ds2_set_ftime
from func_ds2_read import ds2_read_alt_dta, ds2_read_eof
from func_ds2_set_fattrs import ds2_set_fattrs
from func_ds3_file_access import ds3_file_access
from func_ds3_lock_concurrent import ds3_lock_concurrent
from func_ds3_lock_two_handles import ds3_lock_two_handles
from func_ds3_lock_readlckd import ds3_lock_readlckd
from func_ds3_lock_readonly import ds3_lock_readonly
from func_ds3_lock_twice import ds3_lock_twice
from func_ds3_lock_writable import ds3_lock_writable
from func_ds3_share_open_access import ds3_share_open_access
from func_ds3_share_open_twice import ds3_share_open_twice
from func_fat_img_d_writable import fat_img_d_writable
from func_fcb import (fcb_delete_common, fcb_find_common, fcb_read, fcb_read_alt_dta,
                      fcb_rename_common, fcb_write)
from func_floppy import floppy_img, floppy_vfs
from func_ioctl import drv_removable
from func_lfn_support import lfn_support
from func_lfn_voln_info import lfn_voln_info
from func_lfs_disk_info import lfs_disk_info
from func_label_create import (label_create, label_create_on_lfns,
                                label_create_noduplicate, label_create_nonrootdir,
                                label_delete_wildcard, label_delete_recreate)
from func_lfs_file_info import lfs_file_info
from func_lfs_file_seek_tell import lfs_file_seek_tell
from func_libi86_testsuite import libi86_create_items
from func_lredir import mfs_lredir_auto_hdc, mfs_lredir_command, mfs_lredir_command_no_perm
from func_memory_dpmi_dpmi10_ldt import memory_dpmi_dpmi10_ldt
from func_memory_dpmi_ecm import (memory_dpmi_ecm_alloc, memory_dpmi_ecm_mini,
                                  memory_dpmi_ecm_modeswitch, memory_dpmi_ecm_psp)
from func_memory_dpmi_japheth import memory_dpmi_japheth
from func_memory_dpmi_leak_check import memory_dpmi_leak_check
from func_memory_dpmi_leak_check_dos import memory_dpmi_leak_check_dos
from func_memory_ems_borland import memory_ems_borland, memory_emm286_borland
from func_memory_hma import (memory_hma_freespace, memory_hma_alloc, memory_hma_a20,
                             memory_hma_alloc3, memory_hma_chain)
from func_memory_uma import memory_uma_strategy
from func_memory_xms import memory_xms
from func_misc import (create_new_psp, passing_dos_errorlevel_back, passing_environment_variable,
                       systype)
from func_mfs_directory import mfs_directory_common, mfs_get_current_directory
from func_mfs_read_write import mfs_file_read, mfs_file_write
from func_findfile import (mfs_findfile_ufs_lfn, mfs_findfile_ufs_sfn,
                           mfs_findfile_vfat_linux_mounted_lfn, mfs_findfile_vfat_linux_mounted_sfn,
                           sfn_findfirst)
from func_serial import (serial_simple_read_echo, serial_simple_write_file,
                         lpt_simple_write_pipe)
from func_truename import (mfs_truename_ufs_lfn, mfs_truename_ufs_sfn, mfs_truename_vfat_linux_mounted_lfn,
                           mfs_truename_vfat_linux_mounted_sfn, sfn_truename)

from func_network import network_pktdriver_mtcp
from func_pit_mode_2 import pit_mode_2


class OurTestCase(BaseTestCase):

    attrs = {'cmdtest', 'dpmitest', 'emstest', 'fattest', 'fcbtest', 'hmatest', 'labeltest', 'lfntest',
             'memtest', 'mfstest', 'nettest', 'serialtest', 'sfntest', 'umatest', 'xmstest'}

    @mark('cmdtest')
    def test_command_com_r200fix_real(self):
        """Command.com r200fix Real Mode"""
        command_com_r200fix(self, 'REAL')

    @unittest.expectedFailure
    @mark('cmdtest')
    def test_command_com_r200fix_protected(self):
        """Command.com r200fix Protected Mode"""
        command_com_r200fix(self, 'PROTECTED')

    def test_drv_removable(self):
        """Drive is removable (IOCTL)"""
        drv_removable(self)

    @mark(['mfstest', 'sfntest'])
    def test_mfs_sfn_directory_create(self):
        """MFS SFN directory create"""
        mfs_directory_common(self, "SFN", "Create")

    @mark(['mfstest', 'sfntest'])
    def test_mfs_sfn_directory_delete(self):
        """MFS SFN directory delete"""
        mfs_directory_common(self, "SFN", "Delete")

    @mark(['mfstest', 'sfntest'])
    def test_mfs_sfn_directory_delete_not_empty(self):
        """MFS SFN directory delete not empty"""
        mfs_directory_common(self, "SFN", "DeleteNotEmpty")

    @mark(['mfstest', 'sfntest'])
    def test_mfs_sfn_directory_chdir(self):
        """MFS SFN directory change current"""
        mfs_directory_common(self, "SFN", "Chdir")

    @mark(['mfstest', 'lfntest'])
    def test_mfs_lfn_directory_create(self):
        """MFS LFN directory create"""
        mfs_directory_common(self, "LFN", "Create")

    @mark(['mfstest', 'lfntest'])
    def test_mfs_lfn_directory_delete(self):
        """MFS LFN directory delete"""
        mfs_directory_common(self, "LFN", "Delete")

    @mark(['mfstest', 'lfntest'])
    def test_mfs_lfn_directory_delete_not_empty(self):
        """MFS LFN directory delete not empty"""
        mfs_directory_common(self, "LFN", "DeleteNotEmpty")

    @mark(['mfstest', 'lfntest'])
    def test_mfs_lfn_directory_chdir(self):
        """MFS LFN directory change current"""
        mfs_directory_common(self, "LFN", "Chdir")

    @mark(['mfstest', 'sfntest'])
    def test_mfs_sfn_get_current_directory(self):
        """MFS SFN get current directory"""
        mfs_get_current_directory(self, "SFN")

    @mark(['mfstest', 'lfntest'])
    def test_mfs_lfn_get_current_directory(self):
        """MFS LFN get current directory"""
        mfs_get_current_directory(self, "LFN")

    @mark(['mfstest', 'lfntest'])
    def test_lfn_mfs_support_on(self):
        """LFN MFS Support On"""
        lfn_support(self, "MFS", "on")

    @mark(['fattest', 'lfntest'])
    def test_lfn_fat_support_on(self):
        """LFN FAT Support On"""
        lfn_support(self, "FAT", "on")

    @mark(['mfstest', 'lfntest'])
    def test_lfn_mfs_support_off(self):
        """LFN MFS Support Off"""
        lfn_support(self, "MFS", "off")

    @mark(['fattest', 'lfntest'])
    def test_lfn_fat_support_off(self):
        """LFN FAT Support Off"""
        lfn_support(self, "FAT", "off")

    @mark('labeltest')
    def test_fat_bpb_set_fstype_dinfo(self):
        """FAT BPB store fstype drive info"""
        bpb_set(self, 'fstype', 'dinfo')

    @mark('labeltest')
    def test_fat_bpb_set_fstype_ioctl16(self):
        """FAT BPB store fstype ioctl16"""
        bpb_set(self, 'fstype', 'ioctl')

    @mark('labeltest')
    def test_fat_bpb_set_fstype_ioctl32(self):
        """FAT BPB store fstype ioctl32"""
        bpb_set(self, 'fstype', 'ioctl', 32)

    @mark('labeltest')
    def test_fat_bpb_set_serial_dinfo(self):
        """FAT BPB store serial drive info"""
        bpb_set(self, 'serial', 'dinfo')

    @mark('labeltest')
    def test_fat_bpb_set_serial_ioctl16(self):
        """FAT BPB store serial ioctl16"""
        bpb_set(self, 'serial', 'ioctl')

    @mark('labeltest')
    def test_fat_bpb_set_serial_ioctl32(self):
        """FAT BPB store serial ioctl32"""
        bpb_set(self, 'serial', 'ioctl', 32)

    @mark('labeltest')
    def test_fat_bpb_set_volume_dinfo(self):
        """FAT BPB store volume drive info"""
        bpb_set(self, 'volume', 'dinfo')

    @mark('labeltest')
    def test_fat_bpb_set_volume_ioctl16(self):
        """FAT BPB store volume ioctl16"""
        bpb_set(self, 'volume', 'ioctl')

    @mark('labeltest')
    def test_fat_bpb_set_volume_ioctl32(self):
        """FAT BPB store volume ioctl32"""
        bpb_set(self, 'volume', 'ioctl', 32)

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_simple(self):
        """FAT FCB label create simple"""
        label_create(self, "FAT", None)

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_bpb12(self):
        """FAT FCB label create BPB FAT12"""
        label_create(self, "FAT", 'bpb12')

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_bpb16(self):
        """FAT FCB label create BPB FAT16"""
        label_create(self, "FAT", 'bpb16')

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_bpb32(self):
        """FAT FCB label create BPB FAT32"""
        label_create(self, "FAT", 'bpb32')

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_prefile(self):
        """FAT FCB label create file beforehand"""
        label_create(self, "FAT", 'prefile')

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_predir(self):
        """FAT FCB label create directory beforehand"""
        label_create(self, "FAT", 'predir')

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_postfile(self):
        """FAT FCB label create file afterwards"""
        label_create(self, "FAT", 'postfile')

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_postdir(self):
        """FAT FCB label create directory afterwards"""
        label_create(self, "FAT", 'postdir')

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_on_lfns(self):
        """FAT FCB label create on top of LFNs"""
        label_create_on_lfns(self)

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_noduplicate(self):
        """FAT FCB label create no duplicate"""
        label_create_noduplicate(self, "FAT")

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_create_nonrootdir(self):
        """FAT FCB label create non-rootdir"""
        label_create_nonrootdir(self, "FAT")

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_delete_recreate(self):
        """FAT FCB label delete recreate"""
        label_delete_recreate(self, "FAT")

    @mark(['labeltest', 'fcbtest'])
    def test_fat_label_delete_wildcard(self):
        """FAT FCB label delete wildcard"""
        label_delete_wildcard(self, "FAT")

    @mark('fcbtest')
    def test_fat_fcb_read(self):
        """FAT FCB file read simple"""
        fcb_read(self, "FAT")

    @mark('fcbtest')
    def test_mfs_fcb_read(self):
        """MFS FCB file read simple"""
        fcb_read(self, "MFS")

    @mark('fcbtest')
    def test_fat_fcb_read_alt_dta(self):
        """FAT FCB file read alternate DTA"""
        fcb_read_alt_dta(self, "FAT")

    @mark('fcbtest')
    def test_mfs_fcb_read_alt_dta(self):
        """MFS FCB file read alternate DTA"""
        fcb_read_alt_dta(self, "MFS")

    @mark('fcbtest')
    def test_fat_fcb_write(self):
        """FAT FCB file write simple"""
        fcb_write(self, "FAT")

    @mark('fcbtest')
    def test_mfs_fcb_write(self):
        """MFS FCB file write simple"""
        fcb_write(self, "MFS")

    @mark('fcbtest')
    def test_fat_fcb_rename_simple(self):
        """FAT FCB file rename simple"""
        fcb_rename_common(self, "FAT", "simple")

    @mark('fcbtest')
    def test_mfs_fcb_rename_simple(self):
        """MFS FCB file rename simple"""
        fcb_rename_common(self, "MFS", "simple")

    @mark('fcbtest')
    def test_fat_fcb_rename_source_missing(self):
        """FAT FCB file rename source missing"""
        fcb_rename_common(self, "FAT", "source_missing")

    @mark('fcbtest')
    def test_mfs_fcb_rename_source_missing(self):
        """MFS FCB file rename source missing"""
        fcb_rename_common(self, "MFS", "source_missing")

    @mark('fcbtest')
    def test_fat_fcb_rename_target_exists(self):
        """FAT FCB file rename target exists"""
        fcb_rename_common(self, "FAT", "target_exists")

    @mark('fcbtest')
    def test_mfs_fcb_rename_target_exists(self):
        """MFS FCB file rename target exists"""
        fcb_rename_common(self, "MFS", "target_exists")

    @mark('fcbtest')
    def test_fat_fcb_rename_wild_1(self):
        """FAT FCB file rename wildcard one"""
        fcb_rename_common(self, "FAT", "wild_one")

    @mark('fcbtest')
    def test_mfs_fcb_rename_wild_1(self):
        """MFS FCB file rename wildcard one"""
        fcb_rename_common(self, "MFS", "wild_one")

    @mark('fcbtest')
    def test_fat_fcb_rename_wild_2(self):
        """FAT FCB file rename wildcard two"""
        fcb_rename_common(self, "FAT", "wild_two")

    @mark('fcbtest')
    def test_mfs_fcb_rename_wild_2(self):
        """MFS FCB file rename wildcard two"""
        fcb_rename_common(self, "MFS", "wild_two")

    @mark('fcbtest')
    def test_fat_fcb_rename_wild_3(self):
        """FAT FCB file rename wildcard three"""
        fcb_rename_common(self, "FAT", "wild_three")

    @mark('fcbtest')
    def test_mfs_fcb_rename_wild_3(self):
        """MFS FCB file rename wildcard three"""
        fcb_rename_common(self, "MFS", "wild_three")

    @mark('fcbtest')
    def test_fat_fcb_rename_wild_4(self):
        """FAT FCB file rename wildcard four"""
        fcb_rename_common(self, "FAT", "wild_four")

    @mark('fcbtest')
    def test_mfs_fcb_rename_wild_4(self):
        """MFS FCB file rename wildcard four"""
        fcb_rename_common(self, "MFS", "wild_four")

    @mark('fcbtest')
    def test_fat_fcb_delete_simple(self):
        """FAT FCB file delete simple"""
        fcb_delete_common(self, "FAT", "simple")

    @mark('fcbtest')
    def test_mfs_fcb_delete_simple(self):
        """MFS FCB file delete simple"""
        fcb_delete_common(self, "MFS", "simple")

    @mark('fcbtest')
    def test_fat_fcb_delete_missing(self):
        """FAT FCB file delete missing"""
        fcb_delete_common(self, "FAT", "missing")

    @mark('fcbtest')
    def test_mfs_fcb_delete_missing(self):
        """MFS FCB file delete missing"""
        fcb_delete_common(self, "MFS", "missing")

    @mark('fcbtest')
    def test_fat_fcb_delete_wild_1(self):
        """FAT FCB file delete wildcard one"""
        fcb_delete_common(self, "FAT", "wild_one")

    @mark('fcbtest')
    def test_mfs_fcb_delete_wild_1(self):
        """MFS FCB file delete wildcard one"""
        fcb_delete_common(self, "MFS", "wild_one")

    @mark('fcbtest')
    def test_fat_fcb_delete_wild_2(self):
        """FAT FCB file delete wildcard two"""
        fcb_delete_common(self, "FAT", "wild_two")

    @mark('fcbtest')
    def test_mfs_fcb_delete_wild_2(self):
        """MFS FCB file delete wildcard two"""
        fcb_delete_common(self, "MFS", "wild_two")

    @mark('fcbtest')
    def test_fat_fcb_delete_wild_3(self):
        """FAT FCB file delete wildcard three"""
        fcb_delete_common(self, "FAT", "wild_three")

    @mark('fcbtest')
    def test_mfs_fcb_delete_wild_3(self):
        """MFS FCB file delete wildcard three"""
        fcb_delete_common(self, "MFS", "wild_three")

    @mark('fcbtest')
    def test_fat_fcb_find_simple(self):
        """FAT FCB file find simple"""
        fcb_find_common(self, "FAT", "simple")

    @mark('fcbtest')
    def test_mfs_fcb_find_simple(self):
        """MFS FCB file find simple"""
        fcb_find_common(self, "MFS", "simple")

    @mark('fcbtest')
    def test_fat_fcb_find_missing(self):
        """FAT FCB file find missing"""
        fcb_find_common(self, "FAT", "missing")

    @mark('fcbtest')
    def test_mfs_fcb_find_missing(self):
        """MFS FCB file find missing"""
        fcb_find_common(self, "MFS", "missing")

    @mark('fcbtest')
    def test_fat_fcb_find_wild_1(self):
        """FAT FCB file find wildcard one"""
        fcb_find_common(self, "FAT", "wild_one")

    @mark('fcbtest')
    def test_mfs_fcb_find_wild_1(self):
        """MFS FCB file find wildcard one"""
        fcb_find_common(self, "MFS", "wild_one")

    @mark('fcbtest')
    def test_fat_fcb_find_wild_2(self):
        """FAT FCB file find wildcard two"""
        fcb_find_common(self, "FAT", "wild_two")

    @mark('fcbtest')
    def test_mfs_fcb_find_wild_2(self):
        """MFS FCB file find wildcard two"""
        fcb_find_common(self, "MFS", "wild_two")

    @mark('fcbtest')
    def test_fat_fcb_find_wild_3(self):
        """FAT FCB file find wildcard three"""
        fcb_find_common(self, "FAT", "wild_three")

    @mark('fcbtest')
    def test_mfs_fcb_find_wild_3(self):
        """MFS FCB file find wildcard three"""
        fcb_find_common(self, "MFS", "wild_three")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_read_eof(self):
        """FAT DOSv2 file read EOF"""
        ds2_read_eof(self, "FAT")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_read_eof(self):
        """MFS DOSv2 file read EOF"""
        ds2_read_eof(self, "MFS")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_read_alt_dta(self):
        """FAT DOSv2 file read alternate DTA"""
        ds2_read_alt_dta(self, "FAT")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_read_alt_dta(self):
        """MFS DOSv2 file read alternate DTA"""
        ds2_read_alt_dta(self, "MFS")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_file_seek_set_read(self):
        """FAT DOSv2 file seek set read"""
        ds2_file_seek_read(self, "FAT", "SET")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_file_seek_set_read(self):
        """MFS DOSv2 file seek set read"""
        ds2_file_seek_read(self, "MFS", "SET")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_file_seek_cur_read(self):
        """FAT DOSv2 file seek current read"""
        ds2_file_seek_read(self, "FAT", "CUR")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_file_seek_cur_read(self):
        """MFS DOSv2 file seek current read"""
        ds2_file_seek_read(self, "MFS", "CUR")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_file_seek_end_read(self):
        """FAT DOSv2 file seek end read"""
        ds2_file_seek_read(self, "FAT", "END")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_file_seek_end_read(self):
        """MFS DOSv2 file seek end read"""
        ds2_file_seek_read(self, "MFS", "END")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_file_seek_tell_end_back(self):
        """FAT DOSv2 file seek tell end back"""
        ds2_file_seek_tell(self, "FAT", "ENDBCKSML")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_file_seek_tell_end_back(self):
        """MFS DOSv2 file seek tell end back"""
        ds2_file_seek_tell(self, "MFS", "ENDBCKSML")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_file_seek_tell_end_back_large(self):
        """FAT DOSv2 file seek tell end back large"""
        ds2_file_seek_tell(self, "FAT", "ENDBCKLRG")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_file_seek_tell_end_back_large(self):
        """MFS DOSv2 file seek tell end back large"""
        ds2_file_seek_tell(self, "MFS", "ENDBCKLRG")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_file_seek_tell_end_forward(self):
        """FAT DOSv2 file seek tell end forward"""
        ds2_file_seek_tell(self, "FAT", "ENDFWDSML")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_file_seek_tell_end_forward(self):
        """MFS DOSv2 file seek tell end forward"""
        ds2_file_seek_tell(self, "MFS", "ENDFWDSML")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_file_seek_tell_end_forward_large(self):
        """FAT DOSv2 file seek tell end forward large"""
        ds2_file_seek_tell(self, "FAT", "ENDFWDLRG")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_file_seek_tell_end_forward_large(self):
        """MFS DOSv2 file seek tell end forward large"""
        ds2_file_seek_tell(self, "MFS", "ENDFWDLRG")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_rename_file(self):
        """FAT DOSv2 rename file"""
        ds2_rename_common(self, "FAT", "file")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_rename_file(self):
        """MFS DOSv2 rename file"""
        ds2_rename_common(self, "MFS", "file")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_rename_file_src_missing(self):
        """FAT DOSv2 rename file src missing"""
        ds2_rename_common(self, "FAT", "file_src_missing")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_rename_file_src_missing(self):
        """MFS DOSv2 rename file src missing"""
        ds2_rename_common(self, "MFS", "file_src_missing")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_rename_file_tgt_exists(self):
        """FAT DOSv2 rename file tgt exists"""
        ds2_rename_common(self, "FAT", "file_tgt_exists")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_rename_file_tgt_exists(self):
        """MFS DOSv2 rename file tgt exists"""
        ds2_rename_common(self, "MFS", "file_tgt_exists")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_rename_dir(self):
        """FAT DOSv2 rename dir"""
        ds2_rename_common(self, "FAT", "dir")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_rename_dir(self):
        """MFS DOSv2 rename dir"""
        ds2_rename_common(self, "MFS", "dir")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_rename_dir_src_missing(self):
        """FAT DOSv2 rename dir src missing"""
        ds2_rename_common(self, "FAT", "dir_src_missing")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_rename_dir_src_missing(self):
        """MFS DOSv2 rename dir src missing"""
        ds2_rename_common(self, "MFS", "dir_src_missing")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_rename_dir_tgt_exists(self):
        """FAT DOSv2 rename dir tgt exists"""
        ds2_rename_common(self, "FAT", "dir_tgt_exists")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_rename_dir_tgt_exists(self):
        """MFS DOSv2 rename dir tgt exists"""
        ds2_rename_common(self, "MFS", "dir_tgt_exists")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_delete_file(self):
        """FAT DOSv2 delete file"""
        ds2_delete_common(self, "FAT", "file")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_delete_file(self):
        """MFS DOSv2 delete file"""
        ds2_delete_common(self, "MFS", "file")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_delete_file_missing(self):
        """FAT DOSv2 delete file missing"""
        ds2_delete_common(self, "FAT", "file_missing")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_delete_file_missing(self):
        """MFS DOSv2 delete file missing"""
        ds2_delete_common(self, "MFS", "file_missing")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_find_simple(self):
        """FAT DOSv2 file find simple"""
        ds2_find_common(self, "FAT", "simple")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_find_simple(self):
        """MFS DOSv2 file find simple"""
        ds2_find_common(self, "MFS", "simple")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_find_missing(self):
        """FAT DOSv2 file find missing"""
        ds2_find_common(self, "FAT", "missing")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_find_missing(self):
        """MFS DOSv2 file find missing"""
        ds2_find_common(self, "MFS", "missing")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_find_wild_1(self):
        """FAT DOSv2 file find wildcard one"""
        ds2_find_common(self, "FAT", "wild_one")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_find_wild_1(self):
        """MFS DOSv2 file find wildcard one"""
        ds2_find_common(self, "MFS", "wild_one")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_find_wild_2(self):
        """FAT DOSv2 file find wildcard two"""
        ds2_find_common(self, "FAT", "wild_two")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_find_wild_2(self):
        """MFS DOSv2 file find wildcard two"""
        ds2_find_common(self, "MFS", "wild_two")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_find_wild_3(self):
        """FAT DOSv2 file find wildcard three"""
        ds2_find_common(self, "FAT", "wild_three")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_find_wild_3(self):
        """MFS DOSv2 file find wildcard three"""
        ds2_find_common(self, "MFS", "wild_three")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_file_exists(self):
        """FAT DOSv2 findfirst file exists"""
        ds2_find_first(self, "FAT", "file_exists")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_file_exists(self):
        """MFS DOSv2 findfirst file exists"""
        ds2_find_first(self, "MFS", "file_exists")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_file_exists_as_dir(self):
        """FAT DOSv2 findfirst file exists as dir"""
        ds2_find_first(self, "FAT", "file_exists_as_dir")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_file_exists_as_dir(self):
        """MFS DOSv2 findfirst file exists as dir"""
        ds2_find_first(self, "MFS", "file_exists_as_dir")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_file_not_found(self):
        """FAT DOSv2 findfirst file not found"""
        ds2_find_first(self, "FAT", "file_not_found")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_file_not_found(self):
        """MFS DOSv2 findfirst file not found"""
        ds2_find_first(self, "MFS", "file_not_found")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_no_more_files(self):
        """FAT DOSv2 findfirst no more files"""
        ds2_find_first(self, "FAT", "no_more_files")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_no_more_files(self):
        """MFS DOSv2 findfirst no more files"""
        ds2_find_first(self, "MFS", "no_more_files")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_path_not_found_wc(self):
        """FAT DOSv2 findfirst path not found wildcard"""
        ds2_find_first(self, "FAT", "path_not_found_wc")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_path_not_found_wc(self):
        """MFS DOSv2 findfirst path not found wildcard"""
        ds2_find_first(self, "MFS", "path_not_found_wc")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_path_not_found_pl(self):
        """FAT DOSv2 findfirst path not found plain"""
        ds2_find_first(self, "FAT", "path_not_found_pl")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_path_not_found_pl(self):
        """MFS DOSv2 findfirst path not found plain"""
        ds2_find_first(self, "MFS", "path_not_found_pl")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_path_exists_empty(self):
        """FAT DOSv2 findfirst path exists empty"""
        ds2_find_first(self, "FAT", "path_exists_empty")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_path_exists_empty(self):
        """MFS DOSv2 findfirst path exists empty"""
        ds2_find_first(self, "MFS", "path_exists_empty")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_path_exists_file_not_dir(self):
        """FAT DOSv2 findfirst path exists file not dir"""
        ds2_find_first(self, "FAT", "path_exists_file_not_dir")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_path_exists_file_not_dir(self):
        """MFS DOSv2 findfirst path exists file not dir"""
        ds2_find_first(self, "MFS", "path_exists_file_not_dir")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_path_exists_not_empty(self):
        """FAT DOSv2 findfirst path exists not empty"""
        ds2_find_first(self, "FAT", "path_exists_not_empty")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_path_exists_not_empty(self):
        """MFS DOSv2 findfirst path exists not empty"""
        ds2_find_first(self, "MFS", "path_exists_not_empty")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_dir_exists_pl(self):
        """FAT DOSv2 findfirst dir exists plain"""
        ds2_find_first(self, "FAT", "dir_exists_pl")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_dir_exists_pl(self):
        """MFS DOSv2 findfirst dir exists plain"""
        ds2_find_first(self, "MFS", "dir_exists_pl")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_dir_exists_wc(self):
        """FAT DOSv2 findfirst dir exists wildcard"""
        ds2_find_first(self, "FAT", "dir_exists_wc")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_dir_exists_wc(self):
        """MFS DOSv2 findfirst dir exists wildcard"""
        ds2_find_first(self, "MFS", "dir_exists_wc")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_dir_not_exists_pl(self):
        """FAT DOSv2 findfirst dir not exists plain"""
        ds2_find_first(self, "FAT", "dir_not_exists_pl")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_dir_not_exists_pl(self):
        """MFS DOSv2 findfirst dir not exists plain"""
        ds2_find_first(self, "MFS", "dir_not_exists_pl")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_dir_not_exists_wc(self):
        """FAT DOSv2 findfirst dir not exists wildcard"""
        ds2_find_first(self, "FAT", "dir_not_exists_wc")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_dir_not_exists_wc(self):
        """MFS DOSv2 findfirst dir not exists wildcard"""
        ds2_find_first(self, "MFS", "dir_not_exists_wc")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_findfirst_dir_not_exists_fn(self):
        """FAT DOSv2 findfirst dir not exists filename"""
        ds2_find_first(self, "FAT", "dir_not_exists_fn")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_findfirst_dir_not_exists_fn(self):
        """MFS DOSv2 findfirst dir not exists filename"""
        ds2_find_first(self, "MFS", "dir_not_exists_fn")

    @mark(['fattest', 'ds2test'])
    def test_fat_ds2_find_mixed_wild_plain(self):
        """FAT DOSv2 findnext intermixed wild plain"""
        ds2_find_mixed_wild_plain(self, "FAT")

    @mark(['mfstest', 'ds2test'])
    def test_mfs_ds2_find_mixed_wild_plain(self):
        """MFS DOSv2 findnext intermixed wild plain"""
        ds2_find_mixed_wild_plain(self, "MFS")

    def test_create_new_psp(self):
        """Create New PSP"""
        create_new_psp(self)

    @mark('serialtest')
    def test_serial_simple_read_echo(self):
        """Serial Simple Read Echo"""
        serial_simple_read_echo(self)

    @mark('serialtest')
    def test_serial_simple_write_file(self):
        """Serial Simple Write File"""
        serial_simple_write_file(self)

    # Obviously not a serial device, but may as well group together
    @mark('serialtest')
    def test_lpt_simple_write_pipe(self):
        """LPT Simple Write Pipe"""
        lpt_simple_write_pipe(self)

    def test_systype(self):
        """SysType"""
        systype(self)

    @mark('cmdtest')
    def test_command_com_cmdline_length_new_dos01(self):
        """Command.com cmdline length(127) new DOS (env var) 01"""
        command_com_cmdline_length(self, 'new_dos01')

    @mark('cmdtest')
    def test_command_com_cmdline_length_new_dos02(self):
        """Command.com cmdline length(150) new DOS (env var) 02"""
        command_com_cmdline_length(self, 'new_dos02')

    @mark('cmdtest')
    def test_command_com_cmdline_length_multiargs01(self):
        """Command.com cmdline length( 30) multiple args 01"""
        command_com_cmdline_length(self, 'multiarg01')

    @mark('cmdtest')
    def test_command_com_cmdline_length_singlearg01(self):
        """Command.com cmdline length( 60) single arg 01"""
        command_com_cmdline_length(self, 'singlearg01')

    @mark('cmdtest')
    def test_command_com_cmdline_length_singlearg02(self):
        """Command.com cmdline length(126) single arg 02"""
        command_com_cmdline_length(self, 'singlearg02')

    @mark('cmdtest')
    def test_command_com_cmdline_length_old_dos01(self):
        """Command.com cmdline length(127) old DOS (truncate) 01"""
        command_com_cmdline_length(self, 'old_dos01')

    @mark('cmdtest')
    def test_command_com_cmdline_length_old_dos02(self):
        """Command.com cmdline length(150) old DOS (truncate) 02"""
        command_com_cmdline_length(self, 'old_dos02')

    @mark('cmdtest')
    def test_command_com_command_copy(self):
        """Command.com command copy"""
        command_com_copy(self)

    @mark('cmdtest')
    def test_command_com_keyword_exist(self):
        """Command.com keyword exist"""
        command_com_keyword_exist(self)

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_ecm_alloc(self):
        """Memory DPMI (ECM) alloc"""
        memory_dpmi_ecm_alloc(self)

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_ecm_mini(self):
        """Memory DPMI (ECM) mini"""
        memory_dpmi_ecm_mini(self)

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_ecm_modeswitch(self):
        """Memory DPMI (ECM) mode switch"""
        memory_dpmi_ecm_modeswitch(self)

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_ecm_psp(self):
        """Memory DPMI (ECM) psp"""
        memory_dpmi_ecm_psp(self)

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_japheth(self):
        """Memory DPMI (Japheth) ''"""
        memory_dpmi_japheth(self, '')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_japheth_c(self):
        """Memory DPMI (Japheth) '-c'"""
        memory_dpmi_japheth(self, '-c')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_japheth_d(self):
        """Memory DPMI (Japheth) '-d'"""
        memory_dpmi_japheth(self, '-d')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_japheth_e(self):
        """Memory DPMI (Japheth) '-e'"""
        memory_dpmi_japheth(self, '-e')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_japheth_i(self):
        """Memory DPMI (Japheth) '-i'"""
        memory_dpmi_japheth(self, '-i')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_japheth_m(self):
        """Memory DPMI (Japheth) '-m'"""
        memory_dpmi_japheth(self, '-m')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_japheth_r(self):
        """Memory DPMI (Japheth) '-r'"""
        memory_dpmi_japheth(self, '-r')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_japheth_t(self):
        """Memory DPMI (Japheth) '-t'"""
        memory_dpmi_japheth(self, '-t')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_japheth_z(self):
        """Memory DPMI (Japheth) '-z'"""
        memory_dpmi_japheth(self, '-z')

    @mark(['memtest', 'emstest'])
    def test_memory_emm286_borland(self):
        """Memory EMM286 (Borland)"""
        memory_emm286_borland(self)

    @mark(['memtest', 'emstest'])
    def test_memory_ems_borland(self):
        """Memory EMS (Borland)"""
        memory_ems_borland(self)

    @mark(['memtest', 'hmatest'])
    def test_memory_hma_a20(self):
        """Memory HMA a20 toggle"""
        memory_hma_a20(self)

    @mark(['memtest', 'hmatest'])
    def test_memory_hma_alloc(self):
        """Memory HMA allocation"""
        memory_hma_alloc(self)

    @mark(['memtest', 'hmatest'])
    def test_memory_hma_alloc3(self):
        """Memory HMA alloc/resize/dealloc"""
        memory_hma_alloc3(self)

    @mark(['memtest', 'hmatest'])
    def test_memory_hma_freespace(self):
        """Memory HMA freespace"""
        memory_hma_freespace(self)

    @mark(['memtest', 'hmatest'])
    def test_memory_hma_chain(self):
        """Memory HMA get chain"""
        memory_hma_chain(self)

    @mark(['memtest', 'xmstest'])
    def test_memory_xms(self):
        """Memory XMS"""
        memory_xms(self)

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi10_ldt(self):
        """Memory DPMI-1.0 LDT"""
        memory_dpmi_dpmi10_ldt(self)

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_leak_check_nofree(self):
        """Memory DPMI Leak Check No Free"""
        memory_dpmi_leak_check(self, 'nofree')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_leak_check_normal(self):
        """Memory DPMI Leak Check Normal"""
        memory_dpmi_leak_check(self, 'normal')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_leak_check_dos_nofree(self):
        """Memory DPMI Leak Check DOS No Free"""
        memory_dpmi_leak_check_dos(self, 'nofree')

    @mark(['memtest', 'dpmitest'])
    def test_memory_dpmi_leak_check_dos_normal(self):
        """Memory DPMI Leak Check DOS Normal"""
        memory_dpmi_leak_check_dos(self, 'normal')

    @mark(['memtest', 'umatest'])
    def test_memory_uma_strategy(self):
        """Memory UMA Strategy"""
        memory_uma_strategy(self)

    def test_floppy_img(self):
        """Floppy image file"""
        floppy_img(self)

    def test_floppy_vfs(self):
        """Floppy vfs directory"""
        floppy_vfs(self)

    def test_three_drives_vfs(self):
        """Three vfs directories configured"""
        three_drives_vfs(self)

    @mark(['fattest', 'imgtest'])
    def test_fat12_img_d_writable(self):
        """FAT12 image file D writable"""
        fat_img_d_writable(self, "12")

    @mark(['fattest', 'imgtest'])
    def test_fat16_img_d_writable(self):
        """FAT16 image file D writable"""
        fat_img_d_writable(self, "16")

    @mark(['fattest', 'imgtest'])
    def test_fat16b_img_d_writable(self):
        """FAT16B image file D writable"""
        fat_img_d_writable(self, "16b")

    @mark(['fattest', 'imgtest'])
    def test_fat32_img_d_writable(self):
        """FAT32 image file D writable"""
        fat_img_d_writable(self, "32")

    @mark('mfstest')
    def test_mfs_lredir_auto_hdc(self):
        """MFS lredir auto C drive redirection"""
        mfs_lredir_auto_hdc(self)

    @mark('mfstest')
    def test_mfs_lredir_command(self):
        """MFS lredir command redirection"""
        mfs_lredir_command(self)

    @mark('mfstest')
    def test_mfs_lredir_command_no_perm(self):
        """MFS lredir command redirection permission fail"""
        mfs_lredir_command_no_perm(self)

    @mark(['mfstest', 'lfntest'])
    def test_mfs_findfile_ufs_lfn(self):
        """MFS findfile UFS LFN"""
        mfs_findfile_ufs_lfn(self)

    @mark(['mfstest', 'sfntest'])
    def test_mfs_findfile_ufs_sfn(self):
        """MFS findfile UFS SFN"""
        mfs_findfile_ufs_sfn(self)

    @mark(['mfstest', 'lfntest'])
    def test_mfs_findfile_vfat_linux_mounted_lfn(self):
        """MFS findfile VFAT Linux mounted LFN"""
        mfs_findfile_vfat_linux_mounted_lfn(self)

    @mark(['mfstest', 'sfntest'])
    def test_mfs_findfile_vfat_linux_mounted_sfn(self):
        """MFS findfile VFAT Linux mounted SFN"""
        mfs_findfile_vfat_linux_mounted_sfn(self)

    @mark('sfntest')
    def test_sfn_findfirst(self):
        """SFN findfile devices, files, directories"""
        sfn_findfirst(self)

    @mark(['mfstest', 'lfntest'])
    def test_mfs_truename_ufs_lfn(self):
        """MFS truename UFS LFN"""
        mfs_truename_ufs_lfn(self)

    @mark(['mfstest', 'sfntest'])
    def test_mfs_truename_ufs_sfn(self):
        """MFS truename UFS SFN"""
        mfs_truename_ufs_sfn(self)

    @mark(['mfstest', 'lfntest'])
    def test_mfs_truename_vfat_linux_mounted_lfn(self):
        """MFS truename VFAT Linux mounted LFN"""
        mfs_truename_vfat_linux_mounted_lfn(self)

    @mark(['mfstest', 'sfntest'])
    def test_mfs_truename_vfat_linux_mounted_sfn(self):
        """MFS truename VFAT Linux mounted SFN"""
        mfs_truename_vfat_linux_mounted_sfn(self)

    @mark('sfntest')
    def test_sfn_truename(self):
        """SFN truename devices"""
        sfn_truename(self)

    @mark(['mfstest', 'lfntest'])
    def test_mfs_lfn_file_read(self):
        """MFS LFN file read"""
        mfs_file_read(self, "LFN")

    @mark(['mfstest', 'sfntest'])
    def test_mfs_sfn_file_read(self):
        """MFS SFN file read"""
        mfs_file_read(self, "SFN")

    @mark(['mfstest', 'lfntest'])
    def test_mfs_lfn_file_create(self):
        """MFS LFN file create"""
        mfs_file_write(self, "LFN", "create")

    @mark(['mfstest', 'sfntest'])
    def test_mfs_sfn_file_create(self):
        """MFS SFN file create"""
        mfs_file_write(self, "SFN", "create")

    @mark(['mfstest', 'lfntest'])
    def test_mfs_lfn_file_create_readonly(self):
        """MFS LFN file create readonly"""
        mfs_file_write(self, "LFN", "createreadonly")

    @mark(['mfstest', 'sfntest'])
    def test_mfs_sfn_file_create_readonly(self):
        """MFS SFN file create readonly"""
        mfs_file_write(self, "SFN", "createreadonly")

    @mark(['mfstest', 'lfntest'])
    def test_mfs_lfn_file_truncate(self):
        """MFS LFN file truncate"""
        mfs_file_write(self, "LFN", "truncate")

    @mark(['mfstest', 'sfntest'])
    def test_mfs_sfn_file_truncate(self):
        """MFS SFN file truncate"""
        mfs_file_write(self, "SFN", "truncate")

    @mark(['mfstest', 'lfntest'])
    def test_mfs_lfn_file_append(self):
        """MFS LFN file append"""
        mfs_file_write(self, "LFN", "append")

    @mark(['mfstest', 'sfntest'])
    def test_mfs_sfn_file_append(self):
        """MFS SFN file append"""
        mfs_file_write(self, "SFN", "append")

    def test_lfn_volume_info_mfs(self):
        """LFN volume info on MFS"""
        lfn_voln_info(self, "MFS")

    def test_lfn_volume_info_fat16(self):
        """LFN volume info on FAT16"""
        lfn_voln_info(self, "FAT16")

    def test_lfn_volume_info_fat32(self):
        """LFN volume info on FAT32"""
        lfn_voln_info(self, "FAT32")

    def test_int21_disk_info(self):
        """INT21 disk info"""
        int21_disk_info(self)

    def test_lfs_disk_info_fat32(self):
        """LFS disk info FAT32"""
        lfs_disk_info(self, "FAT32")

    def test_lfs_disk_info_mfs(self):
        """LFS disk info MFS"""
        lfs_disk_info(self, "MFS")

    def test_mfs_lfs_file_info_1MiB(self):
        """MFS LFS file info (1 MiB)"""
        lfs_file_info(self, "MFS", "1MiB")

    def test_mfs_lfs_file_info_6GiB(self):
        """MFS LFS file info (6 GiB)"""
        lfs_file_info(self, "MFS", "6GiB")

    def test_mfs_lfs_file_seek_tell_set(self):
        """MFS LFS file seek tell set"""
        lfs_file_seek_tell(self, "MFS", "SET")

    def test_mfs_lfs_file_seek_tell_cur(self):
        """MFS LFS file seek tell current"""
        lfs_file_seek_tell(self, "MFS", "CUR")

    def test_mfs_lfs_file_seek_tell_end(self):
        """MFS LFS file seek tell end"""
        lfs_file_seek_tell(self, "MFS", "END")

    def test_mfs_ds2_get_ftime(self):
        """MFS DOSv2 get file time"""
        ds2_get_ftime(self, "MFS", "DATE")
        ds2_get_ftime(self, "MFS", "TIME")

    def test_fat_ds2_get_ftime(self):
        """FAT DOSv2 get file time"""
        # Note: we need to split this test as FAT cannot have enough files
        #       in the root directory, and mkimage can't store them in a
        #       subdirectory.
        ds2_get_ftime(self, "FAT", "DATE")
        ds2_get_ftime(self, "FAT", "TIME")

    def test_mfs_ds2_set_ftime(self):
        """MFS DOSv2 set file time"""
        ds2_set_ftime(self, "MFS")

    def test_fat_ds2_set_ftime(self):
        """FAT DOSv2 set file time"""
        ds2_set_ftime(self, "FAT")

    def test_fat_ds2_set_fattr_rdonly(self):
        """FAT DOSv2 set file attr RDONLY"""
        ds2_set_fattrs(self, "FAT", "RDONLY")

    def test_fat_ds2_set_fattr_hidden(self):
        """FAT DOSv2 set file attr HIDDEN"""
        ds2_set_fattrs(self, "FAT", "HIDDEN")

    def test_fat_ds2_set_fattr_system(self):
        """FAT DOSv2 set file attr SYSTEM"""
        ds2_set_fattrs(self, "FAT", "SYSTEM")

    def test_mfs_ds2_set_fattr_rdonly(self):
        """MFS DOSv2 set file attr RDONLY"""
        ds2_set_fattrs(self, "MFS", "RDONLY")

    def test_mfs_ds2_set_fattr_hidden(self):
        """MFS DOSv2 set file attr HIDDEN"""
        ds2_set_fattrs(self, "MFS", "HIDDEN")

    def test_mfs_ds2_set_fattr_system(self):
        """MFS DOSv2 set file attr SYSTEM"""
        ds2_set_fattrs(self, "MFS", "SYSTEM")

    def test_fat_ds3_file_access_read(self):
        """FAT DOSv3 file access read"""
        ds3_file_access(self, "FAT", "READ")

    def test_mfs_ds3_file_access_read(self):
        """MFS DOSv3 file access read"""
        ds3_file_access(self, "MFS", "READ")

    def test_fat_ds3_file_access_write(self):
        """FAT DOSv3 file access write"""
        ds3_file_access(self, "FAT", "WRITE")

    def test_mfs_ds3_file_access_write(self):
        """MFS DOSv3 file access write"""
        ds3_file_access(self, "MFS", "WRITE")

    def test_fat_ds3_file_access_read_device_readonly(self):
        """FAT DOSv3 file access read device readonly"""
        ds3_file_access(self, "FATRO", "READ")

    def test_mfs_ds3_file_access_read_device_readonly(self):
        """MFS DOSv3 file access read device readonly"""
        ds3_file_access(self, "MFSRO", "READ")

    def test_fat_ds3_file_access_write_device_readonly(self):
        """FAT DOSv3 file access write device readonly"""
        ds3_file_access(self, "FATRO", "WRITE")

    def test_mfs_ds3_file_access_write_device_readonly(self):
        """MFS DOSv3 file access write device readonly"""
        ds3_file_access(self, "MFSRO", "WRITE")

#    def test_mfs_ds3_lock_readonly(self):
#        """MFS DOSv3 lock file readonly"""
#        ds3_lock_readonly(self, "MFS")

    def test_fat_ds3_lock_readonly(self):
        """FAT DOSv3 lock file readonly"""
        ds3_lock_readonly(self, "FAT")

    def test_mfs_ds3_lock_readlckd(self):
        """MFS DOSv3 lock file read locked"""
        ds3_lock_readlckd(self, "MFS")

    def test_fat_ds3_lock_readlckd(self):
        """FAT DOSv3 lock file read locked"""
        ds3_lock_readlckd(self, "FAT")

    def test_mfs_ds3_lock_concurrent(self):
        """MFS DOSv3 lock file lock concurrent limit"""
        ds3_lock_concurrent(self, "MFS")

    def test_fat_ds3_lock_concurrent(self):
        """FAT DOSv3 lock file lock concurrent limit"""
        ds3_lock_concurrent(self, "FAT")

    def test_mfs_ds3_lock_two_handles(self):
        """MFS DOSv3 lock file lock with two handles"""
        ds3_lock_two_handles(self, "MFS")

    def test_fat_ds3_lock_two_handles(self):
        """FAT DOSv3 lock file lock with two handles"""
        ds3_lock_two_handles(self, "FAT")

    def test_mfs_ds3_lock_twice(self):
        """MFS DOSv3 lock file twice"""
        ds3_lock_twice(self, "MFS")

    def test_fat_ds3_lock_twice(self):
        """FAT DOSv3 lock file twice"""
        ds3_lock_twice(self, "FAT")

    def test_mfs_ds3_lock_writable(self):
        """MFS DOSv3 lock file writable"""
        ds3_lock_writable(self, "MFS")

    def test_fat_ds3_lock_writable(self):
        """FAT DOSv3 lock file writable"""
        ds3_lock_writable(self, "FAT")

    def test_mfs_ds3_share_open_twice(self):
        """MFS DOSv3 share open twice"""
        ds3_share_open_twice(self, "MFS")

    def test_fat_ds3_share_open_twice(self):
        """FAT DOSv3 share open twice"""
        ds3_share_open_twice(self, "FAT")

    def test_mfs_ds3_share_open_delete_one_process_ds2(self):
        """MFS DOSv3 share open delete one process DOSv2"""
        ds3_share_open_access(self, "ONE", "MFS", "DELPTH")

    def test_fat_ds3_share_open_delete_one_process_ds2(self):
        """FAT DOSv3 share open delete one process DOSv2"""
        ds3_share_open_access(self, "ONE", "FAT", "DELPTH")

    def test_mfs_ds3_share_open_delete_one_process_fcb(self):
        """MFS DOSv3 share open delete one process FCB"""
        ds3_share_open_access(self, "ONE", "MFS", "DELFCB")

    def test_fat_ds3_share_open_delete_one_process_fcb(self):
        """FAT DOSv3 share open delete one process FCB"""
        ds3_share_open_access(self, "ONE", "FAT", "DELFCB")

    def test_mfs_ds3_share_open_rename_one_process_ds2(self):
        """MFS DOSv3 share open rename one process DOSv2"""
        ds3_share_open_access(self, "ONE", "MFS", "RENPTH")

    def test_fat_ds3_share_open_rename_one_process_ds2(self):
        """FAT DOSv3 share open rename one process DOSv2"""
        ds3_share_open_access(self, "ONE", "FAT", "RENPTH")

    def test_mfs_ds3_share_open_rename_one_process_fcb(self):
        """MFS DOSv3 share open rename one process FCB"""
        ds3_share_open_access(self, "ONE", "MFS", "RENFCB")

    def test_fat_ds3_share_open_rename_one_process_fcb(self):
        """FAT DOSv3 share open rename one process FCB"""
        ds3_share_open_access(self, "ONE", "FAT", "RENFCB")

    def test_mfs_ds3_share_open_setfattrs_one_process(self):
        """MFS DOSv3 share open set file attrs one process DOSv2"""
        ds3_share_open_access(self, "ONE", "MFS", "SETATT")

    def test_fat_ds3_share_open_setfattrs_one_process(self):
        """FAT DOSv3 share open set file attrs one process DOSv2"""
        ds3_share_open_access(self, "ONE", "FAT", "SETATT")

    def test_mfs_ds3_share_open_delete_two_process_ds2(self):
        """MFS DOSv3 share open delete two process DOSv2"""
        ds3_share_open_access(self, "TWO", "MFS", "DELPTH")

    def test_fat_ds3_share_open_delete_two_process_ds2(self):
        """FAT DOSv3 share open delete two process DOSv2"""
        ds3_share_open_access(self, "TWO", "FAT", "DELPTH")

    def test_mfs_ds3_share_open_delete_two_process_fcb(self):
        """MFS DOSv3 share open delete two process FCB"""
        ds3_share_open_access(self, "TWO", "MFS", "DELFCB")

    def test_fat_ds3_share_open_delete_two_process_fcb(self):
        """FAT DOSv3 share open delete two process FCB"""
        ds3_share_open_access(self, "TWO", "FAT", "DELFCB")

    def test_mfs_ds3_share_open_rename_two_process_ds2(self):
        """MFS DOSv3 share open rename two process DOSv2"""
        ds3_share_open_access(self, "TWO", "MFS", "RENPTH")

    def test_fat_ds3_share_open_rename_two_process_ds2(self):
        """FAT DOSv3 share open rename two process DOSv2"""
        ds3_share_open_access(self, "TWO", "FAT", "RENPTH")

    def test_mfs_ds3_share_open_rename_two_process_fcb(self):
        """MFS DOSv3 share open rename two process FCB"""
        ds3_share_open_access(self, "TWO", "MFS", "RENFCB")

    def test_fat_ds3_share_open_rename_two_process_fcb(self):
        """FAT DOSv3 share open rename two process FCB"""
        ds3_share_open_access(self, "TWO", "FAT", "RENFCB")

    def test_mfs_ds3_share_open_setfattrs_two_process(self):
        """MFS DOSv3 share open set file attrs two process DOSv2"""
        ds3_share_open_access(self, "TWO", "MFS", "SETATT")

    def test_fat_ds3_share_open_setfattrs_two_process(self):
        """FAT DOSv3 share open set file attrs two process DOSv2"""
        ds3_share_open_access(self, "TWO", "FAT", "SETATT")

    @mark('nettest')
    def test_network_pktdriver_mtcp_builtin(self):
        """Network pktdriver mTCP built-in"""
        network_pktdriver_mtcp(self, 'builtin')

    @mark('nettest')
    def test_network_pktdriver_mtcp_ne2000(self):
        """Network pktdriver mTCP NE2000"""
        network_pktdriver_mtcp(self, 'ne2000')

    def test_passing_environment_variable(self):
        """Passing Environment Variable to DOS"""
        passing_environment_variable(self)

    def test_passing_dos_errorlevel_back(self):
        """Passing DOS Errorlevel back"""
        passing_dos_errorlevel_back(self)

    @acceptFailure
    def test_pit_mode_2(self):
        """PIT Mode 2"""
        pit_mode_2(self)

DRDOS701TestCase = drdos701(OurTestCase, {
    "test_command_com_r200fix_real": UNSUPPORTED,
    "test_command_com_r200fix_protected": UNSUPPORTED,
    "test_command_com_cmdline_length_new_dos01": UNSUPPORTED,
    "test_command_com_cmdline_length_new_dos02": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos01": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos02": UNSUPPORTED,
    r"test_fat_ds3_share_open_setfattrs_(one|two)_process": KNOWNFAIL,
    r"test_..._ds3_share_open_rename_one_process_fcb": KNOWNFAIL,
    r"test_..._fcb_rename_simple": KNOWNFAIL,
    r"test_..._fcb_rename_wild_\d": KNOWNFAIL,
    "test_mfs_truename_ufs_sfn": KNOWNFAIL,
    "test_mfs_truename_vfat_linux_mounted_sfn": KNOWNFAIL,
    "test_fat32_img_d_writable": UNSUPPORTED,
    "test_fat_bpb_set_fstype_ioctl32": UNSUPPORTED,
    "test_fat_bpb_set_serial_ioctl32": UNSUPPORTED,
    "test_fat_bpb_set_volume_ioctl32": UNSUPPORTED,
    "test_lfn_volume_info_fat16": KNOWNFAIL,
    "test_lfn_volume_info_fat32": UNSUPPORTED,
    "test_lfn_volume_info_mfs": KNOWNFAIL,
    "test_lfs_disk_info_fat32": UNSUPPORTED,
    "test_floppy_vfs": KNOWNFAIL,
    "test_memory_hma_alloc3": UNSUPPORTED,
    "test_memory_hma_chain": UNSUPPORTED,
    "test_pcmos_build": KNOWNFAIL,
    "test_passing_dos_errorlevel_back": KNOWNFAIL,
    "test_fat_label_create_bpb12": KNOWNFAIL,
    "test_fat_label_create_bpb16": KNOWNFAIL,
    "test_fat_label_create_bpb32": UNSUPPORTED,
    "test_fat_label_create_on_lfns": UNSUPPORTED,
    "test_fat_label_create_nonrootdir": KNOWNFAIL,
    "test_fat_label_delete_recreate": KNOWNFAIL,
    "test_fat_label_delete_wildcard": KNOWNFAIL,
    "test_sfn_truename": KNOWNFAIL,
    "test_sfn_findfirst": KNOWNFAIL,
    "test_libi86_item_104": KNOWNFAIL,
})

FRDOS120TestCase = frdos120(OurTestCase, {
    "test_command_com_r200fix_real": UNSUPPORTED,
    "test_command_com_r200fix_protected": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos01": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos02": UNSUPPORTED,
    "test_drv_removable": KNOWNFAIL,
    "test_fat_bpb_set_fstype_dinfo": KNOWNFAIL,
    "test_fat_bpb_set_fstype_ioctl16": KNOWNFAIL,
    "test_fat_bpb_set_fstype_ioctl32": KNOWNFAIL,
    "test_fat_bpb_set_serial_dinfo": KNOWNFAIL,
    "test_fat_bpb_set_serial_ioctl16": KNOWNFAIL,
    "test_fat_bpb_set_serial_ioctl32": KNOWNFAIL,
    "test_fat_bpb_set_volume_dinfo": KNOWNFAIL,
    "test_fat_bpb_set_volume_ioctl16": KNOWNFAIL,
    "test_fat_bpb_set_volume_ioctl32": KNOWNFAIL,
    "test_fat_fcb_rename_target_exists": KNOWNFAIL,
    "test_fat_fcb_rename_source_missing": KNOWNFAIL,
    "test_fat_fcb_rename_wild_1": KNOWNFAIL,
    "test_fat_fcb_rename_wild_2": KNOWNFAIL,
    "test_fat_fcb_rename_wild_3": KNOWNFAIL,
    "test_mfs_fcb_rename_target_exists": KNOWNFAIL,
    "test_mfs_fcb_rename_source_missing": KNOWNFAIL,
    "test_mfs_fcb_rename_wild_1": KNOWNFAIL,
    "test_mfs_fcb_rename_wild_2": KNOWNFAIL,
    "test_mfs_fcb_rename_wild_3": KNOWNFAIL,
    "test_mfs_fcb_rename_wild_4": KNOWNFAIL,
    "test_fat_fcb_find_wild_1": KNOWNFAIL,
    "test_fat_fcb_find_wild_2": KNOWNFAIL,
    "test_fat_fcb_find_wild_3": KNOWNFAIL,
    "test_mfs_fcb_find_wild_1": KNOWNFAIL,
    "test_mfs_fcb_find_wild_2": KNOWNFAIL,
    "test_mfs_fcb_find_wild_3": KNOWNFAIL,
    "test_mfs_lfs_file_info_1MiB": KNOWNFAIL,
    "test_mfs_lfs_file_info_6GiB": KNOWNFAIL,
    "test_mfs_lfs_file_seek_tell_set": KNOWNFAIL,
    "test_mfs_lfs_file_seek_tell_cur": KNOWNFAIL,
    "test_mfs_lfs_file_seek_tell_end": KNOWNFAIL,
    "test_mfs_lredir_command": KNOWNFAIL,
    "test_mfs_lredir_command_no_perm": KNOWNFAIL,
    "test_fat_ds3_lock_writable": KNOWNFAIL,
    "test_fat_ds3_lock_readlckd": KNOWNFAIL,
    "test_fat_ds3_lock_two_handles": KNOWNFAIL,
    "test_lfs_disk_info_fat32": KNOWNFAIL,
    "test_lfs_disk_info_mfs": KNOWNFAIL,
    "test_mfs_truename_vfat_linux_mounted_lfn": KNOWNFAIL,
    "test_mfs_truename_vfat_linux_mounted_sfn": KNOWNFAIL,
    "test_mfs_findfile_vfat_linux_mounted_lfn": KNOWNFAIL,
    "test_mfs_findfile_vfat_linux_mounted_sfn": KNOWNFAIL,
    "test_fat_ds3_share_open_twice": KNOWNFAIL,
    r"test_fat_ds3_share_open_(delete|rename)_.*": KNOWNFAIL,
    r"test_mfs_ds3_share_open_rename_(one|two)_process_fcb": KNOWNFAIL,
    r"test_fat_ds3_share_open_setfattrs_(one|two)_process": KNOWNFAIL,
    "test_create_new_psp": KNOWNFAIL,
    "test_command_com_keyword_exist": KNOWNFAIL,
    "test_memory_emm286_borland": KNOWNFAIL,
    "test_memory_hma_alloc": KNOWNFAIL,
    "test_memory_hma_alloc3": UNSUPPORTED,
    "test_memory_hma_chain": UNSUPPORTED,
    "test_memory_uma_strategy": KNOWNFAIL,
    "test_pcmos_build": KNOWNFAIL,
    r"test_libi86_item_\d+": KNOWNFAIL,
    "test_passing_dos_errorlevel_back": KNOWNFAIL,
    "test_fat_label_create_bpb12": KNOWNFAIL,
    "test_fat_label_create_bpb16": KNOWNFAIL,
    "test_fat_label_create_bpb32": KNOWNFAIL,
    "test_fat_label_create_noduplicate": KNOWNFAIL,
    "test_fat_label_create_nonrootdir": KNOWNFAIL,
    "test_fat_label_create_prefile": KNOWNFAIL,
    "test_fat_label_create_predir": KNOWNFAIL,
    "test_sfn_truename": KNOWNFAIL,
    "test_sfn_findfirst": KNOWNFAIL,
})

FRDOS130TestCase = frdos130(OurTestCase, {
    "test_command_com_r200fix_real": UNSUPPORTED,
    "test_command_com_r200fix_protected": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos01": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos02": UNSUPPORTED,
    "test_command_com_keyword_exist": KNOWNFAIL,
    "test_create_new_psp": KNOWNFAIL,
    "test_drv_removable": KNOWNFAIL,
    "test_fat_bpb_set_fstype_dinfo": KNOWNFAIL,
    "test_fat_bpb_set_fstype_ioctl16": KNOWNFAIL,
    "test_fat_bpb_set_fstype_ioctl32": KNOWNFAIL,
    "test_fat_bpb_set_serial_dinfo": KNOWNFAIL,
    "test_fat_bpb_set_serial_ioctl16": KNOWNFAIL,
    "test_fat_bpb_set_serial_ioctl32": KNOWNFAIL,
    "test_fat_bpb_set_volume_dinfo": KNOWNFAIL,
    "test_fat_bpb_set_volume_ioctl16": KNOWNFAIL,
    "test_fat_bpb_set_volume_ioctl32": KNOWNFAIL,
    "test_fat_ds3_lock_readlckd": KNOWNFAIL,
    "test_fat_ds3_lock_two_handles": KNOWNFAIL,
    "test_fat_ds3_lock_writable": KNOWNFAIL,
    r"test_fat_ds3_share_open_(delete|rename)_.*": KNOWNFAIL,
    r"test_fat_ds3_share_open_setfattrs_(one|two)_process": KNOWNFAIL,
    "test_fat_ds3_share_open_twice": KNOWNFAIL,
    "test_fat_fcb_find_wild_1": KNOWNFAIL,
    "test_fat_fcb_find_wild_2": KNOWNFAIL,
    "test_fat_fcb_find_wild_3": KNOWNFAIL,
    "test_fat_fcb_rename_wild_1": KNOWNFAIL,
    "test_fat_fcb_rename_wild_2": KNOWNFAIL,
    "test_fat_fcb_rename_wild_3": KNOWNFAIL,
    "test_fat_fcb_rename_wild_4": KNOWNFAIL,
    "test_fat_label_create_bpb12": KNOWNFAIL,
    "test_fat_label_create_bpb16": KNOWNFAIL,
    "test_fat_label_create_bpb32": KNOWNFAIL,
    "test_fat_label_create_noduplicate": KNOWNFAIL,
    "test_fat_label_create_nonrootdir": KNOWNFAIL,
    "test_fat_label_create_predir": KNOWNFAIL,
    "test_fat_label_create_prefile": KNOWNFAIL,
    "test_lfs_disk_info_fat32": KNOWNFAIL,
    "test_lfs_disk_info_mfs": KNOWNFAIL,
    "test_memory_emm286_borland": KNOWNFAIL,
    "test_memory_hma_alloc": KNOWNFAIL,
    "test_memory_hma_alloc3": UNSUPPORTED,
    "test_memory_hma_chain": UNSUPPORTED,
    "test_memory_uma_strategy": KNOWNFAIL,
    "test_mfs_fcb_rename_wild_1": KNOWNFAIL,
    "test_mfs_fcb_rename_wild_2": KNOWNFAIL,
    "test_mfs_fcb_rename_wild_3": KNOWNFAIL,
    "test_mfs_fcb_rename_wild_4": KNOWNFAIL,
    "test_passing_dos_errorlevel_back": KNOWNFAIL,
    "test_serial_simple_read_echo": KNOWNFAIL,
    "test_sfn_truename": KNOWNFAIL,
    "test_sfn_findfirst": KNOWNFAIL,
    "test_libi86_item_056": KNOWNFAIL,
    "test_libi86_item_104": KNOWNFAIL,
})

FRDOSGITTestCase = frdosgit(OurTestCase, {
    "test_command_com_r200fix_real": UNSUPPORTED,
    "test_command_com_r200fix_protected": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos01": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos02": UNSUPPORTED,
    "test_fat_bpb_set_fstype_dinfo": KNOWNFAIL,
    "test_fat_bpb_set_fstype_ioctl16": KNOWNFAIL,
    "test_fat_bpb_set_fstype_ioctl32": KNOWNFAIL,
    "test_fat_ds3_lock_concurrent": KNOWNFAIL,
    "test_fat_ds3_lock_readlckd": KNOWNFAIL,
    "test_fat_ds3_lock_two_handles": KNOWNFAIL,
    "test_fat_ds3_lock_writable": KNOWNFAIL,
    "test_fat_ds3_share_open_delete_one_process_ds2": KNOWNFAIL,
    "test_fat_ds3_share_open_delete_one_process_fcb": KNOWNFAIL,
    "test_fat_ds3_share_open_rename_one_process_ds2": KNOWNFAIL,
    "test_fat_ds3_share_open_rename_one_process_fcb": KNOWNFAIL,
    "test_fat_ds3_share_open_setfattrs_one_process": KNOWNFAIL,
    "test_fat_ds3_share_open_twice": KNOWNFAIL,
    "test_fat_label_create_bpb12": KNOWNFAIL,
    "test_fat_label_create_bpb16": KNOWNFAIL,
    "test_fat_label_create_bpb32": KNOWNFAIL,
    "test_fat_label_create_noduplicate": KNOWNFAIL,
    "test_fat_label_create_nonrootdir": KNOWNFAIL,
    "test_fat_label_create_predir": KNOWNFAIL,
    "test_fat_label_create_prefile": KNOWNFAIL,
    "test_memory_emm286_borland": KNOWNFAIL,
    "test_memory_hma_alloc3": UNSUPPORTED,
    "test_memory_hma_chain": UNSUPPORTED,
    "test_memory_uma_strategy": KNOWNFAIL,
    "test_passing_dos_errorlevel_back": KNOWNFAIL,
    "test_serial_simple_read_echo": KNOWNFAIL,
})

MSDOS622TestCase = msdos622(OurTestCase, {
    "test_command_com_r200fix_real": UNSUPPORTED,
    "test_command_com_r200fix_protected": UNSUPPORTED,
    "test_command_com_cmdline_length_new_dos01": UNSUPPORTED,
    "test_command_com_cmdline_length_new_dos02": UNSUPPORTED,
    "test_fat32_img_d_writable": UNSUPPORTED,
    "test_fat_bpb_set_fstype_ioctl32": UNSUPPORTED,
    "test_fat_bpb_set_serial_ioctl32": UNSUPPORTED,
    "test_fat_bpb_set_volume_ioctl32": UNSUPPORTED,
    "test_lfn_volume_info_fat16": KNOWNFAIL,
    "test_lfn_volume_info_fat32": UNSUPPORTED,
    "test_lfs_disk_info_fat32": UNSUPPORTED,
    "test_memory_hma_alloc3": UNSUPPORTED,
    "test_memory_hma_chain": UNSUPPORTED,
    "test_passing_dos_errorlevel_back": KNOWNFAIL,
    "test_fat_label_create_bpb32": UNSUPPORTED,
    "test_fat_label_create_on_lfns": UNSUPPORTED,
    "test_libi86_item_104": KNOWNFAIL,
})

MSDOS700TestCase = msdos700(OurTestCase, {
    "test_command_com_r200fix_real": UNSUPPORTED,
    "test_command_com_r200fix_protected": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos01": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos02": UNSUPPORTED,
    "test_fat32_img_d_writable": UNSUPPORTED,
    "test_fat_ds3_share_open_twice": UNSUPPORTED,
    "test_fat_label_create_bpb32": UNSUPPORTED,
    "test_lfn_volume_info_fat16": KNOWNFAIL,
    "test_lfn_volume_info_fat32": UNSUPPORTED,
    "test_lfs_disk_info_fat32": UNSUPPORTED,
    "test_lfs_disk_info_mfs": KNOWNFAIL,
})

MSDOS710TestCase = msdos710(OurTestCase, {
    "test_command_com_r200fix_real": UNSUPPORTED,
    "test_command_com_r200fix_protected": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos01": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos02": UNSUPPORTED,
    "test_fat_ds3_share_open_twice": UNSUPPORTED,
})

PPDOSGITTestCase = ppdosgit(OurTestCase, {
    "test_command_com_r200fix_protected": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos01": UNSUPPORTED,
    "test_command_com_cmdline_length_old_dos02": UNSUPPORTED,
    "test_drv_removable": KNOWNFAIL,
    "test_fat_bpb_set_fstype_dinfo": UNSUPPORTED,
    "test_fat_bpb_set_fstype_ioctl16": UNSUPPORTED,
    "test_fat_bpb_set_fstype_ioctl32": UNSUPPORTED,
    "test_fat_bpb_set_volume_dinfo": UNSUPPORTED,
    "test_fat_bpb_set_volume_ioctl16": UNSUPPORTED,
    "test_fat_bpb_set_volume_ioctl32": UNSUPPORTED,
    "test_floppy_img": UNSUPPORTED,
    "test_floppy_vfs": UNSUPPORTED,
})

if __name__ == '__main__':

    # Dynamically create tests
    is_libi86 = False
    specific = False
    for arg in argv[1:]:
        if 'test_' in arg:
            specific = True
            if 'test_libi86_item' in arg:
                is_libi86 = True
    if not specific or is_libi86:
        libi86_create_items(OurTestCase)

    cases = [
        PPDOSGITTestCase,
        MSDOS622TestCase,
        DRDOS701TestCase,
        FRDOS120TestCase,
        FRDOS130TestCase,
        FRDOSGITTestCase,
        MSDOS700TestCase,
        MSDOS710TestCase,
    ]
    xargv = main_setup(cases)
    main(xargv)
