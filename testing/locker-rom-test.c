#include "core.h"
#include "cpu.h"
#include "memory.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct result { uint16_t af, hl, de, sp; uint8_t b, flag; };

static struct result invoke(struct gbcc_core *gbc, uint16_t entry, uint16_t flag_address, uint8_t bank)
{
    gbc->cpu = (struct cpu){0};
    gbc->cpu.reg.pc = entry;
    gbc->cpu.reg.sp = 0xc0e0;
    gbc->cpu.reg.hl = 0xc900;
    gbc->cpu.reg.de = 0xca00;
    gbc->cpu.reg.b = 0xa5;
    gbcc_memory_write(gbc, 0x2000, bank);
    gbc->memory.wram0[0xe0] = 0x00;
    gbc->memory.wram0[0xe1] = 0xc8;
    gbcc_memory_write(gbc, flag_address, 0);
    unsigned cycles = 0;
    do {
        gbcc_emulate_cycle(gbc);
        if (gbc->error) {
            fprintf(stderr, "CPU error at %04x: %s\n", gbc->cpu.reg.pc, gbc->error_msg);
            exit(2);
        }
        assert(++cycles < 100000);
    } while (gbc->cpu.reg.pc != 0xc800 || gbc->cpu.instruction.running);
    return (struct result){gbc->cpu.reg.af, gbc->cpu.reg.hl, gbc->cpu.reg.de,
        gbc->cpu.reg.sp, gbc->cpu.reg.b, gbcc_memory_read(gbc, flag_address)};
}

static void equivalent(struct result old, struct result optimized)
{
    assert(old.af == optimized.af);
    assert(old.hl == optimized.hl);
    assert(old.de == optimized.de);
    assert(old.sp == optimized.sp);
    assert(old.flag == optimized.flag);
    assert(optimized.b == 0xa5);
}

static struct gbcc_core *load(const char *path)
{
    struct gbcc_core *gbc = calloc(1, sizeof(*gbc));
    assert(gbc);
    gbcc_initialise(gbc, path);
    assert(!gbc->error);
    gbc->keys.turbo = true;
    gbc->sync_to_video = false;
    gbcc_memory_write(gbc, 0xff40, 0);
    gbcc_memory_write(gbc, 0xffff, 0);
    return gbc;
}

static void texts(struct gbcc_core *gbc, const uint8_t *left, const uint8_t *right)
{
    memcpy(gbc->memory.wram0 + 0x900, left, 32);
    memcpy(gbc->memory.wram0 + 0xa00, right, 32);
}

static bool equal(const uint8_t *left, const uint8_t *right)
{
    for (unsigned i = 0; i < 32; ++i) {
        if (left[i] != right[i]) return false;
        if (left[i] == 0x50) return true;
    }
    abort();
}

static void party(struct gbcc_core *gbc, const unsigned *order)
{
    const char *names[] = {"This", "is", "My", "Pass", "Word", "!!!"};
    for (unsigned mon = 0; mon < 6; ++mon) {
        uint8_t *dest = gbc->memory.wram_bank[1] + 0xe42 + mon * 11;
        memset(dest, 0x50, 11);
        const char *name = names[order[mon]];
        for (unsigned j = 0; name[j]; ++j) {
            char ch = name[j];
            dest[j] = ch >= 'A' && ch <= 'Z' ? 0x80 + ch - 'A' :
                ch >= 'a' && ch <= 'z' ? 0xa0 + ch - 'a' : 0xe7;
        }
    }
}

static bool next(unsigned *order)
{
    int i = 4;
    while (i >= 0 && order[i] > order[i + 1]) --i;
    if (i < 0) return false;
    int j = 5;
    while (order[j] < order[i]) --j;
    unsigned temp = order[i]; order[i] = order[j]; order[j] = temp;
    for (int a = i + 1, b = 5; a < b; ++a, --b) {
        temp = order[a]; order[a] = order[b]; order[b] = temp;
    }
    return true;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    assert(argc == 11);
    struct gbcc_core *old = load(argv[1]), *optimized = load(argv[2]);
    uint16_t old_compare = strtoul(argv[3], NULL, 16);
    uint16_t new_compare = strtoul(argv[4], NULL, 16);
    uint16_t old_party = strtoul(argv[5], NULL, 16);
    uint16_t new_party = strtoul(argv[6], NULL, 16);
    uint16_t old_flag = strtoul(argv[7], NULL, 16);
    uint16_t new_flag = strtoul(argv[8], NULL, 16);
    uint8_t old_bank = strtoul(argv[9], NULL, 16);
    uint8_t new_bank = strtoul(argv[10], NULL, 16);
    uint8_t left[32], right[32];
    memset(left, 0x50, sizeof(left));
    memset(right, 0x50, sizeof(right));
    for (unsigned l = 0; l < 256; ++l) for (unsigned r = 0; r < 256; ++r) {
        left[0] = l; right[0] = r;
        texts(old, left, right); texts(optimized, left, right);
        struct result a = invoke(old, old_compare, old_flag, old_bank);
        struct result b = invoke(optimized, new_compare, new_flag, new_bank);
        equivalent(a, b);
        assert(!!(b.af & 0x80) == (l == r));
    }
    srand(42);
    for (unsigned test = 0; test < 1024; ++test) {
        memset(left, 0x50, sizeof(left));
        unsigned length = test % 11;
        for (unsigned i = 0; i < length; ++i) {
            do { left[i] = (uint8_t)rand(); } while (left[i] == 0x50);
        }
        memcpy(right, left, sizeof(right));
        if (test & 1) right[test % (length + 1)] ^= 1;
        texts(old, left, right); texts(optimized, left, right);
        struct result a = invoke(old, old_compare, old_flag, old_bank);
        struct result b = invoke(optimized, new_compare, new_flag, new_bank);
        equivalent(a, b);
        assert(!!(b.af & 0x80) == equal(left, right));
    }
    unsigned order[] = {0, 1, 2, 3, 4, 5};
    unsigned permutations = 0, accepted = 0;
    do {
        party(old, order); party(optimized, order);
        struct result a = invoke(old, old_party, old_flag, old_bank);
        struct result b = invoke(optimized, new_party, new_flag, new_bank);
        equivalent(a, b);
        bool expected = true;
        for (unsigned i = 0; i < 6; ++i) expected &= order[i] == i;
        assert((b.flag == 99) == expected);
        if (b.flag == 99) ++accepted;
        ++permutations;
    } while (next(order));
    assert(permutations == 720 && accepted == 1);
    printf("PASS: 65536 byte pairs, 1024 strings, 720 party permutations; one accepted order; AF/HL/DE/SP/flag equivalent; B preserved.\n");
    gbcc_free(old); gbcc_free(optimized);
    free(old); free(optimized);
    return 0;
}
