/***************************************************************************
* Copyright (C) 2016
* by Dimok
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any
* damages arising from the use of this software.
*
* Permission is granted to anyone to use this software for any
* purpose, including commercial applications, and to alter it and
* redistribute it freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you
* must not claim that you wrote the original software. If you use
* this software in a product, an acknowledgment in the product
* documentation would be appreciated but is not required.
*
* 2. Altered source versions must be plainly marked as such, and
* must not be misrepresented as being the original software.
*
* 3. This notice may not be removed or altered from any source
* distribution.
***************************************************************************/
#include "ios_fs_patches.h"
#include "../../ios_fs/ios_fs_syms.h"
#include "elf_patcher.h"
#include "types.h"
#include <string.h>

#define FS_PHYS_DIFF 0

u32 fs_get_phys_code_base(void) {
    return _text_start + FS_PHYS_DIFF;
}

void reset_fs_bss() {
    memset((void *) _bss_start, 0, _bss_end - _bss_start);
}

void fs_run_patches(uint32_t ios_elf_start) {
    section_write(ios_elf_start, _text_start, (void *) fs_get_phys_code_base(), _text_end - _text_start);
    section_write_bss(ios_elf_start, _bss_start, _bss_end - _bss_start);

    // Add IOCTL 0x28 to indicate the calling client should have full fs permissions
    section_write_word(ios_elf_start, 0x10701248, _FSA_ioctl0x28_hook);

    // Give clients that called IOCTL 0x28 full permissions
    section_write_word(ios_elf_start, 0x10704540, ARM_BL(0x10704540, FSA_IOCTLV_HOOK));
    section_write_word(ios_elf_start, 0x107044f0, ARM_BL(0x107044f0, FSA_IOCTL_HOOK));
    section_write_word(ios_elf_start, 0x10704458, ARM_BL(0x10704458, FSA_IOS_Close_Hook));

    // Keep patches for backwards compatibility (libiosuhax)
    // patch FSA raw access
    section_write_word(ios_elf_start, 0x1070FAE8, 0x05812070);
    section_write_word(ios_elf_start, 0x1070FAEC, 0xEAFFFFF9);

    // patch FSFAT_AddClient to enable FAT support for USB devices
    section_write_word(ios_elf_start, 0x1080bdc4, FSFAT_AddClient_hook);
    // mov r2, #0x1106
    //section_write_word(ios_elf_start, 0x1078f5c0, 0x062101E3);
    // str r2, [sp, #0x0c]
    //section_write_word(ios_elf_start, 0x1078f5c8, 0x0C208DE5);
    // nop
    //section_write_word(ios_elf_start, 0x1078f5d4, 0x00F020E3);

    // patch pdm_bpb_check_boot_sector to allow mounting ustealth'd drives if flag set
    section_write_word(ios_elf_start, 0x10793280, ARM_BL(0x10793280, pdm_bpb_check_boot_sector_hook));
    section_write_word(ios_elf_start, 0x107a8b74, ARM_BL(0x107a8b74, pdm_bpb_check_boot_sector_hook));
}