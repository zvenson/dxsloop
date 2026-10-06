/* SPDX-License-Identifier: GPL-3.0-only */
/* The DX7 engine's .syx import: a 32-voice bulk dump unpacks to the same voices that went in,
 * a damaged dump is refused, and every value of a random dump stays in range.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/dx7_syx_test tests/dx7_syx_test.c -lm
 *   build/host/dx7_syx_test [SYXFILE]
 * Without a file the dump is packed here from the built-in voices (17 synth + 15 drums). */
#define main hostsim_main
#include "hostsim.c"
#undef main

/* the DX7's 128-byte packed voice (the inverse of dx_unpack) */
static void dx_pack(const uint8_t *u, uint8_t *b)
{
    uint32_t op, i;
    for (op = 0; op < 6u; op++) {
        const uint8_t *o = u + op * 21u;
        uint8_t *q = b + op * 17u;
        for (i = 0; i < 11u; i++)
            q[i] = o[i];
        q[11] = (uint8_t)(o[11] | o[12] << 2);
        q[12] = (uint8_t)(o[13] | o[20] << 3);
        q[13] = (uint8_t)(o[14] | o[15] << 2);
        q[14] = o[16];
        q[15] = (uint8_t)(o[17] | o[18] << 1);
        q[16] = o[19];
    }
    for (i = 0; i < 8u; i++)
        b[102 + i] = u[126 + i];
    b[110] = u[134];
    b[111] = (uint8_t)(u[135] | u[136] << 3);
    b[112] = u[137], b[113] = u[138], b[114] = u[139], b[115] = u[140];
    b[116] = (uint8_t)(u[141] | u[142] << 1 | u[143] << 4);
    b[117] = u[144];
    for (i = 0; i < 10u; i++)
        b[118 + i] = u[145 + i];
}
static const uint8_t *src_voice(uint32_t k)
{
    return k < DX_NSYNTH ? DX_SYNTH[k] : DX_DRUM_VOICE[k - DX_NSYNTH];
}

int main(int argc, char **argv)
{
    static uint8_t s[5000];
    uint8_t u[156];
    FILE *f = argc > 1 ? fopen(argv[1], "rb") : 0;
    int n, fails = 0, k, i;
    if (f) {
        n = (int)fread(s, 1, sizeof s, f);
        fclose(f);
    } else {                                    /* a dump of the built-in voices */
        uint32_t sum = 0;
        s[0] = 0xF0, s[1] = 0x43, s[2] = 0, s[3] = 9, s[4] = 0x20, s[5] = 0;
        for (k = 0; k < 32; k++)
            dx_pack(src_voice((uint32_t)k), s + 6 + 128 * k);
        for (i = 0; i < 4096; i++)
            sum += s[6 + i];
        s[4102] = (uint8_t)((128u - (sum & 127u)) & 127u);
        s[4103] = 0xF7;
        n = 4104;
    }
    if (!dx_bank_load(s, (uint32_t)n)) {
        printf("FAIL: the dump was refused\n");
        return 1;
    }
    for (k = 0; k < 32; k++) {
        dx_unpack(dx_user[k], u);
        if (f) {
            printf("U%02d %s\n", k + 1, dx_names[DX_NSYNTH + k]);
            continue;
        }
        if (memcmp(u, src_voice((uint32_t)k), 155)) {
            printf("FAIL: U%02d %s does not unpack to what went in\n", k + 1, dx_names[DX_NSYNTH + k]);
            fails++;
        }
    }
    {   /* the editor's way (BANK_BEGIN, eight BANK_WRITE of 512, BANK_END): the same bank, the names */
        int off;
        dx_bank_begin();
        if (dx_user_ok || strcmp(dx_names[DX_NSYNTH + 4], "U05"))
            fails++, printf("FAIL: during an upload the bank must be empty (U05 = %s)\n", dx_names[DX_NSYNTH + 4]);
        for (off = 0; off < 4096; off += 512)
            if (dx_bank_write((uint32_t)off, s + 6 + off, 512))
                fails++, printf("FAIL: BANK_WRITE at %d refused\n", off);
        if (dx_bank_write(4095, s + 6, 2) == 0 || dx_bank_write(0, s + 6, 0) == 0)
            fails++, printf("FAIL: a write past the bank or an empty one was taken\n");
        if (dx_bank_end(s[4102]) || !dx_user_ok)
            fails++, printf("FAIL: BANK_END refused a right checksum\n");
        for (k = 0; k < 32; k++) {
            dx_unpack(dx_user[k], u);
            if (!f && memcmp(u, src_voice((uint32_t)k), 155))
                fails++, printf("FAIL: U%02d after the pieces is not what went in\n", k + 1);
        }
        if (dx_bank_write(0, s + 6, 1) == 0)
            fails++, printf("FAIL: a write without BANK_BEGIN was taken\n");
        dx_bank_begin();                        /* a wrong checksum: no bank, slot labels again */
        dx_bank_write(0, s + 6, 4096);
        if (dx_bank_end((s[4102] + 1u) & 127u) != 1 || dx_user_ok || strcmp(dx_names[DX_NSYNTH + 31], "U32"))
            fails++, printf("FAIL: a wrong checksum must leave no bank\n");
        if (!dx_bank_load(s, (uint32_t)n))
            fails++, printf("FAIL: the dump again\n");
    }
    s[100] ^= 1;                                /* one bit wrong: the checksum refuses it */
    if (dx_bank_load(s, (uint32_t)n) || dx_user_ok) {
        printf("FAIL: a damaged dump was taken\n");
        fails++;
    }
    s[100] ^= 1;
    {   /* random bytes with a right checksum: every value in range after dx_sanitize */
        uint32_t sum = 0, r = 7;
        s[0] = 0xF0, s[1] = 0x43, s[2] = 0, s[3] = 9, s[4] = 0x20, s[5] = 0, s[4103] = 0xF7;
        for (i = 0; i < 4096; i++) {
            r = r * 1103515245u + 12345u;
            s[6 + i] = (uint8_t)((r >> 16) & 127u);
            sum += s[6 + i];
        }
        s[4102] = (uint8_t)((128u - (sum & 127u)) & 127u);
        if (!dx_bank_load(s, 4104))
            fails++, printf("FAIL: random dump refused\n");
        for (k = 0; k < 32; k++) {
            dx_unpack(dx_user[k], u);
            dx_sanitize(u);
            dxv_t v;
            dx_init(&v, u, 60, 100);            /* must not read past a table */
            int32_t b[DX_N] = {0};
            dx_compute(&v, b, 0);
        }
    }
    printf("%s\n", fails ? "SYX TEST FAILED" : "syx import ok");
    return fails != 0;
}
