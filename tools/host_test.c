/* Tests sur ordinateur : simule les deux ecrans, joue toutes les modes et exporte des images PPM.
   Compiler avec : gcc -O1 -w -fsanitize=address,undefined -o host_test tools/host_test.c */
#include "../source/game.c"
#include "../source/gamedata.c"
#include "../source/font_data.c"
static uint16_t T[256*256], B[256*256];
static uint8_t fakesd[sizeof(SaveFile) + 64]; static int fakesd_ok = 0, writes = 0;
int plat_save_available(void) { return 1; }
int plat_save_read(void *b, int n) { if (!fakesd_ok) return 0; memcpy(b, fakesd, n); return 1; }
int plat_save_write(const void *b, int n) { memcpy(fakesd, b, n); fakesd_ok = 1; writes++; return 1; }
static void dump(const char *name) {
    for (int k = 0; k < 2; k++) {
        char fn[128]; snprintf(fn, sizeof fn, "/tmp/%s_%s.ppm", name, k ? "bot" : "top");
        FILE *f = fopen(fn, "wb"); fprintf(f, "P6\n256 192\n255\n");
        uint16_t *buf = k ? B : T;
        for (int i = 0; i < 256*192; i++) { int p = buf[i]; fputc((p&31)*255/31, f); fputc(((p>>5)&31)*255/31, f); fputc(((p>>10)&31)*255/31, f); }
        fclose(f);
    }
}
static void frame(Input in) { game_frame(&in); }
static void idle(int n) { Input in = {0}; while (n--) frame(in); }
static void tap(int x, int y) { Input in = {0}; in.touch_down = 1; in.tx = x; in.ty = y; frame(in); idle(1); }
static void tapr(Rect r) { tap(r.x + r.w/2, r.y + r.h/2); }
static void press(int a, int b, int st, int se) { Input in = {0}; in.down_a = a; in.down_b = b; in.down_start = st; in.down_select = se; frame(in); idle(1); }
static void tap_tok(int i) { tapr(wbox[i]); }
static int check(int cond, const char *msg) { if (!cond) { printf("ECHEC : %s (etat %d)\n", msg, state); return 1; } return 0; }
static int fails = 0;
#define CHECK(c) fails += check((c), #c)

/* repond a la question courante (right=1 : juste) */
static void answer(int right) {
    int k = kind();
    if (k == K_TAG || k == K_SEL || k == K_SELTYPE) {
        for (int i = 0; i < ntok; i++) {
            int want = (k == K_TAG) ? (tagv[i] == cat + 1) : (tagv[i] == 1);
            if (!right && i == 0) want = !want;
            if (want) tap_tok(i);
        }
        if (k == K_SELTYPE) { if (nsel() == 0) tap_tok(0); tapr(typebtn(right ? cq->a : (cq->a + 1) % G()->ntypes, G()->ntypes)); }
        else { if (nsel() == 0) tap_tok(0); press(1, 0, 0, 0); }
    } else if (k == K_TF) {
        tapr(tfbtn(right ? cq->a : (cq->a + 1) % 4));
        if (state == S_PLAY) tapr(tfform(cq->b));
    } else {
        const Cat *C = &G()->cats[cat];
        int t = right ? cq->a : (cq->a + 1) % C->nch;
        tapr(C->nch == 2 ? tfform(t) : tfbtn(t));
    }
}
static void menu_tap(void) { if (state == S_PLAY && (kind() == K_TF || kind() == K_GAP)) tapr(BTN_SMALLMENU); else tapr(btn_menu()); }
static void next(void) { press(1, 0, 0, 0); if (state == S_POPUP) { while (state == S_POPUP) press(1, 0, 0, 0); } }

/* clic sur un mode depuis le menu d'un jeu */
static void mode_tile(int i) { Rect r = tile2(i); r.y += 4; tapr(r); }

int main(int argc, char **argv) {
    printf("taille du fichier de sauvegarde : %d octets\n", (int)sizeof(SaveFile));
    game_init(T, B, 7);
    CHECK(state == S_PROFILES); idle(1); dump("p0");
    tapr(tile2(0)); CHECK(state == S_KEYB);
    const char *keys[] = {"L", "É", "A"};
    for (int k = 0; k < 3; k++)
        for (int row = 0; row < 4; row++) for (int col = 0; col < 10; col++)
            if (KB[row][col] && !strcmp(KB[row][col], keys[k])) tapr(kbkey(row, col, 0));
    idle(1); dump("p1");
    tapr(kbkey(4, 2, 0)); CHECK(state == S_HOME); CHECK(!strcmp(P->name, "LÉA")); idle(1); dump("home");

    /* --- menu d'un jeu + cours --- */
    tapr(tile2(1)); CHECK(state == S_GMENU); idle(1); dump("gm0");
    mode_tile(2); CHECK(state == S_GMENU);  /* verrouille : cours pas lu */
    idle(1); dump("gm_lock");
    mode_tile(0); CHECK(state == S_COURS); idle(1); dump("cours1");
    press(1,0,0,0); dump("cours2"); press(1,0,0,0); press(1,0,0,0);
    printf("cours nm : %d pages\n", coursN);
    tapr(cours_back()); CHECK(state == S_GMENU);

    /* --- entrainement nm (verbe) : bon puis faux, serie complete --- */
    mode_tile(2); CHECK(state == S_PICK); idle(1); dump("pick");
    tapr(pick_btn(0)); CHECK(state == S_PLAY); idle(1); dump("play_nm");
    answer(1); CHECK(state == S_RESULT && lastCorrect); idle(1); dump("res_ok");
    next(); answer(0); CHECK(state == S_RESULT && !lastCorrect); idle(1); dump("res_ko");
    for (int q = 2; q < NQ; q++) { next(); answer(1); }
    next(); CHECK(state == S_END || state == S_POPUP); idle(1); dump("end_nm");
    while (state == S_POPUP) { dump("popup"); press(1,0,0,0); }
    CHECK(state == S_END); idle(1); dump("end_nm2");
    CHECK(GS(1)->best[0] == 9);
    tapr(btn_menu()); CHECK(state == S_GMENU);

    /* --- revision --- */
    CHECK(GS(1)->nerr == 1);
    idle(1); dump("gm1");
    mode_tile(1); CHECK(state == S_PLAY && mode == M_REVIEW); idle(1); dump("rev_play");
    answer(1); next(); CHECK(state == S_END); CHECK(GS(1)->nerr == 0); idle(1); dump("rev_end");
    tapr(btn_menu());

    /* --- atelier avec indice --- */
    mode_tile(3); tapr(pick_btn(1)); CHECK(state == S_PLAY && mode == M_ATELIER);
    tapr(btn_hint()); CHECK(hintOn); idle(1); dump("atelier_hint");
    for (int q = 0; q < NQ; q++) { if (q) next(); answer(1); }
    next(); idle(1); dump("atelier_end");
    CHECK(GS(1)->stars[1] == 3);
    while (state == S_POPUP) press(1,0,0,0);
    tapr(btn_menu());

    /* --- defi (palier 3) --- */
    mode_tile(4); CHECK(state == S_PALIER); idle(1); dump("palier");
    tapr(palier_btn(2)); CHECK(state == S_PLAY && mode == M_DEFI); idle(30); dump("defi_play");
    for (int q = 0; q < 6; q++) { answer(1); if (q == 0) { idle(1); dump("defi_ok"); } next(); }
    CHECK(streak == 6);
    idle(tmax + 5); CHECK(state == S_RESULT && timeUp); dump("defi_timeup");
    next(); answer(1); next();
    tapr(btn_menu()); CHECK(state == S_END); idle(1); dump("defi_end"); tapr(btn_menu());

    /* --- blitz --- */
    mode_tile(5); CHECK(state == S_PALIER); tapr(palier_btn(7));
    for (int q = 0; q < 12; q++) { answer(1); next(); }
    idle(1); dump("blitz_play");
    idle(BLITZ_SEC * 60 + 5); CHECK(state == S_END || state == S_POPUP || state == S_RESULT);
    while (state == S_POPUP) press(1,0,0,0);
    idle(1); dump("blitz_end"); if (state == S_END) tapr(btn_menu());
    while (state == S_POPUP) press(1,0,0,0);

    /* --- bilan et trophees --- */
    mode_tile(6); CHECK(state == S_BILAN); idle(1); dump("bilan"); tapr(BTN_BACK2);
    mode_tile(7); CHECK(state == S_TROPH); tapr(troph_cell(5)); idle(1); dump("troph"); tapr(BTN_BACK2);
    printf("nm : points %d, grade %s, trophees %d/%d, taux %d%%\n", GS(1)->points, GR_LAB[grade_of(GS(1)->points)], total_trophies(1), G()->ntroph, rate(1));

    /* --- les 6 autres jeux : entrainement + defi + blitz sans erreur --- */
    const char *ids[] = {"det","nm","suj","tf","co","cc","ho"};
    for (int g = 0; g < 7; g++) {
        if (g == 1) continue;
        press(0,0,1,0); idle(1);      /* start -> accueil */
        while (state != S_HOME) press(0,0,1,0);
        tapr(tile2(g)); CHECK(state == S_GMENU && gi == g);
        mode_tile(0); tapr(cours_back());
        mode_tile(2);
        if (state == S_PICK) tapr(NPICK[g] > 0 && G()->ncat ? pick_btn(g == 6 ? 4 : 1) : list_btn(NPICK[g] - 1));
        CHECK(state == S_PLAY);
        char n[32]; snprintf(n, sizeof n, "%s_play", ids[g]); idle(1); dump(n);
        answer(1); snprintf(n, sizeof n, "%s_res", ids[g]); idle(1); dump(n);
        for (int q = 1; q < NQ; q++) { next(); answer(g % 2 ? 1 : (q % 3 != 0)); }
        next(); while (state == S_POPUP) press(1,0,0,0); CHECK(state == S_END);
        tapr(btn_menu());
        mode_tile(4); if (state == S_PALIER) tapr(palier_btn(7));
        CHECK(state == S_PLAY && mode == M_DEFI);
        for (int q = 0; q < 8; q++) { answer(q % 4 != 3); next(); }
        snprintf(n, sizeof n, "%s_defi", ids[g]); idle(1); dump(n);
        menu_tap(); while (state == S_POPUP) press(1,0,0,0); menu_tap();
        mode_tile(5); if (state == S_PALIER) tapr(palier_btn(7));
        CHECK(mode == M_BLITZ);
        for (int q = 0; q < 6; q++) { answer(1); next(); }
        menu_tap(); while (state == S_POPUP) press(1,0,0,0); menu_tap();
        printf("%s : points %d, trophees %d/%d, taux %d%% (%d reponses)\n", ids[g], GS(g)->points, total_trophies(g), GAMES[g].ntroph, rate(g), stat_n(g));
    }

    /* --- tresor : depot des cartes --- */
    press(0,0,1,0); while (state != S_HOME) press(0,0,1,0);
    tapr(tile2(7)); CHECK(state == S_TRE);
    for (int g = 0; g < 7; g++) { int r0 = rate_dep(g); if (r0 >= SEUIL) tapr(tile2(g)); }
    idle(60); dump("tre_a");
    printf("tresor : %d etapes\n", P->steps);
    { Input in = {0}; in.held_l = in.held_r = 1; in.down_y = 1; for (int i = 0; i < 7; i++) { frame(in); idle(45); } }
    CHECK(P->steps == 7); idle(1); dump("tre_full");

    /* --- sauvegarde : relecture --- */
    int pts = GS(1)->points, steps = P->steps; char nm[12]; strcpy(nm, P->name);
    game_init(T, B, 9);
    CHECK(state == S_HOME); CHECK(GS(1)->points == pts); CHECK(P->steps == steps); CHECK(!strcmp(P->name, nm));
    printf("sauvegardes ecrites : %d\n", writes);
    /* effacement du joueur */
    press(0,0,0,1); CHECK(state == S_CONFIRM); idle(1); dump("confirm");
    tapr((Rect){134,60,112,70}); tapr((Rect){134,60,112,70}); CHECK(state == S_PROFILES && !SF.p[SF.cur].used);
    printf(fails ? "\n%d ECHEC(S)\n" : "\nTOUS LES TESTS PASSENT\n", fails);
    return fails != 0;
}
