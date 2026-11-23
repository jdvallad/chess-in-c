#include "engine/tt.h"

#include <stdlib.h>
#include <string.h>

bool tt_alloc(engine *eng, size_t tt_mb) {
    size_t bytes = (tt_mb > 0 ? tt_mb : 16) * 1024ull * 1024ull;
    size_t buckets = 1;
    while ((buckets * 2) * sizeof(tt_bucket) <= bytes) {
        buckets *= 2;
    }
    eng->tt = (tt_bucket *) calloc(buckets, sizeof(tt_bucket));
    if (!eng->tt) {
        eng->tt_count = 0;
        eng->tt_mask = 0;
        return false;
    }
    eng->tt_count = buckets;
    eng->tt_mask = buckets - 1;
    return true;
}

void tt_clear(engine *eng) {
    if (eng && eng->tt) {
        memset(eng->tt, 0, eng->tt_count * sizeof(tt_bucket));
    }
}

void tt_free(engine *eng) {
    if (eng) {
        free(eng->tt);
        eng->tt = NULL;
        eng->tt_count = 0;
        eng->tt_mask = 0;
    }
}
