/* Point d'entree Nintendo DS (libnds) */
#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <time.h>
#include "game.h"

static int sd_ok;
#define SAVE_PATH "fat:/GrammaQuest_DS.sav"

int plat_save_available(void) { return sd_ok; }
int plat_save_read(void *buf, int n) {
    FILE *f = fopen(SAVE_PATH, "rb");
    if (!f) return 0;
    size_t r = fread(buf, 1, (size_t)n, f);
    fclose(f);
    return r == (size_t)n;
}
int plat_save_write(const void *buf, int n) {
    FILE *f = fopen(SAVE_PATH, "wb");
    if (!f) return 0;
    size_t r = fwrite(buf, 1, (size_t)n, f);
    fclose(f);
    return r == (size_t)n;
}

int main(void) {
    lcdMainOnTop();                       /* ecran du haut = affichage seul */

    videoSetMode(MODE_FB0);               /* haut : framebuffer 256x192 */
    vramSetBankA(VRAM_A_LCD);

    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);   /* bas : fond bitmap 16 bits (tactile) */
    vramSetBankC(VRAM_C_SUB_BG);
    int bg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    sd_ok = fatInitDefault();             /* carte SD (R4...) pour la sauvegarde */

    game_init((uint16_t *)VRAM_A, (uint16_t *)bgGetGfxPtr(bg), (unsigned)time(NULL));

    touchPosition touch;
    while (1) {
        swiWaitForVBlank();
        scanKeys();
        int down = keysDown(), held = keysHeld();
        Input in = {0};
        in.down_a = (down & KEY_A) != 0;
        in.down_b = (down & KEY_B) != 0;
        in.down_y = (down & KEY_Y) != 0;
        in.down_start = (down & KEY_START) != 0;
        in.down_select = (down & KEY_SELECT) != 0;
        in.held_l = (held & KEY_L) != 0;
        in.held_r = (held & KEY_R) != 0;
        if (down & KEY_TOUCH) {
            touchRead(&touch);
            in.touch_down = 1;
            in.tx = touch.px;
            in.ty = touch.py;
        }
        game_frame(&in);
    }
    return 0;
}
