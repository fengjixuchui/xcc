#include <stdio.h>
#include <stdlib.h>  // calloc

#if defined(__XV6)
// XV6
#include "../kernel/types.h"
#include "../kernel/elf.h"

#elif defined(__linux__)
// Linux
#include <elf.h>

#else

#error Target not supported

#endif

void out_elf_header(FILE* fp, uintptr_t entry) {
  Elf64_Ehdr ehdr = {
    .e_ident     = { ELFMAG0, ELFMAG1, ELFMAG2 ,ELFMAG3,
                     ELFCLASS64, ELFDATA2LSB, EV_CURRENT, ELFOSABI_SYSV },
    .e_type      = ET_EXEC,
    .e_machine   = EM_X86_64,
    .e_version   = EV_CURRENT,
    .e_entry     = entry,
    .e_phoff     = sizeof(Elf64_Ehdr),
    .e_shoff     = 0, // dummy
    .e_flags     = 0x0,
    .e_ehsize    = sizeof(Elf64_Ehdr),
    .e_phentsize = sizeof(Elf64_Phdr),
    .e_phnum     = 1,
    .e_shentsize = 0, // dummy
    .e_shnum     = 0,
    .e_shstrndx  = 0, // dummy
  };

  fwrite(&ehdr, sizeof(Elf64_Ehdr), 1, fp);
}

void out_program_header(FILE* fp, uintptr_t offset, uintptr_t vaddr, uintptr_t filesz, uintptr_t memsz) {
  Elf64_Phdr phdr = {
    .p_type   = PT_LOAD,
    .p_offset = offset,
    .p_vaddr  = vaddr,
    .p_paddr  = 0, // dummy
    .p_filesz = filesz,
    .p_memsz  = memsz,
    .p_flags  = PF_R | PF_X | PF_W,
    .p_align  = 0x10,
  };

  fwrite(&phdr, sizeof(Elf64_Phdr), 1, fp);
}

void put_padding(FILE* fp, uintptr_t prog_start) {
  size_t size = prog_start - (sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr));
  char* buf = calloc(1, size);
  fwrite(buf, size, 1, fp);
}
