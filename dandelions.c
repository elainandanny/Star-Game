#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten.h>

#ifdef _MSC_VER
#  define ASSERT(c) if (!(c)) __debugbreak();
#  define POPCOUNT64(x) __popcnt64(x)
#elif __GNUC__
#  define ASSERT(c) if (!(c)) __builtin_trap();
#  define POPCOUNT64(x) __builtin_popcountll(x)
#else
#  define ASSERT(c) if (!(c)) *(volatile int *)0 = 0;
#endif

#define GAME_INIT ((uint64_t)255 << 50)
typedef uint64_t game;

#define MEMOIZE_EXP 28
#define MEMOIZE_CAP (1L << MEMOIZE_EXP)
#define FLAG_FULL (1 << 0)
struct memoize {
    uint32_t len;
    int flags;
    game *slots;
};

struct memoize memoize(void) {
    struct memoize mt = {0, 0, calloc(sizeof(game), MEMOIZE_CAP)};
    return mt;
}

static int turn(game g) {
    return POPCOUNT64((g & 0x3fffffffe000000) ^ GAME_INIT);
}

static int empty(game g, int i) {
    return !((g >> (25 + i)) & 1);
}

static int seeded(game g, int i) {
    return g >> i & 1;
}

static int calm(game g, int d) {
    return (g >> (50 + d)) & 1;
}

static int null(game g) {
    return !g;
}

static int score(game g) {
    int gaps = 25 - POPCOUNT64(g & 0x1ffffff);
    return gaps ? gaps : POPCOUNT64(g & 0x3fffffe000000) - 7;
}

static game plant(game g, int i) {
    return g | (uint64_t)1 << (i + 25) | (uint64_t)1 << i;
}

static game clean(game g) {
    return g & 0x3ffffffffffffff;
}

static game store(game g, int v) {
    return g | (uint64_t)(v + 32) << 58;
}

static int load(game g) {
    return (int)(g >> 58) - 32;
}

static game blow(game g, int dir) {
    static uint8_t rotate[] = {31, 26, 27, 28, 1, 6, 5, 4};
    static uint32_t mask[] = {
        0x0f7bdef, 0x007bdef, 0x00fffff, 0x00f7bde,
        0x1ef7bde, 0x1ef7bc0, 0x1ffffe0, 0x0f7bde0
    };
    uint32_t s = g & 0x1ffffff;
    uint32_t f = g >> 25;
    uint32_t m = mask[dir];
    int r = rotate[dir];
    f &= m; f = f >> r | f << (32 - r); s |= f;
    f &= m; f = f >> r | f << (32 - r); s |= f;
    f &= m; f = f >> r | f << (32 - r); s |= f;
    f &= m; f = f >> r | f << (32 - r); s |= f;
    return (g ^ (uint64_t)1 << (dir + 50)) | s;
}

static unsigned char revrot(unsigned char b, int r) {
    static unsigned char t[] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
    b = t[b & 15] << 4 | t[b >> 4];
    b = b >> r | b << (8 - r);
    return b;
}

static game transpose(game g) {
    unsigned char b = revrot(g >> 50, 5);
    return (uint64_t)b << 50 |
           ((g >> 16) & 0x0000020000010) |
           ((g >> 12) & 0x0000410000208) |
           ((g >> 8) & 0x0008208004104) |
           ((g >> 4) & 0x0104104082082) |
           ((g >> 0) & 0x2082083041041) |
           ((g << 4) & 0x1041040820820) |
           ((g << 8) & 0x0820800410400) |
           ((g << 12) & 0x0410000208000) |
           ((g << 16) & 0x0200000100000);
}

static game flipv(game g) {
    unsigned char b = revrot(g >> 50, 7);
    return (uint64_t)b << 50 |
           ((g >> 20) & 0x000003e00001f) |
           ((g >> 10) & 0x00007c00003e0) |
           ((g >> 0) & 0x000f800007c00) |
           ((g << 10) & 0x01f00000f8000) |
           ((g << 20) & 0x3e00001f00000);
}

static game canonicalize(game g) {
    uint64_t c = g;
    g = transpose(g); c = c < g ? c : g;
    g = flipv(g); c = c < g ? c : g;
    g = transpose(g); c = c < g ? c : g;
    g = flipv(g); c = c < g ? c : g;
    g = transpose(g); c = c < g ? c : g;
    g = flipv(g); c = c < g ? c : g;
    g = transpose(g); c = c < g ? c : g;
    return c;
}

static uint64_t hash(game g) {
    return g ^ g >> 32;
}

EMSCRIPTEN_KEEPALIVE
void display(game g, char *buffer) {
    int b = g >> 50;
    int idx = 0;
    for (int d = 0; d < 8; d++) {
        buffer[idx++] = (1 << d & b) ? '-' : "01234567"[d];
    }
    buffer[idx++] = '\n';
    int32_t s = g & 0x1ffffff;
    int32_t f = g >> 25;
    for (int i = 0; i < 25; i++) {
        int32_t b = (int32_t)1 << i;
        buffer[idx++] = "_.*"[!!(f & b) + !!(s & b)];
        if (i % 5 == 4) {
            buffer[idx++] = '\n';
        }
    }
    buffer[idx] = '\0';
}

static double usage(struct memoize *mt) {
    return 100.0 * mt->len / MEMOIZE_CAP;
}

static game *lookup(struct memoize *mt, game g) {
    uint64_t h = hash(g);
    size_t m = MEMOIZE_CAP - 1;
    size_t s = h >> (64 - MEMOIZE_EXP) | 1;
    for (size_t i = h;;) {
        i = (i + s) & m;
        if (null(mt->slots[i])) {
            mt->len++;
            ASSERT(mt->len < MEMOIZE_CAP);
            return mt->slots + i;
        }
        if (clean(mt->slots[i]) == g) {
            return mt->slots + i;
        }
    }
}

static int eval(struct memoize *mt, game g) {
    g = canonicalize(g);
    game *k = lookup(mt, g);
    if (!null(*k)) {
        return load(*k);
    }
    int s = score(g);
    if (s <= 0) {
        *k = store(g, s);
        return s;
    }
    int t = turn(g);
    if (t == 14) {
        int s = score(g);
        *k = store(g, s);
        return s;
    }
    int result;
    *k = g;
    if (t & 1) {
        result = 0;
        for (int i = 0; i < 8; i++) {
            if (calm(g, i)) {
                int r = eval(mt, blow(g, i));
                if (r > result) {
                    result = r;
                    *k = store(g, r);
                    if (!(mt->flags & FLAG_FULL)) {
                        return r;
                    }
                }
            }
        }
    } else {
        result = 31;
        for (int i = 0; i < 25; i++) {
            if (empty(g, i)) {
                int r = eval(mt, plant(g, i));
                if (r < result) {
                    result = r;
                    *k = store(g, r);
                    if (!r && !(mt->flags & FLAG_FULL)) {
                        return 0;
                    }
                }
            }
        }
    }
    *k = store(g, result);
    return result;
}

struct option {
    int move;
    int delta;
};

static int cmp(const void *p0, const void *p1) {
    const struct option *a = p0, *b = p1;
    if (a->delta == b->delta) {
        return a->move - b->move;
    }
    return a->delta - b->delta;
}

EMSCRIPTEN_KEEPALIVE
int options(struct memoize *mt, struct option *opts, game g) {
    int len = 0;
    if (turn(g) & 1) {
        for (int i = 0; i < 8; i++) {
            if (calm(g, i)) {
                int s = eval(mt, blow(g, i));
                if (s > 0) {
                    opts[len].move = i;
                    opts[len].delta = -s;
                    len++;
                }
            }
        }
    } else {
        for (int i = 0; i < 25; i++) {
            if (empty(g, i)) {
                int s = eval(mt, plant(g, i));
                if (s <= 0) {
                    opts[len].move = i;
                    opts[len].delta = s;
                    len++;
                }
            }
        }
    }
    qsort(opts, len, sizeof(*opts), cmp);
    return len;
}

EMSCRIPTEN_KEEPALIVE
struct memoize *init_memoize() {
    struct memoize *mt = malloc(sizeof(struct memoize));
    *mt = memoize();
    return mt;
}

EMSCRIPTEN_KEEPALIVE
void free_memoize(struct memoize *mt) {
    free(mt->slots);
    free(mt);
}

EMSCRIPTEN_KEEPALIVE
game plant_move(game g, int pos) {
    if (pos >= 0 && pos < 25 && empty(g, pos)) {
        return plant(g, pos);
    }
    return g;
}

EMSCRIPTEN_KEEPALIVE
game blow_move(game g, int dir) {
    if (dir >= 0 && dir < 8 && calm(g, dir)) {
        return blow(g, dir);
    }
    return g;
}

EMSCRIPTEN_KEEPALIVE
int get_score(game g) {
    return score(g);
}

EMSCRIPTEN_KEEPALIVE
game get_initial_game() {
    return GAME_INIT;
}

EMSCRIPTEN_KEEPALIVE
int get_turn(game g) {
    return turn(g);
}
