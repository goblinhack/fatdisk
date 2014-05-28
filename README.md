fatdisk
=======

fatdisk is a utility that allows you to perform various operations on files
on a DOS formatted disk image in FAT12,16,32 formats without needing to do any
mounting of the disk image, or needing root or sudo access.

It can extract files from the DOS disk to the local harddrive, and likewise
can import files from the local disk back onto the DOS disk. Additionally
you can do basic operations like list, cat, hexdump etc...

Lastly this tool can also format and partition a disk, setting up the FAT
filesystem and even copying in a bootloader like grub. This is a bit 
experimental so use with care.

You may specify the partition of the disk the tool is to look for, but it
will default to partition 0 if not. And if no partition info is found, it
will do a hunt of the disk to try and find it.

Usage: fatdisk [OPTIONS] disk-image-file [COMMAND]

COMMAND are things like extract, rm, ls, add, info, summary, format.
Options:
        --verbose        : print lots of disk info
        -verbose         :
        -v               :

        --quiet          : print less info when adding/removing
        -quiet           :
        -q               :

        --debug          : print internal debug info
        -debug           :
        -d               :

        --offset         : offset to start of disk
        -offset          : e.g. -o 32256, -o 63s -o 0x7e00
        -o               :

        --partition      : partition to use
        -partition       :
        -p               :

        --sectors_per_cluster : sectors per cluster
        -sectors_per_cluster  :
        -S               :

        --sectorsize     : default 512
        -sectorsize      :

        --help           : this help
        -help            :
        -h               :

        --version        : tool version number
        -version         :
        -ver             :

Commands:

        info             : print disk info
        i                :

        summary          : print disk info summary
        sum              :
        s                :

        list      <pat>  : list a file or dir
        ls        <pat>  :
        l         <pat>  :

        find      <pat>  : find and raw list files
        fi        <pat>  :
        f         <pat>  :

        extract   <pat>  : extract a file or dir
        x         <pat>  :

        add       <pat>  : add a file or dir, keeping same
        a         <pat>  : full pathname on the disk image

        fileadd   local-name [remote-name]
                         : add a file with a different name from source
        f         <pat>  :

        remove    <pat>  : remove a file or dir
        rm        <pat>  :
        r         <pat>  :

        hexdump          : include hex dump of files
        hex              :
        h                :

        cat              : raw dump of file to console
        ca               :
        c                :

        format
               size xG/xM
               [part 0-3]           select partiton
               [zero]               zero sectors
               [bootloader <file>]  install bootloader
               [<disktype>]         select filesys type
               where <disktype> is: Empty FAT12 XENIX-root XENIX-usr Small-FAT16 Extended FAT16 HPFS/NTFS AIX AIX-bootable OS/2-boot-mgr FAT32 FAT32-LBA FAT16-LBA Extended-LBA OPUS Hidden-FAT12 Compaq-diag Hidd-Sm-FAT16 Hidd-FAT16 Hidd-HPFS/NTFS AST-SmartSleep Hidd-FAT32 Hidd-FAT32-LBA Hidd-FAT16-LBA NEC-DOS Plan-9 PMagic-recovery Venix-80286 PPC-PReP-Boot SFS QNX4.x QNX4.x-2nd-part QNX4.x-3rd-part OnTrack-DM OnTrackDM6-Aux1 CP/M OnTrackDM6-Aux3 OnTrack-DM6 EZ-Drive Golden-Bow Priam-Edisk SpeedStor GNU-HURD/SysV Netware-286 Netware-386 DiskSec-MltBoot PC/IX Minix-<1.4a Minix->1.4b Linux-swap Linux OS/2-hidden-C: Linux-extended NTFS-volume-set NTFS-volume-set Linux-plaintext Linux-LVM Amoeba Amoeba-BBT BSD/OS Thinkpad-hib FreeBSD OpenBSD NeXTSTEP Darwin-UFS NetBSD Darwin-boot BSDI-fs BSDI-swap Boot-Wizard-Hid Solaris-boot Solaris DRDOS/2-FAT12 DRDOS/2-smFAT16 DRDOS/2-FAT16 Syrinx Non-FS-data CP/M/CTOS Dell-Utility BootIt DOS-access DOS-R/O SpeedStor BeOS-fs EFI-GPT EFI-FAT Lnx/PA-RISC-bt SpeedStor DOS-secondary SpeedStor Lnx-RAID-auto LANstep 

Examples:

  $ fatdisk mybootdisk ls
    ----daD            0       2013 Jan 02   locale/             LOCALE
    -----aD        18573       2013 Jan 02    ast.mo             AST.MO
    ...

  $ fatdisk mybootdisk info

  $ fatdisk mybootdisk summary

  $ fatdisk mybootdisk extract dir
					-- dumps dir to the local disk
  $ fatdisk mybootdisk rm dir
					-- recursively remove dir
  $ fatdisk mybootdisk rm dir/*/*.c
					-- selectively remove files
  $ fatdisk mybootdisk add dir
					-- recursively add dir to the disk
  $ fatdisk mybootdisk hexdump foo.c
					-- dump a file from the disk

  $ fatdisk mybootdisk format size 1G name MYDISK part 0 50% \
      bootloader grub_disk part 1 50% fat32 bootloader grub_disk

					-- create and format a 1G disk
					   with 2 FAT 32 partitions and grub
					   installed in sector 0 of part 0

Written by Neil McGill, goblinhack@gmail.com, with special thanks
to Donald Sharp, Andy Dalton and Mike Woods
