/* test_mgb.c — 472-MGB gates: frame parser strictness, mapper policy.
 * Exit 0 all green, 1 any red. Counts printed as `PASS n FAIL m`.
 */
#include "mgb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int npass = 0, nfail = 0;
#define CHECK(cond, name) do { \
    if (cond) { npass++; } \
    else { nfail++; printf ("  FAIL %s\n", name); } \
} while (0)

static void write_tmp (const char *path, const char *content) {
    FILE *f = fopen (path, "w");
    if (f) { fputs (content, f); fclose (f); }
}

/* minimal valid frame: 2 atoms, 1 bond, 0 restraints */
static const char *GOOD_FRAME =
    "S2SAVE1\n"
    "seed 42\n"
    "dt 0.5 cutoff 12 dielectric 1 temp 310 thermostat 2 tau 50 nu 0.02\n"
    "atoms 2\n"
    "8 0 0 0 -0.834 0.00659 3.15061 0 0 0\n"
    "1 0.9572 0 0 0.417 0 0 0 0 0\n"
    "bonds 1\n"
    "0 1 1 0.9572 34.5\n"
    "restraints 0\n";

static int count_bodies = 0, count_bonds = 0, finish_seen = 0;
static int t_body (void *c, int i, int Z, double x, double y, double z,
                   double r, const float rgb[3]) {
    (void)c; (void)i; (void)Z; (void)x; (void)y; (void)z; (void)r; (void)rgb;
    count_bodies++;
    return 0;
}
static int t_bond (void *c, int a, int b) {
    (void)c; (void)a; (void)b;
    count_bonds++;
    return 0;
}
static int t_finish (void *c) {
    (void)c;
    finish_seen = 1;
    return 0;
}
static const mgb_sink_t TSINK = { t_body, t_bond, t_finish };

int main (void) {
    const char *td = "/tmp";
    char p_good[256], p_bad[256];
    snprintf (p_good, sizeof p_good, "%s/mgb_t_good.s2", td);
    snprintf (p_bad, sizeof p_bad, "%s/mgb_t_bad.s2", td);
    mgb_frame_t f;

    /* 1: good frame parses */
    write_tmp (p_good, GOOD_FRAME);
    memset (&f, 0, sizeof f);
    CHECK (mgb_frame_parse (p_good, &f) == 0, "good parses");
    CHECK (f.natoms == 2 && f.nbonds == 1 && f.nrestraints == 0, "good counts");
    CHECK (f.atoms[0].Z == 8 && f.bonds[0].a == 0 && f.bonds[0].b == 1,
           "good content");
    mgb_frame_free (&f);

    /* 2: bad magic rejected */
    write_tmp (p_bad, "S2SAVE0\nseed 42\n");
    memset (&f, 0, sizeof f);
    CHECK (mgb_frame_parse (p_bad, &f) != 0, "bad magic rejected");

    /* 3: truncated atoms rejected */
    write_tmp (p_bad,
               "S2SAVE1\nseed 42\n"
               "dt 0.5 cutoff 12 dielectric 1 temp 310 thermostat 2 tau 50 nu 0.02\n"
               "atoms 2\n"
               "8 0 0 0 -0.834 0.00659 3.15061 0 0 0\n");
    memset (&f, 0, sizeof f);
    CHECK (mgb_frame_parse (p_bad, &f) != 0, "truncated atoms rejected");

    /* 4: bond index OOB rejected */
    write_tmp (p_bad,
               "S2SAVE1\nseed 42\n"
               "dt 0.5 cutoff 12 dielectric 1 temp 310 thermostat 2 tau 50 nu 0.02\n"
               "atoms 1\n"
               "8 0 0 0 -0.834 0.00659 3.15061 0 0 0\n"
               "bonds 1\n"
               "0 7 1 0.9572 34.5\n"
               "restraints 0\n");
    memset (&f, 0, sizeof f);
    CHECK (mgb_frame_parse (p_bad, &f) != 0, "bond OOB rejected");

    /* 5: trailing garbage rejected */
    {
        char big[2048];
        snprintf (big, sizeof big, "%sEXTRA\n", GOOD_FRAME);
        write_tmp (p_bad, big);
    }
    memset (&f, 0, sizeof f);
    CHECK (mgb_frame_parse (p_bad, &f) != 0, "trailing garbage rejected");

    /* 6: missing file rejected */
    memset (&f, 0, sizeof f);
    CHECK (mgb_frame_parse ("/tmp/mgb_t_no_such_file.s2", &f) != 0,
           "missing file rejected");

    /* 7: mapper calls + scale + invertibility */
    write_tmp (p_good, GOOD_FRAME);
    memset (&f, 0, sizeof f);
    CHECK (mgb_frame_parse (p_good, &f) == 0, "remap parses");
    count_bodies = count_bonds = finish_seen = 0;
    CHECK (mgb_map (&f, &TSINK, NULL, 2.0) == 0, "map ok");
    CHECK (count_bodies == 2 && count_bonds == 1 && finish_seen == 1,
           "map counts");
    CHECK (mgb_unmap (1.9144, 2.0) == 0.9572, "scale inverts exactly");
    CHECK (mgb_map (&f, &TSINK, NULL, 0.0) != 0, "zero scale refused");
    CHECK (mgb_map (&f, &TSINK, NULL, -1.0) != 0, "negative scale refused");
    mgb_frame_free (&f);

    /* 8: element table known + fallback */
    {
        double r;
        float rgb[3];
        CHECK (mgb_element_display (8, &r, rgb) == 0, "O known");
        CHECK (mgb_element_display (999, &r, rgb) != 0, "unknown Z fallback");
    }

    /* 9: bogus s2tui path fails closed (no hang: short timeout) */
    {
        mgb_session_t *s = mgb_spawn ("/tmp/mgb_t_no_such_binary", 2);
        CHECK (s == NULL, "bogus binary refused");
        mgb_close (s);  /* NULL-safe */
    }

    remove (p_good);
    remove (p_bad);
    printf ("mgb: PASS %d FAIL %d\n", npass, nfail);
    return nfail ? 1 : 0;
}
