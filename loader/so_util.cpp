#include "abi.h"
#include <algorithm>
#include <vector>
/* so_util.c -- utils to load and hook .so modules
 *
 * Copyright (C) 2021 Andy Nguyen
 *
 * This software may be modified and distributed under the terms
 * of the GPLv3 license. See the LICENSE.md file for details.
 */

// JohnnyonFlame:
// This is a severely rewritten/reworked version of the so_util.c library built by @TheOfficialFloW
// for the gtasa_vita project.
// Notable changes:
// - Ported from PS-Vita to Linux.
// - Supports both ARMHF and AARCH64 targets
// - Supports many more relocation segments - e.g. DT_ANDROID_REL_OFFSET
//   - Can now load multiple android system libraries for better compatibility, e.g. libm, libc++, etc.
// - Single-word hooks using trampoline code generation for longer jump targets
//   - Allows for safely hooking functions that have been stubbed by compile targets, such as enabling debugging output messages.
// - PLT0 stub hook for easier debugging when the application crashes with a missing import

#define __USE_MISC
#include <cstdlib>
#include <execinfo.h>
#include <iostream>

#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <leb128.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <type_traits>
#include <unistd.h>

#include "platform.h"
#include "so_util.h"

#if defined(__aarch64__)
#include "arm64_encodings.h"
#elif defined(__arm__)
#include "arm32_encodings.h"
#endif
#include <cstring>

#define PATCH_SZ 0x10000 // 64 KB-ish arenas
static so_module* head = NULL;
so_module* so_get_head() { return head; }

// rela_functor functions should return 1 to stop, or 0 to continue executing,
// and will receive the module and relocation data from the relocation iterator
using rela_functor = std::function<int(so_module* mod, const Elf_Rela* rel)>;

void foreach_rela(so_module* mod, rela_functor functor);
void so_relocate_all(so_module* mod);

static void so_flush_caches(so_module* mod, int write)
{
    // clear cache and set EXEC on all executable regions
    __builtin___clear_cache((void*)mod->patch_base, (void*)(mod->cave_head));
    mprotect((void*)mod->patch_base, mod->cave_head - mod->patch_base, PROT_EXEC | (write ? PROT_WRITE | PROT_READ : PROT_READ));
}

/*
 * alloc_arena: allocates space on either patch or cave arenas,
 * range: maximum range from allocation to dst (ignored if NULL)
 * dst: destination address
 */
uintptr_t so_alloc_arena(so_module* so, uintptr_t range, uintptr_t dst, size_t sz)
{
// Is address in range?
#define inrange(lsr, gtr, range) \
    (((uintptr_t)(range) == (uintptr_t)NULL) || ((uintptr_t)(range) >= ((uintptr_t)(gtr) - (uintptr_t)(lsr))))
// Space left on block
#define blkavail(type) (so->type##_size - (so->type##_head - so->type##_base))

    // keep allocations 4-byte aligned for simplicity
    sz = ALIGN_MEM(sz, 4);

    if (sz <= (blkavail(patch)) && inrange(so->patch_base, dst, range)) {
        so->patch_head += sz;
        return (so->patch_head - sz);
    } else if (sz <= (blkavail(cave)) && inrange(dst, so->cave_base, range)) {
        so->cave_head += sz;
        return (so->cave_head - sz);
    }

    return (uintptr_t)NULL;
}

#pragma GCC push_options
#pragma GCC optimize("O0")
void gdb_push(const char* name, uintptr_t load_addr)
{
    // This is used by the debug.gdb script
    // Optimizations are forced OFF so this isn't stripped away
}
void gdb_break_unknown_symbol()
{
    // This is used by the debug.gdb script
    // Optimizations are forced OFF so this isn't stripped away
}
#pragma GCC pop_options

// Maps PT_LOAD segment i straight from the file at its final address (private copy-on-write), the way a real
// dynamic linker does: pages are only read from storage when touched, instead of copying the whole library in.
// Returns false when it can't (misaligned segment, a page shared with another segment, or an executable file mapping
// refused because the file system is mounted noexec); the caller then copies the segment instead.
static bool so_map_segment(so_module* mod, int i, int fd, uintptr_t load_addr, size_t min_vaddr, const uintptr_t* origVaddr)
{
    const Elf_Phdr& ph = mod->phdr[i];
    if (fd < 0 || ph.p_filesz == 0)
        return false;
    const uintptr_t page = (uintptr_t)sysconf(_SC_PAGESIZE);
    uintptr_t vaddr = origVaddr[i] + load_addr - min_vaddr;
    if ((vaddr & (page - 1)) != (ph.p_offset & (page - 1)))
        return false;
    uintptr_t mapStart = vaddr & ~(page - 1);
    uintptr_t fileEnd = vaddr + ph.p_filesz;
    uintptr_t mapEnd = (fileEnd + page - 1) & ~(page - 1);
    // A page shared with another segment can't hold this segment's file data alone.
    for (int j = 0; j < mod->ehdr->e_phnum; j++) {
        const Elf_Phdr& o = mod->phdr[j];
        if (j == i || o.p_type != PT_LOAD) continue;
        uintptr_t os = (origVaddr[j] + load_addr - min_vaddr) & ~(page - 1);
        uintptr_t oe = (origVaddr[j] + load_addr - min_vaddr + o.p_memsz + page - 1) & ~(page - 1);
        if (os < mapEnd && oe > mapStart) return false;
    }
    off_t fileOff = (off_t)(ph.p_offset - (vaddr - mapStart));
    void* m = mmap((void*)mapStart, mapEnd - mapStart, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, fd, fileOff);
    if (m == MAP_FAILED)
        return false;
    if ((ph.p_flags & PF_X) && mprotect(m, mapEnd - mapStart, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        // noexec mount: put anonymous memory back and let the caller copy
        mmap((void*)mapStart, mapEnd - mapStart, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        return false;
    }
    // The rest of the last page is the start of .bss, not whatever follows in the file.
    if (mapEnd > fileEnd)
        memset((void*)fileEnd, 0, mapEnd - fileEnd);
    return true;
}

static int so_load_impl(so_module* mod, const char* filename, uintptr_t load_addr, void* so_data, size_t sz, int fd);

int so_load(so_module* mod, const char* filename, uintptr_t load_addr, void* so_data, size_t sz)
{
    int r = so_load_impl(mod, filename, load_addr, so_data, sz, -1);
    free(so_data);
    return r;
}

int so_load_fd(so_module* mod, const char* filename, uintptr_t load_addr, int fd)
{
    struct stat st;
    if (fstat(fd, &st) != 0)
        return -1;
    // The whole file read-only, for the headers, symbol and relocation tables; only touched pages are read.
    void* so_data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (so_data == MAP_FAILED)
        return -1;
    int r = so_load_impl(mod, filename, load_addr, so_data, st.st_size, fd);
    munmap(so_data, st.st_size);
    return r;
}

static int so_load_impl(so_module* mod, const char* filename, uintptr_t load_addr, void* so_data, size_t sz, int fd)
{
    // Basic elf header pointer
    mod->ehdr = (Elf_Ehdr*)so_data;
    mod->phdr = (Elf_Phdr*)((uintptr_t)so_data + mod->ehdr->e_phoff);
    mod->shdr = (Elf_Shdr*)((uintptr_t)so_data + mod->ehdr->e_shoff);
    mod->shstr = (char*)((uintptr_t)so_data + mod->shdr[mod->ehdr->e_shstrndx].sh_offset);

    // Copy header data to persistent memory
    // Calculate size needed for all headers and string table
    size_t ehdr_size = sizeof(Elf_Ehdr);
    size_t phdr_size = sizeof(Elf_Phdr) * mod->ehdr->e_phnum;
    size_t shdr_size = sizeof(Elf_Shdr) * mod->ehdr->e_shnum;
    size_t shstr_size = mod->shdr[mod->ehdr->e_shstrndx].sh_size;
    
    size_t total_hdr_size = ehdr_size + phdr_size + shdr_size + shstr_size;
    void* hdr_copy = malloc(total_hdr_size);
    if (!hdr_copy)
        return -1;
    
    // Copy and update pointers
    uintptr_t offset = 0;
    
    // Copy ELF header
    memcpy(hdr_copy, mod->ehdr, ehdr_size);
    mod->ehdr = (Elf_Ehdr*)((uintptr_t)hdr_copy + offset);
    offset += ehdr_size;
    
    // Copy program headers
    memcpy((void*)((uintptr_t)hdr_copy + offset), mod->phdr, phdr_size);
    mod->phdr = (Elf_Phdr*)((uintptr_t)hdr_copy + offset);
    offset += phdr_size;
    
    // Copy section headers
    memcpy((void*)((uintptr_t)hdr_copy + offset), mod->shdr, shdr_size);
    mod->shdr = (Elf_Shdr*)((uintptr_t)hdr_copy + offset);
    offset += shdr_size;
    
    // Copy section header string table
    memcpy((void*)((uintptr_t)hdr_copy + offset), mod->shstr, shstr_size);
    mod->shstr = (char*)((uintptr_t)hdr_copy + offset);
    
    // Calculate the necessary memory mapped area for the entire elf
    size_t max_vaddr = 0;
    size_t min_vaddr = ~0;
    for (int i = 0; i < mod->ehdr->e_phnum; i++) {
        if (mod->phdr[i].p_type == PT_LOAD) {
            size_t seg_end = ALIGN_MEM(mod->phdr[i].p_vaddr + mod->phdr[i].p_memsz, mod->phdr[i].p_align);
            if (seg_end > max_vaddr)
                max_vaddr = seg_end;
            if (mod->phdr[i].p_vaddr < min_vaddr)
                min_vaddr = mod->phdr[i].p_vaddr;
        }
    }

    // Total area = load + patch arena
    size_t load_sz = max_vaddr - min_vaddr;
    size_t load_total_sz = load_sz + PATCH_SZ;

    // Now memory map the requested size
    // Not MAP_POPULATE: that allocated (and the memset below used to zero) every page of the image up front,
    // hundreds of MB for Unity. Anonymous memory is zero already and pages appear when first touched.
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (load_addr > 0)
        flags |= MAP_FIXED;

    void* shd = mmap((void*)(load_addr - PATCH_SZ), load_total_sz, PROT_READ | PROT_WRITE, flags, 0, 0);

    if (load_addr == 0)
        load_addr = (uintptr_t)shd + PATCH_SZ;

    if (!shd)
        return -1;

    // This is a hint for GDB to load the elf file symbols - you need to add a
    // breakpoint and handle this yourself however.
    gdb_push(filename, load_addr);

    // Allocate arena for code patches, trampolines, etc
    // Ideally right under .text
    mod->patch_size = PATCH_SZ;
    mod->patch_base = mod->patch_head = ALIGN_MEM(load_addr - PATCH_SZ, 0x4);
    mod->base = load_addr;

    int mapped = 0, copied = 0;
    std::vector<uintptr_t> origVaddr(mod->ehdr->e_phnum);
    for (int i = 0; i < mod->ehdr->e_phnum; i++)
        origVaddr[i] = mod->phdr[i].p_vaddr;
    for (int i = 0; i < mod->ehdr->e_phnum; i++) {
        if (mod->phdr[i].p_type == PT_LOAD) {
            size_t prog_size = mod->phdr[i].p_memsz;
            mod->phdr[i].p_vaddr += load_addr - min_vaddr;

            if ((mod->phdr[i].p_flags & PF_X) == PF_X) {
                mod->text_base = mod->phdr[i].p_vaddr;
                mod->text_size = prog_size;
            } else {
                if (mod->n_data >= MAX_DATA_SEG) {
                    return -1;
                }

                mod->data_base[mod->n_data] = mod->phdr[i].p_vaddr;
                mod->data_size[mod->n_data] = mod->phdr[i].p_memsz;
                mod->n_data++;
            }

            // Map the segment from the file when possible, otherwise copy it into the allocated region.
            if (so_map_segment(mod, i, fd, load_addr, min_vaddr, origVaddr.data())) {
                mapped++;
            } else {
                memcpy((void*)mod->phdr[i].p_vaddr, (void*)((uintptr_t)so_data + mod->phdr[i].p_offset), mod->phdr[i].p_filesz);
                copied++;
            }
        }
    }

    // Start by setting the cave to go from ".text" to the end of the mapped
    // virtual memory region
    uintptr_t cave_base = mod->text_base + mod->text_size;
    uintptr_t cave_end = load_addr + load_sz;

    // Shrink the cave until it fits between the executable segment and the next
    // loaded segment.
    for (int i = 0; i < mod->ehdr->e_phnum; i++) {
        if (mod->phdr[i].p_type != PT_LOAD)
            continue;

        if (mod->phdr[i].p_vaddr > cave_base && mod->phdr[i].p_vaddr < cave_end)
            cave_end = mod->phdr[i].p_vaddr;
    }

    mod->cave_base = mod->cave_head = cave_base;
    mod->cave_size = cave_end - cave_base;

    // Gather some information from the headers for posteriority
    // TODO:: Maybe rework into using DT_*?
    for (int i = 0; i < mod->ehdr->e_shnum; i++) {
        char* sh_name = mod->shstr + mod->shdr[i].sh_name;
        uintptr_t sh_addr = load_addr + mod->shdr[i].sh_addr;
        size_t sh_size = mod->shdr[i].sh_size;
        if (strcmp(sh_name, ".dynamic") == 0) {
            mod->dynamic = (Elf_Dyn*)sh_addr;
            mod->num_dynamic = sh_size / sizeof(Elf_Dyn);
        } else if (strcmp(sh_name, ".dynstr") == 0) {
            mod->dynstr = (char*)sh_addr;
        } else if (strcmp(sh_name, ".dynsym") == 0) {
            mod->dynsym = (Elf_Sym*)sh_addr;
            mod->num_dynsym = sh_size / sizeof(Elf_Sym);
        } else if (strcmp(sh_name, ".rel.dyn") == 0) {
            if (mod->shdr[i].sh_type == SHT_ANDROID_REL) {
                mod->droidreldyn = (uint8_t*)sh_addr;
                mod->num_droidreldyn = sh_size;
            } else {
                mod->reldyn = (Elf_Rel*)sh_addr;
                mod->num_reldyn = sh_size / sizeof(Elf_Rel);
            }
        } else if (strcmp(sh_name, ".rel.plt") == 0) {
            mod->relplt = (Elf_Rel*)sh_addr;
            mod->num_relplt = sh_size / sizeof(Elf_Rel);
        } else if (strcmp(sh_name, ".rela.dyn") == 0) {
            if (mod->shdr[i].sh_type == SHT_ANDROID_RELA) {
                mod->droidreladyn = (uint8_t*)sh_addr;
                mod->num_droidreladyn = sh_size;
            } else {
                mod->reladyn = (Elf_Rela*)sh_addr;
                mod->num_reladyn = sh_size / sizeof(Elf_Rela);
            }
        } else if (strcmp(sh_name, ".rela.plt") == 0) {
            mod->relaplt = (Elf_Rela*)sh_addr;
            mod->num_relaplt = sh_size / sizeof(Elf_Rela);
        } else if (strcmp(sh_name, ".init_array") == 0) {
            mod->init_array = (init_array_t*)sh_addr;
            mod->num_init_array = sh_size / sizeof(void*);
        } else if (strcmp(sh_name, ".hash") == 0) {
            mod->hash = (uint32_t*)sh_addr;
        }
    }

    for (int i = 0; i < mod->num_dynamic; i++) {
        switch (mod->dynamic[i].d_tag) {
        case DT_RELR:
        case DT_ANDROID_RELR:
            mod->relr = (Elf_Relr*)((uintptr_t)so_data + mod->dynamic[i].d_un.d_val);
            break;
        case DT_ANDROID_RELRSZ:
        case DT_RELRSZ:
            mod->num_relr = mod->dynamic[i].d_un.d_val / sizeof(Elf_Relr);
            break;
        case DT_SONAME:
            mod->soname = mod->dynstr + mod->dynamic[i].d_un.d_ptr;
            break;
        default:
            break;
        }
    }

    // Relocations, house keeping, etc.
    so_relocate_all(mod);
    so_static_overrides(mod);
    so_flush_caches(mod, 1);
    so_initialize(mod);

    // Register the loaded shared file for future reference, say, during the
    // resolution of dynamic symbol names.
    mod->path = (char*)malloc(strlen(filename) + 1);
    strcpy(mod->path, filename);

    mod->next = head;
    head = mod;

    if (fd >= 0)
        printf("Loaded %s: %d segment(s) mapped from the file, %d copied\n", filename, mapped, copied);
    return 0;
}

void reloc_err(uintptr_t got0)
{
    void* array[50]; // Array to store stack trace addresses
    size_t size;

    // Get the stack trace
    size = backtrace(array, 50);

    // Print the stack trace
    std::cerr << "Unknown Symbol encountered\n";
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    gdb_break_unknown_symbol();
    // Find to which module this missing symbol belongs
    int found = 0;
    so_module* curr = head;
    while (curr && !found) {
        if ((got0 >= curr->text_base) && (got0 <= curr->text_base + curr->text_size))
            found = 1;

        for (int i = 0; i < curr->n_data; i++)
            if ((got0 >= curr->data_base[i]) && (got0 <= curr->data_base[i] + curr->data_size[i]))
                found = 1;

        if (!found)
            curr = curr->next;
    }

    if (curr) {
        const char* sym_name = NULL;

        // Attempt to find symbol name and then display error
        foreach_rela(curr, [&](so_module* mod, const Elf_Rela* rel) -> int {
            Elf_Sym* sym = &mod->dynsym[ELF_R_SYM(rel->r_info)];
            uintptr_t* ptr = (uintptr_t*)(mod->base + rel->r_offset);
            if (got0 == (uintptr_t)ptr) {
                sym_name = mod->dynstr + sym->st_name;
                return 1;
            }

            return 0;
        });

        if (sym_name) {
            fatal_error("Unknown symbol \"%s\" (%p).\n", sym_name, (void*)got0);
            exit(-1);
        }
    }

    // Ooops, this shouldn't have happened.
    fatal_error("Unknown symbol \"???\" (%p).\n", (void*)got0);
    exit(-1);
}

void plt0_stub()
{
#if defined(__aarch64__)
    register uintptr_t got0 asm("x16");
    reloc_err(got0);
#elif defined(__arm__)
    register uintptr_t got0 asm("r12");
    reloc_err(got0);
#else
#error Implement me
#endif
}

template <typename Rel>
int helper_foreach_rel(so_module* mod, Rel* rels, int rel_cnt, rela_functor functor)
{
    for (int i = 0; i < rel_cnt; i++) {
        Elf_Rela rela = {};
        rela.r_offset = rels[i].r_offset;
        rela.r_info = rels[i].r_info;
        if constexpr (std::is_same_v<decltype(rels), Elf_Rela*>)
            rela.r_addend = rels[i].r_addend;

        if (functor(mod, &rela))
            return 1;
    }

    return 0;
}

template <typename Rel>
void so_relocate_rel(so_module* mod, const Rel* rel)
{
    Elf_Sym* sym = &mod->dynsym[ELF_R_SYM(rel->r_info)];
    uintptr_t* ptr = (uintptr_t*)(mod->base + rel->r_offset);

    Elf_Sword addend = 0;
    if constexpr (std::is_same_v<Elf_Rela, Rel>)
        addend = rel->r_addend;

    int type = ELF_R_TYPE(rel->r_info);
    switch (type) {
    case R_AARCH64_RELATIVE:
        *ptr = mod->base + addend;
        break;

    case R_ARM_RELATIVE:
        *ptr += mod->base;
        break;

    case R_AARCH64_ABS64:
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_JUMP_SLOT:
    case R_ARM_ABS32:
    case R_ARM_GLOB_DAT:
    case R_ARM_JUMP_SLOT: {
        if (sym->st_shndx != SHN_UNDEF) {
            if (type == R_ARM_ABS32)
                *ptr += mod->text_base + sym->st_value;
            else
                *ptr = mod->base + sym->st_value + addend;
        } else {
            uintptr_t link = so_resolve_link(mod, mod->dynstr + sym->st_name);

            if (link) {
                if (type == R_ARM_ABS32 || type == R_AARCH64_ABS64)
                    *ptr += link;
                else
                    *ptr = link;
            } else if ((sym->st_info >> 4) == 2 /* STB_WEAK */
                       && type != R_ARM_JUMP_SLOT && type != R_AARCH64_JUMP_SLOT) {
                // An undefined weak symbol (e.g. a thread_local init function, _ZTH...) is allowed to be absent:
                // the code checks for null. Resolve it to null like a real dynamic linker, and don't report it.
                if (type == R_AARCH64_ABS64)
                    *ptr = (uintptr_t)addend;
                else if (type != R_ARM_ABS32) // R_ARM_ABS32 keeps its implicit addend in place
                    *ptr = 0;
            } else {
                if (type == R_ARM_JUMP_SLOT || type == R_AARCH64_JUMP_SLOT)
                    *ptr = (uintptr_t)&plt0_stub;
                // An optional (weak) import that's absent is normal; calling it still hits the stub.
                if ((sym->st_info >> 4) != 2 /* STB_WEAK */)
                    warning("Missing: %s (%p, %p)\n", mod->dynstr + sym->st_name, ptr, (void*)*ptr);
            }
        }
        break;
    }

    default:
        fatal_error("Error unknown relocation type %d\n", type);
        break;
    }
}

// invert logic a bit
static inline int64_t consume_sleb128(uint8_t*& cursor, uint8_t* last)
{
    int64_t dec = 0;
    int next = read_sleb128_to_int64(cursor, last, &dec);
    cursor += next;
    return dec;
}

int helper_foreach_droid_rel(so_module* mod, uint8_t* rel, size_t bytes, rela_functor functor)
{
    Elf_Rela reloc;
    uint8_t* cursor = rel;
    uint8_t* last = rel + bytes;

    // Bionic: tools/relocation_packer/README.TXT
    if (strncmp((char*)cursor, "APA1", 4) == 0) {
        cursor += 4;
        int64_t pairs = 0, addr = 0, addend = 0;
        pairs = consume_sleb128(cursor, last);
        while (pairs) {
            addr += consume_sleb128(cursor, last);
            addend += consume_sleb128(cursor, last);

            // This is the unpacked relocation - pass it to the generic function
            reloc = (Elf_Rela) { (Elf_Addr)addr, R_ARM_RELATIVE, addend };
            if (functor(mod, &reloc))
                return 1;
            pairs--;
        }
    } else if (strncmp((char*)cursor, "APR1", 4) == 0) {
        cursor += 4;
        int64_t pairs = 0, addr = 0, count = 0, delta = 0;
        pairs = consume_sleb128(cursor, last);
        addr = consume_sleb128(cursor, last);

        // This is the unpacked relocation - pass it to the generic function
        reloc = (Elf_Rela) { (Elf_Addr)addr, R_ARM_RELATIVE, 0 };
        if (functor(mod, &reloc))
            return 1;

        while (pairs) {
            count = consume_sleb128(cursor, last);
            delta = consume_sleb128(cursor, last);
            while (count) {
                addr += delta;

                // This is the unpacked relocation - pass it to the generic function
                reloc = (Elf_Rela) { (Elf_Addr)addr, R_ARM_RELATIVE, 0 };
                if (functor(mod, &reloc))
                    return 1;
                count--;
            }

            pairs--;
        }
    } else if (strncmp((char*)cursor, "APS2", 4) == 0) {
        cursor += 4;
        int64_t groupFlagsAddend = 0, info = 0, count = 0, groupSize = 0, offset = 0, addend = 0, groupFlags = 0, groupOffsetDelta = 0;
        count = consume_sleb128(cursor, last);
        offset += consume_sleb128(cursor, last);

        while (count) {
            if (groupSize <= 0) {
                groupOffsetDelta = 0;
                groupSize = consume_sleb128(cursor, last);
                groupFlags = consume_sleb128(cursor, last);

                if (groupFlags & RELOCATION_GROUPED_BY_OFFSET_DELTA_FLAG)
                    groupOffsetDelta = consume_sleb128(cursor, last);
                if (groupFlags & RELOCATION_GROUPED_BY_INFO_FLAG)
                    info = consume_sleb128(cursor, last);

                groupFlagsAddend = groupFlags & (RELOCATION_GROUP_HAS_ADDEND_FLAG | RELOCATION_GROUPED_BY_ADDEND_FLAG);
                if (groupFlagsAddend == RELOCATION_GROUP_HAS_ADDEND_FLAG) {
                    /* */
                } else if (groupFlagsAddend == (RELOCATION_GROUP_HAS_ADDEND_FLAG | RELOCATION_GROUPED_BY_ADDEND_FLAG)) {
                    addend += consume_sleb128(cursor, last);
                } else {
                    addend = 0;
                }
            }

            if (groupFlags & RELOCATION_GROUPED_BY_OFFSET_DELTA_FLAG) {
                offset += groupOffsetDelta;
            } else {
                offset += consume_sleb128(cursor, last);
            }
            if ((groupFlags & RELOCATION_GROUPED_BY_INFO_FLAG) == 0)
                info = consume_sleb128(cursor, last);

            if (groupFlagsAddend == RELOCATION_GROUP_HAS_ADDEND_FLAG) {
                addend += consume_sleb128(cursor, last);
            }

            // This is the unpacked relocation - pass it to the generic function
            reloc = (Elf_Rela) { (Elf_Addr)offset, (Elf_Addr)info, addend };
            if (functor(mod, &reloc))
                return 1;

            count--;
            groupSize--;
        }
    }

    return 0;
}

void foreach_rela(so_module* mod, rela_functor functor)
{
    if (mod->droidreldyn && helper_foreach_droid_rel(mod, mod->droidreldyn, mod->num_droidreldyn, functor))
        return;
    if (mod->droidreladyn && helper_foreach_droid_rel(mod, mod->droidreladyn, mod->num_droidreladyn, functor))
        return;
    if (mod->num_reladyn > 0 && helper_foreach_rel(mod, mod->reladyn, mod->num_reladyn, functor))
        return;
    if (mod->num_relaplt > 0 && helper_foreach_rel(mod, mod->relaplt, mod->num_relaplt, functor))
        return;
    if (mod->num_reldyn > 0 && helper_foreach_rel(mod, mod->reldyn, mod->num_reldyn, functor))
        return;
    if (mod->num_relplt > 0 && helper_foreach_rel(mod, mod->relplt, mod->num_relplt, functor))
        return;
}

void so_relr_relocate(so_module* mod)
{
    // Not really all that useful to have a foreach for this when you don't really
    // have info you can extract from relr, right?
    Elf_Addr* where = (Elf_Addr*)mod->base;
    Elf_Relr* relr = (Elf_Relr*)mod->relr;
    Elf_Relr* relrlim = &relr[mod->num_relr];
    for (; relr < relrlim; relr++) {
        Elf_Relr entry = *relr;

        if ((entry & 1) == 0) {
            where = (Elf_Addr*)(mod->base + entry);
            *where++ += (Elf_Addr)mod->base;
        } else {
            for (Elf_Word i = 0; (entry >>= 1) != 0; i++)
                if ((entry & 1) != 0)
                    where[i] += (Elf_Addr)mod->base;
            where += 8 * sizeof(Elf_Relr) - 1;
        }
    }
}

void so_relocate_all(so_module* mod)
{
    so_relr_relocate(mod);
    foreach_rela(mod, [](so_module* mod, const Elf_Rela* rel) -> int {
        so_relocate_rel(mod, rel);
        return 0;
    });
}

uintptr_t so_resolve_link(so_module* mod, const char* symbol)
{
    if (symbol != NULL && strlen(symbol) >= 3 && symbol[0] == 'g' && symbol[1] == 'l' && symbol[2] != 'a') {
        size_t new_len = strlen("glad_") + strlen(symbol) + 1;
        char* glad_symbol = (char*)malloc(new_len);
        if (glad_symbol != NULL) {
            strcpy(glad_symbol, "glad_");
            strcat(glad_symbol, symbol);
            uintptr_t new_addr = so_resolve_link(nullptr, glad_symbol);
            if(new_addr)
                return new_addr;
        }
    }

    // Look through the provided host functionality first
    for (int i = 0; so_dynamic_libraries[i] != NULL; i++) {
        DynLibFunction* funcs = so_dynamic_libraries[i];
        for (int j = 0; funcs[j].symbol != NULL; j++) {
            if (strcmp(symbol, funcs[j].symbol) == 0) {
                return funcs[j].func;
            }
        }
    }

    // If module is passed, try to first find it in the bodule
    so_module* curr = head;
    while (curr) {
        if (curr == mod) {
            uintptr_t link = so_symbol(curr, symbol);
            if (link)
                return link;
        }
        curr = curr->next;
    }


    // if(mod != NULL && mod != (void*)0xDEAD)
    //     return 0;

    //General search
    curr = head;
    while (curr) {
            uintptr_t link = so_symbol(curr, symbol);
            if (link)
                return link;
        curr = curr->next;
    }

    return 0;
}

int so_static_overrides(so_module* mod)
{
    // Will look for symbols inside the modules which are included in one of the
    // static patches provided in main.c.
    // This will patch out all statically compiled symbols it can find with one of
    // the available bridges. E.g., for fixing issues with broken audio or fonts on
    // certain GameMaker: Studio runner versions.
    for (int i = 0; so_static_patches[i] != NULL; i++) {
        DynLibFunction* funcs = so_static_patches[i];

        for (int j = 0; funcs[j].symbol != NULL; j++) {
            uintptr_t addr = so_symbol(mod, funcs[j].symbol);
            if (addr) {
                hook_address(mod, addr, funcs[j].func);
            }
        }
    }

    return 0;
}

void so_initialize(so_module* mod)
{
    for (int i = 0; i < mod->num_init_array; i++) {
        if (mod->init_array[i]) {
            mod->init_array[i]();
        }
    }
}

uint32_t so_hash(const uint8_t* name)
{
    uint64_t h = 0, g;
    while (*name) {
        h = (h << 4) + *name++;
        if ((g = (h & 0xf0000000)) != 0)
            h ^= g >> 24;
        h &= 0x0fffffff;
    }
    return h;
}

int so_symbol_index(so_module* mod, const char* symbol)
{
    if (mod->hash) {
        uint32_t hash = so_hash((const uint8_t*)symbol);
        uint32_t nbucket = mod->hash[0];
        uint32_t* bucket = &mod->hash[2];
        uint32_t* chain = &bucket[nbucket];
        for (int i = bucket[hash % nbucket]; i; i = chain[i]) {
            if (mod->dynsym[i].st_shndx == SHN_UNDEF)
                continue;
            if (mod->dynsym[i].st_info != SHN_UNDEF && strcmp(mod->dynstr + mod->dynsym[i].st_name, symbol) == 0)
                return i;
        }
    }

    for (int i = 0; i < mod->num_dynsym; i++) {
        if (mod->dynsym[i].st_shndx == SHN_UNDEF)
            continue;
        if (mod->dynsym[i].st_info != SHN_UNDEF && strcmp(mod->dynstr + mod->dynsym[i].st_name, symbol) == 0)
            return i;
    }

    return -1;
}

uintptr_t so_symbol(so_module* mod, const char* symbol)
{
    if (mod != NULL) {
        int index = so_symbol_index(mod, symbol);
        if (index >= 0)
            return mod->base + mod->dynsym[index].st_value;
    } else {
        so_module* cur = head;
        while (cur) {
            int index = so_symbol_index(cur, symbol);
            if (index >= 0)
                return cur->base + cur->dynsym[index].st_value;

            cur = cur->next;
        }
    }

    return 0;
}

#if !defined(__arm__)
void so_symbol_fix_ldmia(so_module* mod, const char* symbol)
{
    // For non-arm[hf] targets, there's not really any point in doing this hack
}
#endif

void hook_symbol(so_module* mod, const char* symbol, uintptr_t dst, int is_optional)
{
    uintptr_t address = so_symbol(mod, symbol);
    if (address) {
        hook_address(mod, address, dst);
    } else {
        if (!is_optional) {
            fatal_error("Failed to perform hook for symbol \"%s\"!\n", symbol);
            exit(-1);
        } else {
            warning("Failed to perform hook for symbol \"%s\"!\n", symbol);
        }
    }
}

void hook_symbols(so_module* mod, DynLibHooks* hooks)
{
    for (int i = 0; hooks[i].symbol != NULL; i++) {
        hook_symbol(mod, hooks[i].symbol, hooks[i].hook, hooks[i].opt);
    }
}
///// On-demand library loading (dlopen, System.loadLibrary, [libraries] preload)

#include <mutex>
#include <set>
#include <string>
#include <fcntl.h>

namespace {
    std::recursive_mutex g_libMutex;
    uintptr_t g_nextLibAddr = 0x5000000000;     // above the fixed addresses main.cpp uses for Unity's own libraries
    std::set<so_module*> g_jniOnLoadDone;

    std::string so_basename(const std::string& p)
    {
        size_t s = p.rfind('/');
        return s == std::string::npos ? p : p.substr(s + 1);
    }

    so_module* so_find_loaded(const std::string& base)
    {
        for (so_module* m = head; m; m = m->next)
            if (m->path && so_basename(m->path) == base)
                return m;
        return nullptr;
    }

    // The game's copy of a library: a path as given, or lib/arm64-v8a/<name>, or the System.loadLibrary form
    // "foo" -> lib/arm64-v8a/libfoo.so. Empty if the game doesn't ship it (then it's an Android system library).
    std::string so_find_library_file(const std::string& name)
    {
        if (name.find('/') != std::string::npos && access(name.c_str(), R_OK) == 0)
            return name;
        std::string base = so_basename(name);
        std::string candidates[] = { BOGO_LIB_DIR "/" + base, BOGO_LIB_DIR "/lib" + base + ".so" };
        for (auto& c : candidates)
            if (access(c.c_str(), R_OK) == 0)
                return c;
        return "";
    }

    // Reads DT_NEEDED and the loaded size from the ELF headers without loading anything.
    bool so_read_deps(const std::string& path, std::vector<std::string>& needed, size_t& span)
    {
        int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) return false;
        Elf_Ehdr eh;
        bool ok = pread(fd, &eh, sizeof(eh), 0) == (ssize_t)sizeof(eh) && memcmp(eh.e_ident, ELFMAG, SELFMAG) == 0 && eh.e_type == ET_DYN;
        std::vector<Elf_Phdr> ph(ok ? eh.e_phnum : 0);
        if (ok) ok = pread(fd, ph.data(), sizeof(Elf_Phdr) * ph.size(), eh.e_phoff) == (ssize_t)(sizeof(Elf_Phdr) * ph.size());
        if (!ok) { close(fd); return false; }
        size_t minV = ~(size_t)0, maxV = 0;
        const Elf_Phdr* dyn = nullptr;
        for (auto& p : ph) {
            if (p.p_type == PT_LOAD) {
                minV = std::min<size_t>(minV, p.p_vaddr);
                maxV = std::max<size_t>(maxV, p.p_vaddr + p.p_memsz);
            } else if (p.p_type == PT_DYNAMIC) {
                dyn = &p;
            }
        }
        span = maxV > minV ? maxV - minV : 0;
        auto toOffset = [&](uintptr_t v) -> off_t {
            for (auto& p : ph)
                if (p.p_type == PT_LOAD && v >= p.p_vaddr && v < p.p_vaddr + p.p_filesz)
                    return (off_t)(v - p.p_vaddr + p.p_offset);
            return -1;
        };
        if (dyn) {
            std::vector<Elf_Dyn> d(dyn->p_filesz / sizeof(Elf_Dyn));
            if (pread(fd, d.data(), d.size() * sizeof(Elf_Dyn), dyn->p_offset) == (ssize_t)(d.size() * sizeof(Elf_Dyn))) {
                uintptr_t strtab = 0;
                size_t strsz = 0;
                for (auto& e : d) {
                    if (e.d_tag == DT_STRTAB) strtab = e.d_un.d_ptr;
                    if (e.d_tag == DT_STRSZ) strsz = e.d_un.d_val;
                }
                off_t so = strtab ? toOffset(strtab) : -1;
                std::string strs(strsz, '\0');
                if (so >= 0 && strsz && pread(fd, &strs[0], strsz, so) == (ssize_t)strsz) {
                    for (auto& e : d)
                        if (e.d_tag == DT_NEEDED && e.d_un.d_val < strsz)
                            needed.push_back(std::string(strs.c_str() + e.d_un.d_val));
                }
            }
        }
        close(fd);
        return true;
    }
}

bool load_so_from_file(so_module *mod, const char *filename, uintptr_t addr, bool optional = false);

so_module* so_load_library(const char* name, void* java_vm)
{
    if (!name || !*name)
        return nullptr;
    std::lock_guard<std::recursive_mutex> lock(g_libMutex);
    std::string base = so_basename(name);
    so_module* mod = so_find_loaded(base);
    if (!mod && base.find(".so") == std::string::npos)
        mod = so_find_loaded("lib" + base + ".so");
    bool loadedNow = false;
    if (!mod) {
        std::string path = so_find_library_file(name);
        std::vector<std::string> needed;
        size_t span = 0;
        if (path.empty() || !so_read_deps(path, needed, span))
            return nullptr;
        // Dependencies first, so its imports resolve. Ones the game doesn't ship are Android system libraries,
        // which the thunk tables provide.
        for (auto& dep : needed)
            if (!so_find_loaded(so_basename(dep)) && !so_find_library_file(dep).empty())
                so_load_library(dep.c_str(), java_vm);
        mod = new so_module();
        uintptr_t addr = g_nextLibAddr;
        g_nextLibAddr += ((span + PATCH_SZ + 0x1000000) + 0x3FFFFFFF) & ~(uintptr_t)0x3FFFFFFF; // 1 GiB slots
        if (!load_so_from_file(mod, path.c_str(), addr) || !mod->path) {
            fprintf(stderr, "[DLOPEN] failed to load %s\n", path.c_str());
            delete mod;
            return nullptr;
        }
        printf("[DLOPEN] loaded %s at 0x%lx\n", path.c_str(), (unsigned long)addr);
        loadedNow = true;
    }
    // Android calls JNI_OnLoad when Java loads a library (System.loadLibrary), once. Unity's own libraries were
    // loaded and initialised by the loader at startup and aren't touched here.
    if (java_vm && (loadedNow || g_jniOnLoadDone.count(mod) == 0) && (loadedNow || mod->base >= 0x5000000000)) {
        g_jniOnLoadDone.insert(mod);
        uintptr_t onLoad = so_symbol(mod, "JNI_OnLoad");
        if (onLoad) {
            printf("[DLOPEN] calling JNI_OnLoad of %s\n", mod->path);
            ((int (*)(void*, void*))onLoad)(java_vm, nullptr);
        }
    }
    return mod;
}
