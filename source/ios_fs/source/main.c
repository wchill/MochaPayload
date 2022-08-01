#include "ipc_types.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct __attribute__((packed)) {
    uint32_t initialized;
    uint64_t titleId;
    uint32_t processId;
    uint32_t groupId;
    uint32_t unk0;
    uint64_t capabilityMask;
    uint8_t unk1[0x4518];
    char unk2[0x280];
    char unk3[0x280];
    void *mutex;
} FSAProcessData;
static_assert(sizeof(FSAProcessData) == 0x4A3C, "FSAProcessData: wrong size");

typedef struct __attribute__((packed)) {
    uint32_t opened;
    FSAProcessData *processData;
    char unk0[0x10];
    char unk1[0x90];
    uint32_t unk2;
    char work_dir[0x280];
    uint32_t unk3;
} FSAClientHandle;
static_assert(sizeof(FSAClientHandle) == 0x330, "FSAClientHandle: wrong size");

enum NodeTypes {
    NODE_TYPE_DEV_DF        = 0,
    NODE_TYPE_DEV_ATFS      = 1,
    NODE_TYPE_DEV_ISFS      = 2,
    NODE_TYPE_DEV_WFS       = 3,
    NODE_TYPE_DEV_PCFS      = 4,
    NODE_TYPE_DEV_RBFS      = 5,
    NODE_TYPE_DEV_FAT       = 6,
    NODE_TYPE_DEV_FLA       = 7,
    NODE_TYPE_DEV_UMS       = 8,
    NODE_TYPE_DEV_AHCIMGR   = 9,
    NODE_TYPE_DEV_SHDD      = 10,
    NODE_TYPE_DEV_MD        = 11,
    NODE_TYPE_DEV_SCFM      = 12,
    NODE_TYPE_DEV_MMC       = 13,
    NODE_TYPE_DEV_TIMETRACE = 14,
    NODE_TYPE_DEV_TCP_PCFS  = 15
};

enum SALDeviceTypes {
    SAL_DEVICE_END      = 0x00,
    SAL_DEVICE_SI       = 0x01,
    SAL_DEVICE_ODD      = 0x02,
    SAL_DEVICE_SLCCMPT  = 0x03,
    SAL_DEVICE_SLC      = 0x04,
    SAL_DEVICE_MLC      = 0x05,
    SAL_DEVICE_SDCARD   = 0x06,
    SAL_DEVICE_SD       = 0x07,
    SAL_DEVICE_HFIO     = 0x08,
    SAL_DEVICE_RAMDISK  = 0x09,
    SAL_DEVICE_USB      = 0x11,
    SAL_DEVICE_MLCORIG  = 0x12
};

enum FsTypes {
    FS_TYPE_RAW         = 0x8003,
    FS_TYPE_FAT         = 0x8004,
    FS_TYPE_WFS         = 0x8005,
    FS_TYPE_ISFS        = 0x8006,
    FS_TYPE_ATFS        = 0x8007
};

typedef struct __attribute__((packed)) {
    void *callback;
    void *unk;
    void *attach_callback;
    enum SALDeviceTypes allowed_devices[12];
} SALAddFilesystemArg;

typedef int FSSALHandle;

#define PATCHED_CLIENT_HANDLES_MAX_COUNT 0x40

FSAClientHandle *patchedClientHandles[PATCHED_CLIENT_HANDLES_MAX_COUNT];

int (*const IOS_ResourceReply)(void *, int32_t) = (void *) 0x107f6b4c;

int FSA_ioctl0x28_hook(FSAClientHandle *handle, void *request) {
    int res = -5;
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == handle) {
            res = 0;
            break;
        }
        if (patchedClientHandles[i] == 0) {
            patchedClientHandles[i] = handle;
            res                     = 0;
            break;
        }
    }

    IOS_ResourceReply(request, res);
    return 0;
}

typedef struct __attribute__((packed)) {
    ipcmessage ipcmessage;
} ResourceRequest;

int (*const real_FSA_IOCTLV)(ResourceRequest *, uint32_t, uint32_t) = (void *) 0x10703164;
int (*const get_handle_from_val)(uint32_t)                          = (void *) 0x107046d4;
int FSA_IOCTLV_HOOK(ResourceRequest *param_1, uint32_t u2, uint32_t u3) {
    FSAClientHandle *clientHandle = (FSAClientHandle *) get_handle_from_val(param_1->ipcmessage.fd);
    uint64_t oldValue             = clientHandle->processData->capabilityMask;
    int toBeRestored              = 0;
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            clientHandle->processData->capabilityMask = 0xffffffffffffffffL;
            // printf("IOCTL: Force mask to 0xFFFFFFFFFFFFFFFF for client %08X\n", (uint32_t) clientHandle);
            toBeRestored = 1;
            break;
        }
    }
    int res = real_FSA_IOCTLV(param_1, u2, u3);

    if (toBeRestored) {
        // printf("IOCTL: Restore mask for client %08X\n", (uint32_t) clientHandle);
        clientHandle->processData->capabilityMask = oldValue;
    }

    return res;
}

int (*const real_FSA_IOCTL)(ResourceRequest *, uint32_t, uint32_t, uint32_t) = (void *) 0x107010a8;

int FSA_IOCTL_HOOK(ResourceRequest *request, uint32_t u2, uint32_t u3, uint32_t u4) {
    FSAClientHandle *clientHandle = (FSAClientHandle *) get_handle_from_val(request->ipcmessage.fd);
    uint64_t oldValue             = clientHandle->processData->capabilityMask;
    int toBeRestored              = 0;
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            // printf("IOCTL: Force mask to 0xFFFFFFFFFFFFFFFF for client %08X\n", (uint32_t) clientHandle);
            clientHandle->processData->capabilityMask = 0xffffffffffffffffL;
            toBeRestored                              = 1;
            break;
        }
    }
    int res = real_FSA_IOCTL(request, u2, u3, u4);

    if (toBeRestored) {
        // printf("IOCTL: Restore mask for client %08X\n", (uint32_t) clientHandle);
        clientHandle->processData->capabilityMask = oldValue;
    }

    return res;
}

int (*const real_FSA_IOS_Close)(uint32_t fd, ResourceRequest *request) = (void *) 0x10704864;
int FSA_IOS_Close_Hook(uint32_t fd, ResourceRequest *request) {
    FSAClientHandle *clientHandle = (FSAClientHandle *) get_handle_from_val(fd);
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            // printf("Close: %p will be closed, reset slot %d\n", clientHandle, i);
            patchedClientHandles[i] = 0;
            break;
        }
    }
    return real_FSA_IOS_Close(fd, request);
}


/*
int *FSFAT_LastError = (int*) 0x10bb7e80;
int *DAT_10bb7e40 = (int*) 0x10bb7e40;
int *DAT_10bb7e44 = (int*) 0x10bb7e44;
int *DAT_10bb7e50 = (int*) 0x10bb7e50;
int *FSFAT_SALHandle = (int*) 0x10bb7e48;

 */

FSSALHandle (*const FSSAL_AddFilesystem)(SALAddFilesystemArg *arg) = (void *) 0x10732d70;
uint32_t (*const FSFAT_AttachDetachCB)(FSSALHandle handle, bool attach) = (void*) 0x1078e018;

int FSFAT_AddClient_hook(int client, int param2, int param3, int param4) {
    int retval = 0;

    if (client == 0) {
        //*FSFAT_LastError = 0xffe1ffdf;
        *(uint32_t*)0x10bb7e80 = 0xffe1ffdf;
        retval = -0x1e0021;
        return retval;
    }

    // This is a flag of some sort. Maybe a "client initialized"?
    //int r4 = *DAT_10bb7e40 & 0x1;
    int r4 = *(uint32_t*)0x10bb7e40 & 0x1;
    if (r4 != 0) {
        return retval;
    }

    SALAddFilesystemArg arg;
    arg.callback = NULL;
    arg.unk = NULL;
    arg.attach_callback = FSFAT_AttachDetachCB;
    arg.allowed_devices[0] = SAL_DEVICE_SDCARD;
    arg.allowed_devices[1] = SAL_DEVICE_USB;
    arg.allowed_devices[2] = SAL_DEVICE_END;

    //*FSFAT_SALHandle = FSSAL_AddFilesystem(&arg);
    //*DAT_10bb7e44 = 0x200;
    //*DAT_10bb7e40 = 1;
    //*DAT_10bb7e50 = client;
    *(FSSALHandle*)0x10bb7e48 = FSSAL_AddFilesystem(&arg);
    *(uint32_t*)0x10bb7e44 = 0x200;
    *(uint32_t*)0x10bb7e40 = 1;
    *(uint32_t*)0x10bb7e50 = client;

    retval = 0;
    return retval;
}

bool enable_ustealth = 0;
int (*const pdm_bpb_check_boot_sector)(uint8_t *boot_sector, uint32_t *param2) = (void *)0x107a9828;
int pdm_bpb_check_boot_sector_hook(uint8_t *boot_sector, uint32_t *param2) {
    if (enable_ustealth && boot_sector[0x1fe] == 0x55 && boot_sector[0x1ff] == 0xab) {
        return 0;
    }

    return pdm_bpb_check_boot_sector(boot_sector, param2);
}

int FSA_ioctl0x29_hook(FSAClientHandle *handle, void *request) {
    int res = 0;
    enable_ustealth = true;

    IOS_ResourceReply(request, res);
    return 0;
}

int FSA_ioctl0x30_hook(FSAClientHandle *handle, void *request) {
    int res = 0;
    enable_ustealth = false;

    IOS_ResourceReply(request, res);
    return 0;
}