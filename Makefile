EFIINCS = -I/usr/include/efi -I/usr/include/efi/x86_64 -I/usr/include/efi/protocol

CFLAGS  = $(EFIINCS) -fno-stack-protector -fpic -fshort-wchar -mno-red-zone -DEFI_FUNCTION_WRAPPER -Wall
LDFLAGS = -nostdlib -znocombreloc -T /usr/lib64/elf_x86_64_efi.lds -shared -Bsymbolic -L /usr/lib64/gnuefi -L /usr/lib64 /usr/lib64/crt0-efi-x86_64.o

all: tftptest.efi

tftptest.o: tftptest.c
	gcc $(CFLAGS) -c -o $@ $^

tftptest.so: tftptest.o
	ld $(LDFLAGS) $^ -o $@ -lefi -lgnuefi

tftptest.efi: tftptest.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc --target=efi-app-x86_64 $^ $@

clean:
	@-rm tftptest.o tftptest.so tftptest.efi 2>/dev/null || true
