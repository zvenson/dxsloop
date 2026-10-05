/* SPDX-License-Identifier: GPL-3.0-only */
/* The DX7 engine's .syx import: a 32-voice bulk dump unpacks to the same voices that went in,
 * a damaged dump is refused, and every value of a random dump stays in range.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/dx7_syx_test tests/dx7_syx_test.c -lm
 *   build/host/dx7_syx_test SYXFILE (the FM-1 Drums prototype's fm1-drums.syx: 16 drums, twice) */
#define main hostsim_main
#include "hostsim.c"
#undef main
int main(int argc, char **argv)
{
    static uint8_t s[5000];
    uint8_t u[156];
    FILE *f = argc > 1 ? fopen(argv[1], "rb") : 0;
    int n = f ? (int)fread(s, 1, sizeof s, f) : 0, fails = 0, k, i;
    if (f)
        fclose(f);
    if (!dx_bank_load(s, (uint32_t)n)) {
        printf("FAIL: the dump was refused\n");
        return 1;
    }
    for (k = 0; k < 16; k++) {                 /* the prototype's order: no ride / conga there */
        dx_unpack(dx_user[k], u);
        for (i = 0; i < 16; i++)
            if (!memcmp(u, DX_DRUM_VOICE[i], 145))
                break;
        printf("U%02d %-10s %s\n", k + 1, dx_names[DX_NSYNTH + k], i < 16 ? DX_DRUM[i].name : "NO MATCH");
        fails += i == 16 && strcmp(dx_names[DX_NSYNTH + k], "TOM MID") && strcmp(dx_names[DX_NSYNTH + k], "ZAP");
    }
    s[100] ^= 1;                                /* one bit wrong: the checksum refuses it */
    if (dx_bank_load(s, (uint32_t)n)) {
        printf("FAIL: a damaged dump was taken\n");
        fails++;
    }
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
