/* Test "singe" : entrees aleatoires pendant des centaines de milliers d'images, sous ASAN/UBSAN */
#include "../source/game.c"
#include "../source/gamedata.c"
#include "../source/font_data.c"
static uint16_t T[256*256], B[256*256];
static uint8_t fakesd[sizeof(SaveFile) + 64]; static int fakesd_ok = 0;
int plat_save_available(void) { return 1; }
int plat_save_read(void *b, int n) { if (!fakesd_ok) return 0; memcpy(b, fakesd, n); return 1; }
int plat_save_write(const void *b, int n) { memcpy(fakesd, b, n); fakesd_ok = 1; return 1; }
int main(int argc, char **argv) {
    unsigned long seed = argc > 1 ? strtoul(argv[1], 0, 10) : 1;
    srand(seed);
    game_init(T, B, seed);
    long frames = 600000;
    for (long f = 0; f < frames; f++) {
        Input in = {0};
        int r = rand() % 100;
        if (r < 14) { in.touch_down = 1; in.tx = rand() % 256; in.ty = rand() % 192; }
        else if (r < 17) in.down_a = 1;
        else if (r < 18) in.down_b = 1;
        else if (r < 18 + (f % 5000 < 100 ? 3 : 0)) in.down_start = 1;
        else if (r == 90 && f % 400 == 0) in.down_select = 1;
        if (rand() % 50 == 0) { in.held_l = in.held_r = 1; in.down_y = rand() % 2; in.down_b = rand() % 2; }
        game_frame(&in);
        if (f % 20000 == 0 && f) game_init(T, B, rand());   /* relance depuis la sauvegarde */
    }
    printf("singe seed %lu : %ld images sans plantage, joueur %s, etat %d\n", seed, frames, P ? P->name : "-", state);
    return 0;
}
