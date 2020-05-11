/*
 * tftptest.efi
 * EFI application to test an UEFI firmware's EFI_PXE_BASE_CODE_TFTP_READ_FILE function
 * 
 * Copyright (c) 2020 by Pieter Hollants <pieter@hollants.com>
 * Licensed under the GNU Public License (GPL) version 3
 */

#include <efi.h>
#include <efilib.h>

EFI_SYSTEM_TABLE *systab;

#define BOOTP_BOOTFILE_NAME_MAXLEN 256

#define DOWNLOADBUF_SIZE 65536

#define DEFAULT_BLOCKSIZE 1024

VOID
str2U(IN CHAR8 *src, OUT CHAR16 *dst, IN UINTN size)
{
    while (size && size-- && (*dst++ = *src++) != '\0');
    *dst = (CHAR16)0;
}

static VOID
wait_keypress(VOID)
{
    EFI_INPUT_KEY key;
    EFI_STATUS status;

    Print(L"\nPress any key...");
    while (uefi_call_wrapper(systab->ConIn->ReadKeyStroke, 2, systab->ConIn, &key) == EFI_SUCCESS);
    while ((status=uefi_call_wrapper(systab->ConIn->ReadKeyStroke,2, systab->ConIn, &key)) == EFI_NOT_READY);
    Print(L"\n");
}

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *system_tab) {
    EFI_STATUS status;

    EFI_LOADED_IMAGE *info;
    EFI_PXE_BASE_CODE *pxe;

    CHAR16 bootp_bootfile_name[BOOTP_BOOTFILE_NAME_MAXLEN];

    CHAR8 downloadbuf[DOWNLOADBUF_SIZE];
    UINT64 downloadbuf_len = DOWNLOADBUF_SIZE;

    UINTN blocksize = DEFAULT_BLOCKSIZE;

    /* UEFI initialization */
    systab = system_tab;
    InitializeLib(image, system_tab);

    /* Clear screen and write banner */
    uefi_call_wrapper(systab->ConOut->Reset, 2, systab->ConOut, FALSE);
    Print(L"\ntftptest.efi by Pieter Hollants <pieter@hollants.com>\n");
    Print(L"EFI application to test the firmware's EFI_PXE_BASE_CODE_TFTP_READ_FILE function\n");

    Print(L"[System Table] Firmware vendor: %s  Revision: %d\n", systab->FirmwareVendor, systab->FirmwareRevision);

    /* Determine device we were booted from */
    status = uefi_call_wrapper(
        BS->HandleProtocol,                 // BootServices function HandleProtocol
        3,                                  // 3 arguments following
        image,                              // IN EFI_HANDLE *Handle
        &LoadedImageProtocol,               // IN EFI_GUID *Protocol
        (VOID **)&info                      // OUT VOID **Interface
    );
    if (EFI_ERROR(status)) {
        Print(L"\nHandleProtocol EFI_LOADED_IMAGE_PROTOCOL failed!\n");
        wait_keypress();      
        return EFI_LOAD_ERROR;
    }

    /* Get PXE/DHCP information for this device */
    status = uefi_call_wrapper(
        BS->HandleProtocol,                 // BootServices function HandleProtocol
        3,                                  // 3 arguments following
        info->DeviceHandle,                 // IN EFI_HANDLE *Handle
        &PxeBaseCodeProtocol,               // IN EFI_GUID *Protocol
        (VOID **)&pxe                       // OUT VOID **Interface
    );
    if (EFI_ERROR(status)) {
        Print(L"\nHandleProtocol EFI_PXE_BASE_CODE_PROTOCOL on device handle failed!\n");
        wait_keypress();
        return EFI_LOAD_ERROR;
    }

    /* Should never happen */
    if (!pxe->Mode->DhcpDiscoverValid) {
        Print(L"\npxe->Mode->DhcpDiscoverValid not TRUE ?!\n");
        wait_keypress();
        return EFI_LOAD_ERROR;       
    }

    /* Print some information */
    Print(
        L"[DHCP] Client MAC: %02x:%02x:%02x:%02x:%02x:%02x  IP: %d.%d.%d.%d  Netmask: %d.%d.%d.%d\n",
        pxe->Mode->DhcpDiscover.Dhcpv4.BootpHwAddr[0], pxe->Mode->DhcpDiscover.Dhcpv4.BootpHwAddr[1],
        pxe->Mode->DhcpDiscover.Dhcpv4.BootpHwAddr[2], pxe->Mode->DhcpDiscover.Dhcpv4.BootpHwAddr[3],
        pxe->Mode->DhcpDiscover.Dhcpv4.BootpHwAddr[4], pxe->Mode->DhcpDiscover.Dhcpv4.BootpHwAddr[5],
        pxe->Mode->StationIp.v4.Addr[0] & 0xff, pxe->Mode->StationIp.v4.Addr[1] & 0xff,
        pxe->Mode->StationIp.v4.Addr[2] & 0xff, pxe->Mode->StationIp.v4.Addr[3] & 0xff,
        pxe->Mode->SubnetMask.v4.Addr[0] & 0xff, pxe->Mode->SubnetMask.v4.Addr[1] & 0xff,
        pxe->Mode->SubnetMask.v4.Addr[2] & 0xff, pxe->Mode->SubnetMask.v4.Addr[3] & 0xff
    );

    str2U(pxe->Mode->DhcpAck.Dhcpv4.BootpBootFile, bootp_bootfile_name, BOOTP_BOOTFILE_NAME_MAXLEN);
    Print(
        L"[DHCP] TFTP Server IP: %d.%d.%d.%d  Boot file (= us): %s\n",
        pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[0] & 0xff, pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[1] & 0xff,
        pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[2] & 0xff, pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[3] & 0xff,
        bootp_bootfile_name
    );

    for (CHAR8 *p = downloadbuf; (p-downloadbuf) < DOWNLOADBUF_SIZE; p++)
        *p = 0;

    Print(
        L"\ndownloadbuf[0-8]: %02x %02x %02x %02x %02x %02x %02x %02x -- all zeros!\n",
        downloadbuf[0], downloadbuf[1], downloadbuf[2], downloadbuf[3],
        downloadbuf[4], downloadbuf[5], downloadbuf[6], downloadbuf[7]
    );

    /* Download ourselves, an EXISTING file */
    Print(
        L"\nTFTP_READ_FILE \"%s\" from %d.%d.%d.%d... ",
        bootp_bootfile_name,
        pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[0] & 0xff, pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[1] & 0xff,
        pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[2] & 0xff, pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[3] & 0xff
    );

    status = uefi_call_wrapper(
        pxe->Mtftp,                                 // EFI_PXE_BASE_CODE_PROTOCOL function Mtftp
        10,                                         // 10 arguments following
        pxe,                                        // IN EFI_PXE_BASE_CODE_PROTOCOL *this
        EFI_PXE_BASE_CODE_TFTP_READ_FILE,           // IN EFI_PXE_BASE_CODE_TFTP_OPCODE
        &downloadbuf,                               // IN OUT VOID *BufferPtr OPTIONAL
        FALSE,                                      // IN BOOLEAN Overwrite
        &downloadbuf_len,                           // IN OUT UINT64 *BufferSize
        &blocksize,                                 // IN UINTN *BlockSize OPTIONAL
        pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr,      // IN EFI_IP_ADDRESS *ServerIp
        pxe->Mode->DhcpAck.Dhcpv4.BootpBootFile,    // IN CHAR8 *Filename OPTIONAL
        NULL,                                       // IN EFI_PXE_BASE_CODE_MTFTP_INFO *Info OPTIONAL
        FALSE                                       // IN BOOLEAN DontUseBuffer
    );
    if (status != EFI_SUCCESS) {
        Print(L"failed!\nError returned: %r\n", status);
        wait_keypress();
        return EFI_LOAD_ERROR;
    }
    Print(L"EFI_SUCCESS!\n");

    Print(
        L"\ndownloadbuf[0-8]: %02x %02x %02x %02x %02x %02x %02x %02x ",
        downloadbuf[0], downloadbuf[1], downloadbuf[2], downloadbuf[3],
        downloadbuf[4], downloadbuf[5], downloadbuf[6], downloadbuf[7]
    );

    /* Test for PE32 0x5A4D signature */
    if (downloadbuf[0] == 0x4D || downloadbuf[1] != 0x5A) {
        Print(L"-- PE32 sig found!\n");
    } else {
        Print(L"-- no PE32 sig ?!\n");
        wait_keypress();
        return EFI_LOAD_ERROR;
    }

    /* Now try to download a NON-EXISTING file */
    Print(
        L"\nTFTP_READ_FILE \"foobar.txt\" from %d.%d.%d.%d... ",
        pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[0] & 0xff, pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[1] & 0xff,
        pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[2] & 0xff, pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr[3] & 0xff
    );

    downloadbuf_len = DOWNLOADBUF_SIZE;
    status = uefi_call_wrapper(
        pxe->Mtftp,                                 // EFI_PXE_BASE_CODE_PROTOCOL function Mtftp
        10,                                         // 10 arguments following
        pxe,                                        // IN EFI_PXE_BASE_CODE_PROTOCOL *this
        EFI_PXE_BASE_CODE_TFTP_READ_FILE,           // IN EFI_PXE_BASE_CODE_TFTP_OPCODE
        &downloadbuf,                               // IN OUT VOID *BufferPtr OPTIONAL
        FALSE,                                      // IN BOOLEAN Overwrite
        &downloadbuf_len,                           // IN OUT UINT64 *BufferSize
        &blocksize,                                 // IN UINTN *BlockSize OPTIONAL
        pxe->Mode->DhcpAck.Dhcpv4.BootpSiAddr,      // IN EFI_IP_ADDRESS *ServerIp
        (CHAR8*)"foobar.txt",                       // IN CHAR8 *Filename OPTIONAL
        NULL,                                       // IN EFI_PXE_BASE_CODE_MTFTP_INFO *Info OPTIONAL
        FALSE                                       // IN BOOLEAN DontUseBuffer
    );
    if (status == EFI_SUCCESS)
    {
        Print(L"EFI_SUCCESS ?!\n\nThis shouldn't have happened if \"foobar.txt\" doesn't exist !!!");

        Print(
            L"\ndownloadbuf[0-8]: %02x %02x %02x %02x %02x %02x %02x %02x ",
            downloadbuf[0], downloadbuf[1], downloadbuf[2], downloadbuf[3],
            downloadbuf[4], downloadbuf[5], downloadbuf[6], downloadbuf[7]
        );

        /* Test for PE32 0x5A4D signature */
        if (downloadbuf[0] == 0x4D || downloadbuf[1] != 0x5A) {
            Print(L"-- still PE32 sig!\n");
        } else {
            Print(L"-- no PE32 sig ?!\n");
        }

        wait_keypress();
        return EFI_LOAD_ERROR;
    } else if (status == EFI_TFTP_ERROR) {
        Print(L"EFI_TFTP_ERROR!\n\nGood! As expected!\n");
    } else {
        Print(L"%r ?!\n", status);
        wait_keypress();
        return EFI_LOAD_ERROR;
    }

    wait_keypress();

    return EFI_SUCCESS;
}
