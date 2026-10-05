#ifndef GAME_H
#define GAME_H
#include <stdint.h>

typedef struct {
    int down_a, down_b, down_y, down_start, down_select;
    int held_l, held_r;
    int touch_down;   /* le stylet vient de toucher l'ecran */
    int tx, ty;       /* position du stylet (0..255, 0..191) */
} Input;

/* top / bot : buffers 256 pixels de large (RGB15 + bit 15 a 1) */
void game_init(uint16_t *top, uint16_t *bot, unsigned seed);
void game_frame(const Input *in);   /* appelee 60 fois par seconde */

/* fournis par la plate-forme (carte SD sur DS) */
int plat_save_available(void);
int plat_save_read(void *buf, int n);
int plat_save_write(const void *buf, int n);

#endif
