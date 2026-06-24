#include <ctype.h>
#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STRING 256

typedef struct s_elf {
    int elf_class;
    Elf32_Ehdr header32;
    Elf64_Ehdr header64;
} t_elf;

static int print_decimal(const char *text)
{
    unsigned long long value;
    char *end;

    errno = 0;
    value = strtoull(text, &end, 0);
    if (errno != 0 || *text == '\0' || *end != '\0') {
        fprintf(stderr, "invalid number: %s\n", text);
        return 0;
    }
    printf("%s = %llu\n", text, value);
    return 1;
}

static int read_elf(FILE *file, t_elf *elf)
{
    unsigned char ident[EI_NIDENT];

    if (fseek(file, 0, SEEK_SET) != 0
        || fread(ident, sizeof(ident), 1, file) != 1)
        return 0;
    if (memcmp(ident, ELFMAG, SELFMAG) != 0
        || ident[EI_DATA] != ELFDATA2LSB) {
        fprintf(stderr, "error: expected a little-endian ELF file\n");
        return 0;
    }
    elf->elf_class = ident[EI_CLASS];
    if (fseek(file, 0, SEEK_SET) != 0)
        return 0;
    if (elf->elf_class == ELFCLASS32)
        return fread(&elf->header32, sizeof(elf->header32), 1, file) == 1;
    if (elf->elf_class == ELFCLASS64)
        return fread(&elf->header64, sizeof(elf->header64), 1, file) == 1;
    fprintf(stderr, "error: unsupported ELF class\n");
    return 0;
}

static int offset32(FILE *file, const Elf32_Ehdr *header, uint64_t address,
                    uint64_t *offset)
{
    Elf32_Phdr program;
    uint16_t i;

    if (address > UINT32_MAX)
        return 0;
    for (i = 0; i < header->e_phnum; i++) {
        if (fseek(file, (long)header->e_phoff
                        + (long)i * header->e_phentsize,
                  SEEK_SET) != 0
            || fread(&program, sizeof(program), 1, file) != 1)
            return 0;
        if (program.p_type != PT_LOAD)
            continue;
        if (address >= program.p_vaddr
            && address < (uint64_t)program.p_vaddr + program.p_filesz) {
            *offset = program.p_offset + (address - program.p_vaddr);
            return 1;
        }
        if (address >= program.p_vaddr
            && address < (uint64_t)program.p_vaddr + program.p_memsz)
            return -1;
    }
    return 0;
}

static int offset64(FILE *file, const Elf64_Ehdr *header, uint64_t address,
                    uint64_t *offset)
{
    Elf64_Phdr program;
    uint16_t i;

    for (i = 0; i < header->e_phnum; i++) {
        if (fseek(file, (long)header->e_phoff
                        + (long)i * header->e_phentsize,
                  SEEK_SET) != 0
            || fread(&program, sizeof(program), 1, file) != 1)
            return 0;
        if (program.p_type != PT_LOAD)
            continue;
        if (address >= program.p_vaddr
            && address < program.p_vaddr + program.p_filesz) {
            *offset = program.p_offset + (address - program.p_vaddr);
            return 1;
        }
        if (address >= program.p_vaddr
            && address < program.p_vaddr + program.p_memsz)
            return -1;
    }
    return 0;
}

static int virtual_to_offset(FILE *file, const t_elf *elf, uint64_t address,
                             uint64_t *offset)
{
    int result;

    if (elf->elf_class == ELFCLASS32)
        result = offset32(file, &elf->header32, address, offset);
    else
        result = offset64(file, &elf->header64, address, offset);
    if (result == -1)
        fprintf(stderr,
                "0x%" PRIx64 ": address is zero-initialized memory (BSS), "
                "not file data\n",
                address);
    else if (result == 0)
        fprintf(stderr,
                "0x%" PRIx64 ": address is not in a loadable ELF segment\n",
                address);
    return result == 1;
}

static int executable_section32(FILE *file, const Elf32_Ehdr *header,
                                uint64_t address)
{
    Elf32_Shdr section;
    uint16_t i;

    for (i = 0; i < header->e_shnum; i++) {
        if (fseek(file, (long)header->e_shoff
                        + (long)i * header->e_shentsize,
                  SEEK_SET) != 0
            || fread(&section, sizeof(section), 1, file) != 1)
            return -1;
        if (address >= section.sh_addr
            && address < (uint64_t)section.sh_addr + section.sh_size)
            return (section.sh_flags & SHF_EXECINSTR) != 0;
    }
    return 0;
}

static int executable_section64(FILE *file, const Elf64_Ehdr *header,
                                uint64_t address)
{
    Elf64_Shdr section;
    uint16_t i;

    for (i = 0; i < header->e_shnum; i++) {
        if (fseek(file, (long)header->e_shoff
                        + (long)i * header->e_shentsize,
                  SEEK_SET) != 0
            || fread(&section, sizeof(section), 1, file) != 1)
            return -1;
        if (address >= section.sh_addr
            && address < section.sh_addr + section.sh_size)
            return (section.sh_flags & SHF_EXECINSTR) != 0;
    }
    return 0;
}

static int address_is_executable(FILE *file, const t_elf *elf,
                                 uint64_t address)
{
    if (elf->elf_class == ELFCLASS32)
        return executable_section32(file, &elf->header32, address);
    return executable_section64(file, &elf->header64, address);
}

static void print_byte(unsigned char byte)
{
    if (byte == '\n')
        printf("\\n");
    else if (byte == '\t')
        printf("\\t");
    else if (byte == '\r')
        printf("\\r");
    else if (byte == '\\' || byte == '"')
        printf("\\%c", byte);
    else if (isprint(byte))
        putchar(byte);
    else
        printf("\\x%02x", byte);
}

static int decode_address(FILE *file, const t_elf *elf, const char *text)
{
    unsigned long long parsed;
    unsigned char byte;
    uint64_t offset;
    char *end;
    int executable;
    size_t count;

    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno != 0 || *text == '\0' || *end != '\0') {
        fprintf(stderr, "invalid address: %s\n", text);
        return 0;
    }
    if (!virtual_to_offset(file, elf, parsed, &offset))
        return 0;
    executable = address_is_executable(file, elf, parsed);
    if (executable < 0)
        return 0;
    if (executable) {
        fprintf(stderr,
                "0x%llx: address is in an executable ELF section "
                "(code), not string data\n",
                parsed);
        return 0;
    }
    if (fseek(file, (long)offset, SEEK_SET) != 0)
        return 0;

    if (elf->elf_class == ELFCLASS32)
        printf("0x%08llx", parsed);
    else
        printf("0x%016llx", parsed);
    printf(" (file offset 0x%" PRIx64 "): \"", offset);
    count = 0;
    while (count < MAX_STRING && fread(&byte, 1, 1, file) == 1) {
        if (byte == '\0')
            break;
        print_byte(byte);
        count++;
    }
    printf("\"\n");
    return 1;
}

int main(int argc, char **argv)
{
    t_elf elf;
    FILE *file;
    int status;
    int i;

    if (argc == 3 && strcmp(argv[1], "--decimal") == 0)
        return print_decimal(argv[2]) ? 0 : 1;
    if (argc < 3) {
        fprintf(stderr,
                "usage: %s elf_file address [address ...]\n"
                "       %s --decimal number\n",
                argv[0], argv[0]);
        return 1;
    }
    file = fopen(argv[1], "rb");
    if (file == NULL) {
        perror(argv[1]);
        return 1;
    }
    if (!read_elf(file, &elf)) {
        fclose(file);
        return 1;
    }
    status = 0;
    for (i = 2; i < argc; i++) {
        if (!decode_address(file, &elf, argv[i]))
            status = 1;
    }
    fclose(file);
    return status;
}
