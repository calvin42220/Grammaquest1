/* GrammaQuest Hub - version Nintendo DS
 * 7 jeux (cours, revision, entrainement, atelier, defi, blitz, bilan, trophees) + Carte au Tresor + profils joueurs */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "gamedata.h"

#define NQ 10
#define NG NGAMES            /* 7 jeux, le 8e ecran est la Carte au Tresor */
#define MAXCAT 10
#define NPROF 8
#define SEUIL 90             /* % de reussite global pour deposer une carte */
#define BLITZ_SEC 180
#define MAXERR 24

static px *top, *bot;
#define INK_ INK

/* ================= donnees de jeu supplementaires ================= */
static const int NSTAT[NG] = {8, 9, 1, 2, 2, 3, 9};   /* categories de statistiques par jeu */
static const int NPICK[NG] = {8, 9, 1, 3, 3, 4, 9};   /* choix possibles en entrainement */
static const char *const PICK_TF[3] = {"Le type", "La forme", "Les deux"};
static const char *const PICK_CO[3] = {"COD", "COI", "COD et COI"};
static const char *const PICK_CC[4] = {"CCL (lieu)", "CCT (temps)", "CCM (manière)", "Les trois"};
static const char *const STAT_TF[2] = {"Type", "Forme"};
static const char *const STAT_CO[2] = {"COD", "COI"};
static const char *const STAT_CC[3] = {"CCL", "CCT", "CCM"};
static const signed char AG[NG][3][3] = {   /* groupes d'ateliers (trophees "Sans faute") */
    {{0, 1, 2}, {4, 3, 5}, {7, 6, -1}},
    {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}},
    {{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}}, {{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}},
    {{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}}, {{-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}},
    {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}}};
static const int GR_MIN[6] = {0, 100, 250, 450, 700, 1000};
static const char *const GR_LAB[6] = {"Apprenti", "Explorateur", "Expert", "Maître", "Légende", "Champion Suprême"};
static const char *const HINT_KIND[5] = {
    "", "Pose la question « Qui est-ce qui + verbe ? » avant le verbe.",
    "Pose QUOI ? ou À QUI ? juste après le verbe, puis choisis le type.",
    "Regarde la ponctuation (. ? !) puis cherche « ne...pas », « jamais »...", ""};

/* ================= sauvegarde ================= */
typedef struct {
    uint16_t ok[MAXCAT], err[MAXCAT];
    uint8_t best[MAXCAT], stars[MAXCAT];
    uint16_t points, defiTotal, nearMiss;
    uint8_t bestStreak, speedBest, streakP8, noHintBest, perfectCount, blitzBestOk, blitzFlawless, palier, cours, nerr, lang;
    uint32_t troph;
    uint16_t errs[MAXERR];
} GSave;
typedef struct { char name[12]; uint8_t used, steps, dep[NG]; GSave g[NG]; } Profile;
typedef struct { uint8_t magic[4]; uint8_t cur, pad[3]; Profile p[NPROF]; uint32_t chk; } SaveFile;

static SaveFile SF;
static Profile *P;
static int sdok, dirtySave, qsaveN;

static GSave *GS(int g) { return &P->g[g]; }
static uint32_t sum_save(const SaveFile *s) {
    uint32_t c = 0;
    const uint8_t *p = (const uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s - 4; i++) c = c * 31 + p[i];
    return c;
}
static void save_now(void) {
    dirtySave = 0;
    SF.chk = sum_save(&SF);
    if (sdok) plat_save_write(&SF, sizeof SF);
}
static void load_save(void) {
    static SaveFile t;
    memset(&SF, 0, sizeof SF);
    memcpy(SF.magic, "GQ02", 4);
    if (sdok && plat_save_read(&t, sizeof t) && memcmp(t.magic, "GQ02", 4) == 0 && t.chk == sum_save(&t)) SF = t;
    SF.cur = (SF.cur < NPROF) ? SF.cur : 0;
    P = &SF.p[SF.cur];
}

/* ================= grades, stats, trophees ================= */
static int grade_of(int pts) { int g = 0; for (int i = 0; i < 6; i++) if (pts >= GR_MIN[i]) g = i; return g; }
static int stat_ok(int g) { int s = 0; for (int c = 0; c < NSTAT[g]; c++) s += GS(g)->ok[c]; return s; }
static int stat_n(int g) { int s = 0; for (int c = 0; c < NSTAT[g]; c++) s += GS(g)->ok[c] + GS(g)->err[c]; return s; }
static int rate(int g) { int n = stat_n(g); return n ? stat_ok(g) * 100 / n : 0; }
static int played(int g) { return stat_n(g) > 0; }
static void stat_add(int g, int c, int ok) {
    GSave *s = GS(g);
    if (c < 0 || c >= MAXCAT) return;
    if (ok) { if (s->ok[c] < 65000) s->ok[c]++; } else { if (s->err[c] < 65000) s->err[c]++; }
}
static int total_trophies(int g) { int n = 0; for (int i = 0; i < GAMES[g].ntroph; i++) n += (GS(g)->troph >> i) & 1; return n; }

static uint8_t popq[24];   /* file de fenetres : 0..31 = trophee, 100+n = nouveau grade */
static int popn;
static void pop_push(int v) { if (popn < 24) popq[popn++] = (uint8_t)v; }

static int tr_idx(int g, const char *id) {
    for (int i = 0; i < GAMES[g].ntroph; i++) if (!strcmp(GAMES[g].troph[i].id, id)) return i;
    return -1;
}
static int tr_has_id(int g, const char *id) { int i = tr_idx(g, id); return i >= 0 && ((GS(g)->troph >> i) & 1); }
static int all_cats(int g, int (*f)(int g, int c)) { for (int c = 0; c < NPICK[g]; c++) if (!f(g, c)) return 0; return 1; }
static int f_best10(int g, int c) { return GS(g)->best[c] >= 10; }
static int f_star3(int g, int c) { return GS(g)->stars[c] >= 3; }
static int any_star3(int g) { for (int c = 0; c < NPICK[g]; c++) if (GS(g)->stars[c] >= 3) return 1; return 0; }
static int group_star3(int g, int k) {
    int n = 0;
    for (int j = 0; j < 3; j++) { int c = AG[g][k][j]; if (c < 0) continue; n++; if (GS(g)->stars[c] < 3) return 0; }
    return n > 0;
}
static int has_pal(int g) { return g == 0 || g == 1 || g == 6; }
static int tr_cond(int g, const char *id) {
    GSave *s = GS(g);
    if (!strcmp(id, "atelier_parfait_1")) return group_star3(g, 0);
    if (!strcmp(id, "atelier_parfait_2")) return group_star3(g, 1);
    if (!strcmp(id, "atelier_parfait_3")) return group_star3(g, 2);
    if (!strcmp(id, "atelier_parfait")) return g == 3 ? (s->stars[0] >= 3 && s->stars[1] >= 3) : any_star3(g);
    if (!strcmp(id, "perfectionist")) return has_pal(g) ? all_cats(g, f_best10) : (s->perfectCount >= 3 && tr_has_id(g, "atelier_parfait"));
    if (!strcmp(id, "no_hint")) return (g == 0 || g == 1) ? any_star3(g) : s->noHintBest >= 5;
    if (!strcmp(id, "streak5")) return s->bestStreak >= 5;
    if (!strcmp(id, "streak10")) return s->bestStreak >= 10;
    if (!strcmp(id, "noctambule")) return s->bestStreak >= 20;
    if (!strcmp(id, "dans_la_vague")) return s->bestStreak >= 35;
    if (!strcmp(id, "score100")) return s->points >= 100;
    if (!strcmp(id, "score300")) return s->points >= 300;
    if (!strcmp(id, "champion")) return s->points >= 1000;
    if (!strcmp(id, "blitz_starter")) return s->blitzBestOk >= 5;
    if (!strcmp(id, "blitz10")) return s->blitzBestOk >= 10;
    if (!strcmp(id, "blitz20")) return s->blitzBestOk >= 20;
    if (!strcmp(id, "blitz_flawless")) return s->blitzFlawless >= 10;
    if (!strcmp(id, "invincible")) return s->points >= 2000;
    if (!strcmp(id, "type_master") || !strcmp(id, "cod_master") || !strcmp(id, "ccl_master")) return s->ok[0] >= (g == 5 ? 10 : 15);
    if (!strcmp(id, "form_master") || !strcmp(id, "coi_master") || !strcmp(id, "cct_master")) return s->ok[1] >= (g == 5 ? 10 : 15);
    if (!strcmp(id, "ccm_master")) return s->ok[2] >= 10;
    if (!strcmp(id, "sujet_15")) return s->ok[0] >= 20;
    if (!strcmp(id, "juste_a_temps")) return s->nearMiss >= 3;
    if (!strcmp(id, "platinum")) {
        for (int i = 0; i < GAMES[g].ntroph; i++) {
            const Troph *t = &GAMES[g].troph[i];
            if (t->flag == 0 && strcmp(t->id, "platinum") && !((s->troph >> i) & 1)) return 0;
        }
        return 1;
    }
    int plat = tr_has_id(g, "platinum"), inv = tr_has_id(g, "invincible");
    if (!strcmp(id, "speed_reflexe")) return plat && (has_pal(g) ? s->streakP8 >= 5 : s->speedBest >= 5);
    if (!strcmp(id, "speed_express")) return plat && (has_pal(g) ? s->streakP8 >= 10 : s->speedBest >= 10);
    if (!strcmp(id, "speed_tornade")) return plat && (has_pal(g) ? s->streakP8 >= 15 : s->speedBest >= 15);
    if (!strcmp(id, "pleine_puissance")) return plat && (has_pal(g) ? s->streakP8 >= 20 : s->blitzFlawless >= 25);
    if (!strcmp(id, "trident")) { if (!inv) return 0; for (int c = 0; c < NSTAT[g]; c++) if (s->ok[c] < 25) return 0; return 1; }
    if (!strcmp(id, "tridente")) { if (!inv) return 0; for (int c = 0; c < NSTAT[g]; c++) if (s->ok[c] < 75) return 0; return 1; }
    if (!strcmp(id, "triforce")) { if (!inv) return 0; for (int c = 0; c < NSTAT[g]; c++) if (s->ok[c] < 50) return 0; return 1; }
    if (!strcmp(id, "aimant")) return inv && s->blitzFlawless >= 30;
    if (!strcmp(id, "marathonien")) return inv && s->defiTotal >= 200;
    if (!strcmp(id, "legende_absolue")) {
        for (int i = 0; i < GAMES[g].ntroph; i++) if (GAMES[g].troph[i].flag != 3 && !((s->troph >> i) & 1)) return 0;
        return 1;
    }
    return 0;
}
static void tr_check(int g) {
    GSave *s = GS(g);
    for (int pass = 0; pass < 4; pass++) {
        int ch = 0;
        for (int i = 0; i < GAMES[g].ntroph; i++) {
            if ((s->troph >> i) & 1) continue;
            if (tr_cond(g, GAMES[g].troph[i].id)) { s->troph |= 1u << i; pop_push(i); ch = 1; }
        }
        if (!ch) break;
    }
}
static void add_points(int g, int pts) {
    GSave *s = GS(g);
    int before = grade_of(s->points);
    int v = s->points + pts; if (v > 65000) v = 65000;
    s->points = (uint16_t)v;
    int after = grade_of(s->points);
    if (after > before) pop_push(100 + after);
}

/* ================= etat du jeu ================= */
typedef struct { int x, y, w, h; } Rect;
static int inside(Rect r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }
enum { S_PROFILES, S_KEYB, S_HOME, S_GMENU, S_PICK, S_PALIER, S_PLAY, S_RESULT, S_END, S_TRE, S_CONFIRM, S_COURS, S_BILAN, S_TROPH, S_POPUP };
enum { M_TRAIN, M_ATELIER, M_REVIEW, M_DEFI, M_BLITZ };
enum { PA_NEXT, PA_END, PA_BACK };
static int rate_dep(int g) { return stat_n(g) >= 20 ? rate(g) : 0; }   /* taux utilisable pour deposer une carte (20 reponses minimum) */

static int state, gi, cat, mode, pickMode, focus, qi, nq, score, dirtyT, dirtyB, lastCorrect, stage;
static int ansT, ansF, ansG, hintOn, timeUp, confirmStage, confirmKind, anim, animStripe;
static int quiz[NQ], quizcat[NQ], quizent[NQ], res[NQ];
static int curId, curCat, curEnt;
static int ses_ok, ses_err, streak, speedCur, noHintCur, sesPts, qcount, lastPts;
static int tleft, tmax, bzleft;
static int popAfter, coursPage, trophSel, newSlot, tick;
static int ntok, pillBig, kbCaps;
static char toks[MAXTOK][40];
static int tagv[MAXTOK], sel[MAXTOK];
static Rect wbox[MAXTOK];
static const Q *cq;
static char namebuf[12];
static int recent[6];

#define dirty_all() (dirtyT = dirtyB = 1)
static const GameData *G(void) { return &GAMES[gi]; }
static int kind(void) { return G()->kind; }
static int ncatsG(void) { return G()->ncat; }

static void err_push(int id, int c) {
    GSave *s = GS(gi);
    int e = id | (c << 10);
    for (int i = 0; i < s->nerr; i++) if (s->errs[i] == e) return;
    if (s->nerr >= MAXERR) { memmove(s->errs, s->errs + 1, (MAXERR - 1) * sizeof(uint16_t)); s->nerr = MAXERR - 1; }
    s->errs[s->nerr++] = (uint16_t)e;
}
static void err_remove(int e) {
    GSave *s = GS(gi);
    for (int i = 0; i < s->nerr; i++) if (s->errs[i] == e) {
        memmove(s->errs + i, s->errs + i + 1, (s->nerr - i - 1) * sizeof(uint16_t)); s->nerr--; return;
    }
}

/* ---------- mise en page des mots ---------- */
static int is_target_tok(int i) { return kind() == K_TAG ? tagv[i] == cat + 1 : tagv[i] == 1; }
static int try_layout(int big) {
    int k = kind();
    const Font *f = big ? &font_big : &font_small;
    int h = big ? 26 : 20, pitch = big ? 34 : 25, pad = big ? 14 : 12;
    int x = 6, y = 8, lines = 1;
    for (int i = 0; i < ntok; i++) {
        int cand = (k == K_TAG) ? tagv[i] != 0 : 1;
        int w = text_w(f, toks[i]) + (cand ? pad : 4);
        if (x + w > SW - 4 && x > 6) { x = 6; y += pitch; lines++; }
        wbox[i].x = x; wbox[i].y = y; wbox[i].w = w; wbox[i].h = h;
        x += w + (big ? 6 : 5);
    }
    return lines;
}
static void layout_words(void) {
    pillBig = 1;
    if (kind() == K_TAG) { try_layout(1); return; }
    if (try_layout(1) > 3) { pillBig = 0; try_layout(0); }
}
static void load_q(void) {
    curId = quiz[qi]; curCat = quizcat[qi]; curEnt = quizent[qi];
    cq = &G()->qs[curId];
    cat = curCat;
    ntok = 0; stage = 0; ansT = ansF = ansG = -1; hintOn = 0; timeUp = 0;
    const char *p = cq->text;
    while (*p && ntok < MAXTOK) {
        int n = 0;
        while (*p && *p != ' ' && n < 39) toks[ntok][n++] = *p++;
        toks[ntok][n] = 0;
        while (*p == ' ') p++;
        tagv[ntok] = cq->tags[ntok]; sel[ntok] = 0;
        ntok++;
    }
    if (kind() == K_TAG || kind() == K_SEL || kind() == K_SELTYPE) layout_words();
    if (kind() == K_TF && focus == 1) { ansT = cq->a; stage = 1; }
    if (mode == M_DEFI) { tleft = tmax; }
}
static int nTargets(void) { int c = 0; for (int i = 0; i < ntok; i++) c += is_target_tok(i); return c; }
static int nsel(void) { int c = 0; for (int i = 0; i < ntok; i++) c += sel[i]; return c; }

/* ---------- choix des questions ---------- */
static int active_cats(int g) {
    int n = GAMES[g].ncat;
    if (!has_pal(g)) return n;
    int p = GS(g)->palier; if (p < 1) p = 1;
    return p + 1 < n ? p + 1 : n;
}
static int lang_ok(const Q *q) {   /* Le Sujet : 0 francais, 1 anglais, 2 mixte */
    if (G()->kind != K_SEL) return 1;
    int l = GS(gi)->lang; if (l > 2) l = 0;
    return l == 2 || q->cat == l;
}
static int in_recent(int id) { for (int i = 0; i < 6; i++) if (recent[i] == id) return 1; return 0; }
static void pick_endless(void) {
    const GameData *D = G();
    int na = active_cats(gi), id = 0, c = 0;
    for (int tries = 0; tries < 600; tries++) {
        id = rand() % D->nq;
        const Q *q = &D->qs[id];
        if (tries < 300 && in_recent(id)) continue;
        if (D->kind == K_TAG) {
            int pres[MAXCAT + 1], np = 0;
            for (int k = 0; k < MAXTOK; k++) {
                int t = q->tags[k];
                if (t >= 1 && t <= na) { int d = 0; for (int j = 0; j < np; j++) if (pres[j] == t) d = 1; if (!d) pres[np++] = t; }
            }
            if (!np) continue;
            c = pres[rand() % np] - 1; break;
        } else if (D->kind == K_GAP) {
            c = rand() % na;
            if (q->cat != c) continue;
            break;
        } else if (D->kind == K_SELTYPE) {
            if (focus < D->ntypes && !(q->cat == 0 && q->a == focus)) continue;
            c = 0; break;
        } else { if (!lang_ok(q)) continue; c = 0; break; }
    }
    memmove(recent, recent + 1, 5 * sizeof(int)); recent[5] = id;
    quiz[0] = id; quizcat[0] = c; quizent[0] = id | (c << 10);
    qi = 0;
}
static int pickIdx(void) { return ncatsG() ? cat : focus; }

static void begin_play(void) { state = S_PLAY; load_q(); dirty_all(); }

static void start_series(int g, int pick, int md) {
    static int pool[760];
    gi = g; mode = md; focus = pick; cat = pick;
    const GameData *D = G();
    int np = 0;
    for (int pass = 0; pass < 2 && np < NQ; pass++) {
        np = 0;
        for (int i = 0; i < D->nq; i++) {
            const Q *q = &D->qs[i];
            int ok = 1;
            if (D->kind == K_TAG) {
                int has = 0, other = 0;
                for (int k = 0; k < MAXTOK; k++) { if (q->tags[k] == pick + 1) has = 1; else if (q->tags[k]) other = 1; }
                ok = has && (other || pass == 1);
            } else if (D->kind == K_GAP) ok = q->cat == pick;
            else if (D->kind == K_SELTYPE && pick < D->ntypes) ok = (q->cat == 0 && q->a == pick);
            else if (D->kind == K_SEL) ok = lang_ok(q);
            if (ok) pool[np++] = i;
        }
    }
    if (np == 0) { pool[0] = 0; np = 1; }
    for (int i = np - 1; i > 0; i--) { int j = rand() % (i + 1); int t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
    nq = NQ;
    for (int i = 0; i < NQ; i++) { quiz[i] = pool[i % np]; quizcat[i] = (D->kind == K_TAG || D->kind == K_GAP) ? pick : 0; quizent[i] = quiz[i] | (quizcat[i] << 10); res[i] = 0; }
    qi = 0; score = 0; ses_ok = ses_err = 0;
    begin_play();
}
static void start_review(int g) {
    gi = g; mode = M_REVIEW; focus = 2;
    GSave *s = GS(g);
    nq = s->nerr < NQ ? s->nerr : NQ;
    for (int i = 0; i < nq; i++) { int e = s->errs[i]; quiz[i] = e & 1023; quizcat[i] = e >> 10; quizent[i] = e; res[i] = 0; }
    for (int i = nq; i < NQ; i++) res[i] = 0;
    qi = 0; score = 0; ses_ok = ses_err = 0;
    begin_play();
}
static void start_endless(int g, int md) {
    gi = g; mode = md;
    focus = ncatsG() ? 0 : NPICK[g] - 1;
    if (GS(g)->palier < 1 || GS(g)->palier > 8) GS(g)->palier = 1;
    for (int i = 0; i < 6; i++) recent[i] = -1;
    ses_ok = ses_err = streak = speedCur = noHintCur = sesPts = qcount = 0;
    tmax = (has_pal(g) ? (15 + (GS(g)->palier - 1) * 5) : 30) * 60;
    bzleft = BLITZ_SEC * 60;
    memset(res, 0, sizeof res);
    pick_endless();
    begin_play();
}

/* ---------- reponses ---------- */
static void finish_q(int ok) {
    GSave *s = GS(gi);
    int k = kind();
    lastCorrect = ok;
    if (mode <= M_REVIEW) res[qi] = ok ? 1 : 2;
    if (ok) { score++; ses_ok++; } else ses_err++;
    qcount++;
    /* statistiques par categorie */
    if (k == K_TAG || k == K_GAP) stat_add(gi, cat, ok);
    else if (k == K_SEL) stat_add(gi, 0, ok);
    else if (k == K_SELTYPE) stat_add(gi, cq->a, ok);
    else {
        if (focus != 1) stat_add(gi, 0, ansT == cq->a);
        if (focus != 0 && ansF >= 0) stat_add(gi, 1, ansF == cq->b);
    }
    /* liste d'erreurs (revision) */
    if (mode == M_REVIEW) { if (ok) err_remove(curEnt); }
    else if (!ok) err_push(curId, curCat);
    /* defi / blitz */
    lastPts = 0;
    if (mode == M_DEFI || mode == M_BLITZ) {
        if (ok) {
            streak++; noHintCur++;
            if (noHintCur > s->noHintBest) s->noHintBest = (uint8_t)(noHintCur > 255 ? 255 : noHintCur);
        } else { streak = 0; speedCur = 0; noHintCur = 0; }
    }
    if (mode == M_DEFI) {
        if (ok) {
            lastPts = (tleft * 45 + tmax / 2) / tmax;
            sesPts += lastPts; add_points(gi, lastPts);
            if (s->defiTotal < 65000) s->defiTotal++;
            if (streak > s->bestStreak) s->bestStreak = (uint8_t)(streak > 255 ? 255 : streak);
            if (has_pal(gi) && s->palier == 8 && streak > s->streakP8) s->streakP8 = (uint8_t)(streak > 255 ? 255 : streak);
            if (tmax - tleft < 5 * 60) { speedCur++; if (speedCur > s->speedBest) s->speedBest = (uint8_t)(speedCur > 255 ? 255 : speedCur); } else speedCur = 0;
            if (tleft < 3 * 60 && s->nearMiss < 65000) s->nearMiss++;
        }
    }
    tr_check(gi);
    dirtySave = 1;
    if (++qsaveN % 3 == 0) save_now();
    state = S_RESULT; dirty_all();
}
static void validate_sel(int typeChosen) {
    int k = kind(), ok = 1;
    if (k == K_TAG) {
        for (int i = 0; i < ntok; i++) if (tagv[i] && ((tagv[i] == cat + 1) != (sel[i] != 0))) ok = 0;
    } else if (k == K_SEL) {
        for (int i = 0; i < ntok; i++) if ((tagv[i] == 1) != (sel[i] != 0)) ok = 0;
    } else {
        int okA = typeChosen == cq->a, okB = typeChosen == cq->b, hasB = 0;
        for (int i = 0; i < ntok; i++) if (tagv[i] == 2) hasB = 1;
        for (int i = 0; i < ntok; i++) {
            if ((tagv[i] == 1) != (sel[i] != 0)) okA = 0;
            if ((tagv[i] == 2) != (sel[i] != 0)) okB = 0;
        }
        ok = okA || (hasB && okB);
        ansT = typeChosen;
    }
    finish_q(ok);
}
static void show_popups_then(int action) {
    if (popn > 0) { popAfter = action; state = S_POPUP; dirty_all(); }
    else if (action == PA_NEXT) { /* rien : l'appelant continue */ }
}
static void enter_end(void) {
    GSave *s = GS(gi);
    int idx = pickIdx();
    if (mode == M_TRAIN) {
        if (score > s->best[idx]) s->best[idx] = (uint8_t)score;
        if (score >= NQ && s->perfectCount < 250) s->perfectCount++;
    } else if (mode == M_ATELIER) {
        int st = ses_err == 0 ? 3 : ses_err <= 2 ? 2 : 1;
        if (st > s->stars[idx]) s->stars[idx] = (uint8_t)st;
    } else if (mode == M_BLITZ) {
        if (ses_ok > s->blitzBestOk) s->blitzBestOk = (uint8_t)(ses_ok > 255 ? 255 : ses_ok);
        if (ses_err == 0 && ses_ok > s->blitzFlawless) s->blitzFlawless = (uint8_t)(ses_ok > 255 ? 255 : ses_ok);
    }
    tr_check(gi);
    save_now();
    state = S_END; dirty_all();
    if (popn) { popAfter = PA_END; state = S_POPUP; }
}
static void next_q(void) {
    if (mode == M_DEFI || mode == M_BLITZ) {
        if (mode == M_BLITZ && bzleft <= 0) { enter_end(); return; }
        pick_endless(); state = S_PLAY; load_q(); dirty_all();
        return;
    }
    qi++;
    if (qi >= nq) { enter_end(); return; }
    state = S_PLAY; load_q(); dirty_all();
}
static void go_after_result(void) {
    if (popn > 0) { popAfter = PA_NEXT; state = S_POPUP; dirty_all(); return; }
    next_q();
}

/* ================= geometrie des ecrans ================= */
static Rect tile2(int i) { Rect r = {4 + (i % 2) * 126, 20 + (i / 2) * 38, 122, 36}; return r; }
static Rect pick_btn(int i) { Rect r = {2 + (i % 3) * 85, 24 + (i / 3) * 54, 82, 50}; return r; }
static Rect list_btn(int i) { Rect r = {8, 30 + i * 38, 240, 34}; return r; }
static const Rect BTN_BACK = {2, 1, 62, 20};
static const Rect BTN_LANG = {132, 0, 122, 19};
static const char *const LANGN[3] = {"Langue : Français", "Language : English", "Langue : FR + EN"};
static const Rect BTN_SMALLMENU = {4, 1, 50, 18};
static const Rect BTN_HINT_TOP = {198, 1, 56, 18};
static Rect btn_menu(void) { Rect r = {4, 154, mode == M_ATELIER ? 60 : 72, 34}; return r; }
static Rect btn_hint(void) { Rect r = {68, 154, 60, 34}; return r; }
static Rect btn_main(void) {
    if (mode == M_ATELIER && state == S_PLAY) { Rect r = {132, 154, 120, 34}; return r; }
    Rect r = {86, 154, 166, 34}; return r;
}
static Rect tfbtn(int i) { Rect r = {4 + (i % 2) * 126, 112 + (i / 2) * 40, 122, 36}; return r; }
static Rect tfform(int i) { Rect r = {4 + i * 126, 124, 122, 48}; return r; }
static Rect typebtn(int i, int n) { int w = n == 2 ? 122 : 80; Rect r = {4 + i * (w + 4), 114, w, 34}; return r; }
static Rect kbkey(int row, int col, int *wout) {
    int w = 25, x = 3 + col * 25;
    if (row == 4) { w = 84; x = 3 + col * 84; }
    if (wout) *wout = w;
    Rect r = {x, 6 + row * 36, w - 2, 32}; return r;
}
static const char *const KB[4][10] = {
    {"A", "Z", "E", "R", "T", "Y", "U", "I", "O", "P"}, {"Q", "S", "D", "F", "G", "H", "J", "K", "L", "M"},
    {"W", "X", "C", "V", "B", "N", "É", "È", "Ê", "Ç"}, {"Ï", "Ô", "Û", "À", "'", "-", 0, 0, 0, 0}};

/* ================= composants ================= */
static void draw_tile(px *buf, Rect r, const char *l1, const char *l2, uint32_t col, int mode_) {
    /* 0 normal, 1 grise, 2 a deposer (vert), 3 deposee, 4 mise en avant */
    px fill = hex(lighten(col, 82)), border = hex(col), t1 = INK, t2 = GREY;
    if (mode_ == 1) { fill = hex(0xF1F5F9); border = hex(0xCBD5E1); t1 = hex(0x94A3B8); t2 = hex(0x94A3B8); }
    if (mode_ == 2) { fill = hex(0xDCFCE7); border = hex(0x16A34A); t2 = hex(0x15803D); }
    if (mode_ == 3) { fill = hex(lighten(col, 65)); t2 = hex(0x15803D); }
    if (mode_ == 4) { fill = hex(lighten(col, 65)); }
    box(buf, r.x, r.y, r.w, r.h, 8, fill, border);
    draw_center(buf, &font_small, r.x + r.w / 2, r.y + 3, l1, t1, 1);
    draw_center(buf, &font_small, r.x + r.w / 2, r.y + 19, l2, t2, 1);
}
static void draw_button(px *buf, Rect r, const char *label, px fill, px txt) {
    box(buf, r.x, r.y, r.w, r.h, 8, fill, fill);
    draw_center(buf, &font_big, r.x + r.w / 2, r.y + (r.h - font_big.h) / 2, label, txt, 1);
}
static void draw_small_button(px *buf, Rect r, const char *label, px fill, px txt) {
    box(buf, r.x, r.y, r.w, r.h, 6, fill, fill);
    draw_center(buf, &font_small, r.x + r.w / 2, r.y + (r.h - font_small.h) / 2, label, txt, 1);
}
static void draw_bar(px *buf, int x, int y, int w, int h, int pct, px col) {
    fill_rect(buf, x, y, w, h, hex(0xE2E8F0));
    if (pct > 100) pct = 100;
    if (pct > 0) fill_rect(buf, x, y, w * pct / 100, h, col);
}
static void star(px *buf, int cx, int cy, int R, px col) {
    static const int bx[10] = {0, 22, 95, 36, 59, 0, -59, -36, -95, -22};
    static const int by[10] = {-100, -31, -31, 12, 81, 38, 81, 12, -31, -31};
    int vx[10], vy[10];
    for (int i = 0; i < 10; i++) { vx[i] = cx + bx[i] * R / 100; vy[i] = cy + by[i] * R / 100; }
    for (int y = cy - R; y <= cy + R; y++) {
        for (int x = cx - R; x <= cx + R; x++) {
            int in = 0;
            for (int i = 0, j = 9; i < 10; j = i++) {
                int dy = vy[j] - vy[i];
                if (((vy[i] > y) != (vy[j] > y)) && (x < (vx[j] - vx[i]) * (y - vy[i]) / (dy ? dy : 1) + vx[i])) in = !in;
            }
            if (in && x >= 0 && x < SW && y >= 0 && y < SH) buf[y * SW + x] = col;
        }
    }
}
static void draw_stars3(px *buf, int cx, int y, int n, int R) {
    for (int i = 0; i < 3; i++) star(buf, cx + (i - 1) * (R * 2 + 4), y, R, i < n ? hex(0xEAB308) : hex(0xCBD5E1));
}
static const char *stat_label(int g, int c) {
    if (g == 0 || g == 1 || g == 6) return GAMES[g].cats[c].label;
    if (g == 2) return "Le sujet";
    if (g == 3) return STAT_TF[c];
    if (g == 4) return STAT_CO[c];
    return STAT_CC[c];
}
static const char *pick_label(int g, int c) {
    if (GAMES[g].ncat) return GAMES[g].cats[c].label;
    if (g == 3) return PICK_TF[c];
    if (g == 4) return PICK_CO[c];
    if (g == 5) return PICK_CC[c];
    return "Le sujet";
}
static uint32_t cur_col(void) { return G()->ncat ? G()->cats[cat].col : G()->col; }
static uint32_t tflag_col(int flag) { return flag == 3 ? 0xEF4444 : flag == 2 ? 0x9333EA : flag == 1 ? 0x0EA5E9 : 0xEAB308; }
static const char *mode_name(int m) {
    return m == M_TRAIN ? "Entraînement" : m == M_ATELIER ? "Atelier guidé" : m == M_REVIEW ? "Révision" : m == M_DEFI ? "Défi" : "Blitz 3 min";
}

/* ================= cours ================= */
static const char *coursStart[48];
static int coursN;
static const char *cours_layout(const char *p, int draw, int *nl) {
    char line[240], word[64], trial[320];
    int lines = 0;
    while (*p == '\n' || *p == ' ') p++;
    line[0] = 0;
    for (;;) {
        if (*p == 0 || *p == '\f') {
            if (line[0]) { if (draw) draw_text(top, &font_small, 8, 22 + lines * 14, line, INK, 1); lines++; }
            if (*p == '\f') p++;
            break;
        }
        if (*p == '\n') {
            if (line[0]) { if (draw) draw_text(top, &font_small, 8, 22 + lines * 14, line, INK, 1); lines++; line[0] = 0; }
            while (*p == '\n' || *p == ' ') p++;
            if (lines >= 11) break;
            continue;
        }
        if (*p == ' ') { p++; continue; }
        const char *w = p; int n = 0;
        while (*p && *p != ' ' && *p != '\n' && *p != '\f' && n < 63) word[n++] = *p++;
        word[n] = 0;
        if (line[0]) snprintf(trial, sizeof trial, "%s %s", line, word); else snprintf(trial, sizeof trial, "%s", word);
        if (line[0] && text_w(&font_small, trial) > 240) {
            if (draw) draw_text(top, &font_small, 8, 22 + lines * 14, line, INK, 1);
            lines++;
            if (lines >= 11) { p = w; break; }
            snprintf(line, sizeof line, "%s", word);
        } else snprintf(line, sizeof line, "%s", trial);
    }
    if (nl) *nl = lines;
    return p;
}
static void cours_open(void) {
    const char *p = (G()->kind == K_SEL && GS(gi)->lang == 1 && G()->cours_en[0]) ? G()->cours_en : G()->cours;
    coursN = 0;
    while (*p && coursN < 47) { coursStart[coursN++] = p; p = cours_layout(p, 0, 0); while (*p == '\n' || *p == ' ') p++; }
    if (coursN == 0) { coursStart[0] = ""; coursN = 1; }
    coursPage = 0;
    if (!GS(gi)->cours) { GS(gi)->cours = 1; save_now(); }
}

/* ================= fenetre "image Bravo" du tresor ================= */
static void draw_bravo(px *buf) {
    for (int y = 0; y < SH; y++) {
        int r = 255, g = 244 - y * 40 / SH, b = 205 - y * 70 / SH;
        px c = (px)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000);
        for (int x = 0; x < SW; x++) buf[y * SW + x] = c;
    }
    static const uint32_t cols[6] = {0xEF4444, 0x22A06B, 0x0EA5E9, 0xEAB308, 0xEC4899, 0x8B5CF6};
    unsigned s = 12345;
    for (int i = 0; i < 46; i++) {
        s = s * 1103515245u + 12345u; int x = (s >> 8) % SW;
        s = s * 1103515245u + 12345u; int y = (s >> 8) % SH;
        disc(buf, x, y, 2 + (i % 3), hex(cols[i % 6]));
    }
    box(buf, 8, 30, 240, 136, 14, WHITE_, hex(0xEAB308));
    star(buf, 24, 14, 11, hex(0xEAB308)); star(buf, 232, 14, 11, hex(0xEAB308));
    star(buf, 128, 18, 9, hex(0xEC4899));
    draw_center(buf, &font_big, 128, 40, "Bravo !", hex(0xE81A1A), 3);
    const char *a = "Bon travail", *b = "de grammaire !";
    int sa = text_w(&font_big, a) * 2 <= 224 ? 2 : 1, sb = text_w(&font_big, b) * 2 <= 224 ? 2 : 1;
    draw_center(buf, &font_big, 128, 102, a, hex(0x1D5FD4), sa);
    draw_center(buf, &font_big, 128, 132, b, hex(0x22A06B), sb);
}
static void draw_top_tre(void) {
    draw_bravo(top);
    for (int i = 0; i < 7; i++) {
        int x0 = i * SW / 7, x1 = (i + 1) * SW / 7, w = x1 - x0;
        int cover = 0;
        if (i >= P->steps) cover = w;
        else if (anim > 0 && i == animStripe) cover = w * anim / 40;
        if (cover > 0) {
            fill_rect(top, x0, 0, cover, SH, hex(0xCBD5E1));
            fill_rect(top, x0, 0, 1, SH, hex(0x94A3B8));
            if (cover > 24) { char b[4]; snprintf(b, sizeof b, "%d", i + 1); draw_center(top, &font_big, x0 + cover / 2, 84, b, hex(0x64748B), 2); }
        }
    }
}

/* ================= ecran du haut ================= */
static void mission_text(char *buf, int n) {
    const GameData *D = G();
    int k = D->kind;
    if (k == K_TAG) {
        const Cat *C = &D->cats[cat];
        int nt = nTargets();
        if (nt > 1) snprintf(buf, n, "Touche %s de la phrase (%d).", C->plural, nt);
        else snprintf(buf, n, "Touche %s de la phrase.", C->find);
    } else if (k == K_SEL) snprintf(buf, n, "Touche le sujet du verbe «%s».", cq->extra);
    else if (k == K_SELTYPE) snprintf(buf, n, "Touche les mots d'un complément, puis choisis son type.");
    else if (k == K_TF) snprintf(buf, n, stage == 0 ? "Quel est le type de la phrase ?" : "Quelle est sa forme ?");
    else snprintf(buf, n, "Quel mot complète la phrase ?");
}
static void draw_progress(px *buf, int y, uint32_t col) {
    for (int i = 0; i < NQ; i++) {
        int x = 8 + i * 24;
        px c = hex(0xCBD5E1);
        if (i >= nq) c = hex(0xF1F5F9);
        else if (res[i] == 1) c = hex(0x22A06B); else if (res[i] == 2) c = hex(0xEF4444);
        if (state != S_END && i == qi && res[i] == 0 && i < nq) { rrect(buf, x, y, 20, 14, 5, hex(col)); rrect(buf, x + 2, y + 2, 16, 10, 4, WHITE_); }
        else rrect(buf, x, y, 20, 14, 5, c);
    }
}
static void grade_line(char *buf, int n, int g) {
    int pts = GS(g)->points, gr = grade_of(pts);
    if (gr < 5) snprintf(buf, n, "%s - %d pts (prochain : %d)", GR_LAB[gr], pts, GR_MIN[gr + 1]);
    else snprintf(buf, n, "%s - %d pts", GR_LAB[gr], pts);
}
static int flashT; static const char *flashMsg = "";

static void draw_top(void) {
    fill_rect(top, 0, 0, SW, SH, hex(0xFFFBEB));
    char buf[320];
    const GameData *D = G();
    switch (state) {
    case S_PROFILES:
        fill_rect(top, 0, 0, SW, 70, hex(0xEAB308));
        draw_center(top, &font_big, 128, 8, "GrammaQuest", hex(0x422006), 2);
        draw_center(top, &font_big, 128, 46, "CM1 - Grammaire", hex(0x422006), 1);
        draw_center(top, &font_big, 128, 92, "Qui joue ?", INK, 2);
        wrap(top, &font_small, 14, 134, 228, "Touche ton prénom, ou crée un nouveau joueur.", GREY, 15, 1);
        return;
    case S_KEYB: {
        fill_rect(top, 0, 0, SW, 70, hex(0xEAB308));
        draw_center(top, &font_big, 128, 8, "GrammaQuest", hex(0x422006), 2);
        draw_center(top, &font_big, 128, 46, "Nouveau joueur", hex(0x422006), 1);
        draw_center(top, &font_big, 128, 84, "Écris ton prénom", INK, 1);
        box(top, 28, 110, 200, 40, 10, WHITE_, hex(0xEAB308));
        snprintf(buf, sizeof buf, "%s%s", namebuf, (tick / 20) % 2 ? "" : "_");
        draw_center(top, &font_big, 128, 120, buf, INK, 1);
        draw_center(top, &font_small, 128, 164, "10 lettres au maximum", GREY, 1);
        return;
    }
    case S_HOME:
    case S_CONFIRM:
        fill_rect(top, 0, 0, SW, 70, hex(0xEAB308));
        draw_center(top, &font_big, 128, 8, "GrammaQuest", hex(0x422006), 2);
        draw_center(top, &font_big, 128, 46, "CM1 - Grammaire", hex(0x422006), 1);
        if (state == S_HOME) {
            snprintf(buf, sizeof buf, "Bonjour %s !", P->name);
            draw_text(top, &font_big, 12, 82, buf, INK, 1);
            draw_text(top, &font_small, 12, 106, "Touche une application sur l'écran du bas.", GREY, 1);
            snprintf(buf, sizeof buf, "Étapes du trésor : %d / 7", P->steps);
            draw_text(top, &font_small, 12, 140, buf, GREY, 1);
            draw_text(top, &font_small, 12, 156, sdok ? "Sauvegarde : carte SD OK" : "Sans sauvegarde (carte non reconnue)", sdok ? GREY : hex(0xEF4444), 1);
            draw_text(top, &font_small, 12, 174, "SELECT : effacer ce joueur", hex(0x94A3B8), 1);
        } else {
            snprintf(buf, sizeof buf, confirmStage == 0 ? "Effacer le joueur %s et toute sa progression ?" : "Vraiment tout effacer pour %s ? C'est définitif.", P->name);
            wrap(top, &font_big, 12, 84, 232, buf, hex(0xB91C1C), 22, 1);
        }
        return;
    case S_TRE: draw_top_tre(); return;
    case S_GMENU: {
        fill_rect(top, 0, 0, SW, 70, hex(D->col));
        draw_center(top, &font_big, 128, 8, D->label, WHITE_, 2);
        draw_center(top, &font_big, 128, 46, D->sub, WHITE_, 1);
        grade_line(buf, sizeof buf, gi);
        draw_center(top, &font_small, 128, 82, buf, INK, 1);
        int gr = grade_of(GS(gi)->points), pct = 100;
        if (gr < 5) pct = (GS(gi)->points - GR_MIN[gr]) * 100 / (GR_MIN[gr + 1] - GR_MIN[gr]);
        draw_bar(top, 28, 102, 200, 10, pct, hex(D->col));
        snprintf(buf, sizeof buf, "Trophées : %d / %d", total_trophies(gi), D->ntroph);
        draw_center(top, &font_small, 128, 122, buf, GREY, 1);
        snprintf(buf, sizeof buf, "Taux de réussite : %d %% (%d réponses)", rate(gi), stat_n(gi));
        draw_center(top, &font_small, 128, 140, buf, GREY, 1);
        if (flashT > 0) draw_center(top, &font_big, 128, 164, flashMsg, hex(0xDC2626), 1);
        else if (!GS(gi)->cours) draw_center(top, &font_small, 128, 166, "Commence par lire le cours !", hex(0xB45309), 1);
        return;
    }
    case S_PICK:
    case S_PALIER:
        fill_rect(top, 0, 0, SW, 70, hex(D->col));
        draw_center(top, &font_big, 128, 8, D->label, WHITE_, 2);
        draw_center(top, &font_big, 128, 46, state == S_PALIER ? mode_name(pickMode) : mode_name(pickMode), WHITE_, 1);
        if (state == S_PALIER) wrap(top, &font_big, 12, 88, 232, "Choisis ton palier : plus il est haut, plus il y a de catégories et de temps.", INK, 22, 1);
        else wrap(top, &font_big, 12, 90, 232, D->kind == K_GAP ? "Choisis les homophones à travailler." : "Choisis ce que tu veux travailler.", INK, 22, 1);
        if (pickMode == M_ATELIER) draw_text(top, &font_small, 12, 160, "3 étoiles = sans erreur. Un indice est disponible.", GREY, 1);
        return;
    case S_COURS: {
        fill_rect(top, 0, 0, SW, 18, hex(D->col));
        snprintf(buf, sizeof buf, "Cours : %s  (%d / %d)", D->label, coursPage + 1, coursN);
        draw_text(top, &font_small, 6, 2, buf, WHITE_, 1);
        cours_layout(coursStart[coursPage], 1, 0);
        return;
    }
    case S_BILAN: {
        fill_rect(top, 0, 0, SW, 28, hex(D->col));
        snprintf(buf, sizeof buf, "Mon bilan : %s", D->label);
        draw_text(top, &font_big, 8, 5, buf, WHITE_, 1);
        snprintf(buf, sizeof buf, "%d %%", rate(gi));
        draw_center(top, &font_big, 128, 38, buf, hex(D->col), 3);
        snprintf(buf, sizeof buf, "taux global sur %d réponses", stat_n(gi));
        draw_center(top, &font_small, 128, 96, buf, GREY, 1);
        grade_line(buf, sizeof buf, gi);
        draw_center(top, &font_small, 128, 120, buf, INK, 1);
        int gr = grade_of(GS(gi)->points), pct = 100;
        if (gr < 5) pct = (GS(gi)->points - GR_MIN[gr]) * 100 / (GR_MIN[gr + 1] - GR_MIN[gr]);
        draw_bar(top, 28, 140, 200, 10, pct, hex(D->col));
        snprintf(buf, sizeof buf, "Trophées : %d / %d", total_trophies(gi), D->ntroph);
        draw_center(top, &font_small, 128, 158, buf, GREY, 1);
        if (rate(gi) >= SEUIL) draw_center(top, &font_small, 128, 174, "Plus de 90 % : carte à déposer au trésor !", hex(0x15803D), 1);
        return;
    }
    case S_TROPH: {
        fill_rect(top, 0, 0, SW, 28, hex(D->col));
        snprintf(buf, sizeof buf, "Mes trophées : %d / %d", total_trophies(gi), D->ntroph);
        draw_text(top, &font_big, 8, 5, buf, WHITE_, 1);
        if (trophSel >= 0 && trophSel < D->ntroph) {
            const Troph *t = &D->troph[trophSel];
            int has = (GS(gi)->troph >> trophSel) & 1;
            box(top, 6, 36, 244, 140, 10, hex(lighten(tflag_col(t->flag), 85)), hex(tflag_col(t->flag)));
            draw_text(top, &font_small, 14, 42, t->flag == 3 ? "ULTIME" : t->flag == 2 ? "LÉGENDAIRE" : t->flag == 1 ? "VITESSE" : "TROPHÉE", hex(tflag_col(t->flag)), 1);
            int n = wrap(top, &font_big, 14, 60, 228, t->name, INK, 20, 1);
            wrap(top, &font_small, 14, 62 + n * 20, 228, t->desc, hex(0x334155), 14, 1);
            draw_text(top, &font_small, 14, 156, has ? "Débloqué !" : "À débloquer", has ? hex(0x15803D) : hex(0x94A3B8), 1);
        } else draw_center(top, &font_small, 128, 90, "Touche un trophée pour le lire.", GREY, 1);
        return;
    }
    case S_POPUP: {
        int v = popq[0];
        if (v >= 100) {
            box(top, 10, 20, 236, 150, 14, WHITE_, hex(0xEAB308));
            draw_center(top, &font_big, 128, 34, "Nouveau grade !", hex(0xB45309), 2);
            draw_center(top, &font_big, 128, 84, GR_LAB[v - 100], INK, 2);
            snprintf(buf, sizeof buf, "%d points", GS(gi)->points);
            draw_center(top, &font_small, 128, 130, buf, GREY, 1);
            star(top, 36, 50, 12, hex(0xEAB308)); star(top, 220, 50, 12, hex(0xEAB308));
        } else {
            const Troph *t = &D->troph[v];
            box(top, 10, 20, 236, 150, 14, WHITE_, hex(tflag_col(t->flag)));
            draw_center(top, &font_big, 128, 30, "Trophée débloqué !", hex(tflag_col(t->flag)), 1);
            star(top, 128, 74, 20, hex(tflag_col(t->flag)));
            int n = wrap(top, &font_big, 18, 100, 220, t->name, INK, 20, 1);
            wrap(top, &font_small, 18, 102 + n * 20, 220, t->desc, hex(0x475569), 14, 1);
        }
        return;
    }
    default: break;
    }
    /* ----- jeu : entrainement, atelier, revision, defi, blitz ----- */
    uint32_t col = cur_col();
    fill_rect(top, 0, 0, SW, 28, hex(col));
    if (mode == M_DEFI || mode == M_BLITZ) snprintf(buf, sizeof buf, "%s : %s", mode_name(mode), D->label);
    else if (D->ncat && mode != M_REVIEW) snprintf(buf, sizeof buf, "%s : %s", mode_name(mode), D->cats[cat].label);
    else snprintf(buf, sizeof buf, "%s : %s", mode_name(mode), D->label);
    draw_text(top, &font_big, 8, 5, buf, WHITE_, 1);

    if (state == S_END) {
        int idx = pickIdx();
        if (mode == M_TRAIN || mode == M_REVIEW) {
            draw_progress(top, 36, col);
            draw_center(top, &font_big, 128, 66, mode == M_REVIEW ? "Révision terminée !" : "Série terminée !", INK, 1);
            snprintf(buf, sizeof buf, "%d / %d", score, nq);
            draw_center(top, &font_big, 128, 92, buf, hex(col), 3);
            if (mode == M_TRAIN) {
                const char *msg = score >= 9 ? "Excellent !" : score >= 7 ? "Très bien !" : score >= 5 ? "Bon travail, continue !" : "Continue à t'entraîner !";
                draw_center(top, &font_big, 128, 150, msg, INK, 1);
                snprintf(buf, sizeof buf, "Meilleur score : %d / %d", GS(gi)->best[idx], NQ);
            } else snprintf(buf, sizeof buf, GS(gi)->nerr ? "Il reste %d phrase(s) à revoir." : "Plus aucune erreur à revoir !", GS(gi)->nerr);
            draw_center(top, &font_small, 128, 174, buf, GREY, 1);
        } else if (mode == M_ATELIER) {
            draw_progress(top, 36, col);
            draw_center(top, &font_big, 128, 64, "Atelier terminé !", INK, 1);
            int st = ses_err == 0 ? 3 : ses_err <= 2 ? 2 : 1;
            draw_stars3(top, 128, 108, st, 16);
            snprintf(buf, sizeof buf, "%d erreur(s) - meilleur : %d étoile(s)", ses_err, GS(gi)->stars[idx]);
            draw_center(top, &font_small, 128, 142, buf, GREY, 1);
            draw_center(top, &font_big, 128, 162, st == 3 ? "Sans faute !" : "Recommence pour 3 étoiles !", INK, 1);
        } else if (mode == M_DEFI) {
            draw_center(top, &font_big, 128, 42, "Défi terminé !", INK, 1);
            snprintf(buf, sizeof buf, "+%d pts", sesPts);
            draw_center(top, &font_big, 128, 66, buf, hex(col), 3);
            snprintf(buf, sizeof buf, "Phrases réussies : %d / %d", ses_ok, ses_ok + ses_err);
            draw_center(top, &font_small, 128, 124, buf, INK, 1);
            snprintf(buf, sizeof buf, "Meilleure série : %d", GS(gi)->bestStreak);
            draw_center(top, &font_small, 128, 142, buf, GREY, 1);
            grade_line(buf, sizeof buf, gi);
            draw_center(top, &font_small, 128, 160, buf, GREY, 1);
        } else {
            draw_center(top, &font_big, 128, 42, "Temps écoulé !", INK, 1);
            snprintf(buf, sizeof buf, "%d", ses_ok);
            draw_center(top, &font_big, 128, 66, buf, hex(col), 3);
            draw_center(top, &font_small, 128, 120, "bonnes réponses", GREY, 1);
            snprintf(buf, sizeof buf, "Erreurs : %d   Meilleur : %d", ses_err, GS(gi)->blitzBestOk);
            draw_center(top, &font_small, 128, 142, buf, INK, 1);
            draw_center(top, &font_small, 128, 160, ses_err == 0 && ses_ok >= 10 ? "Sans aucune erreur : immaculé !" : "Chaque réponse compte !", GREY, 1);
        }
        return;
    }
    /* ligne d'information */
    if (mode == M_DEFI) {
        snprintf(buf, sizeof buf, "Points : %d", GS(gi)->points);
        draw_text(top, &font_small, 8, 32, buf, GREY, 1);
        snprintf(buf, sizeof buf, "Série : %d", streak);
        draw_text(top, &font_small, SW - 8 - text_w(&font_small, buf), 32, buf, GREY, 1);
        int pct = tleft * 100 / tmax;
        draw_bar(top, 8, 50, 240, 8, pct, pct > 50 ? hex(0x22A06B) : pct > 20 ? hex(0xE8861A) : hex(0xDC2626));
    } else if (mode == M_BLITZ) {
        snprintf(buf, sizeof buf, "Bonnes : %d  Erreurs : %d", ses_ok, ses_err);
        draw_text(top, &font_small, 8, 32, buf, GREY, 1);
        int secs = (bzleft + 59) / 60;
        snprintf(buf, sizeof buf, "%d:%02d", secs / 60, secs % 60);
        draw_text(top, &font_big, SW - 8 - text_w(&font_big, buf), 30, buf, secs <= 20 ? hex(0xDC2626) : INK, 1);
        int pct = bzleft * 100 / (BLITZ_SEC * 60);
        draw_bar(top, 8, 50, 240, 8, pct, pct > 20 ? hex(0x9333EA) : hex(0xDC2626));
    } else {
        draw_progress(top, 34, col);
        snprintf(buf, sizeof buf, "Phrase %d / %d", qi + 1, nq);
        draw_text(top, &font_small, 8, 52, buf, GREY, 1);
        snprintf(buf, sizeof buf, "Score : %d", score);
        draw_text(top, &font_small, SW - 8 - text_w(&font_small, buf), 52, buf, GREY, 1);
    }

    if (state == S_PLAY) {
        char m[300];
        mission_text(m, sizeof m);
        int n1 = wrap(top, &font_big, 14, 0, 228, m, INK, 19, 0);
        const char *sub = (D->kind == K_TAG && mode != M_BLITZ && !hintOn) ? D->cats[cat].sub : "";
        int n2 = sub[0] ? wrap(top, &font_small, 14, 0, 228, sub, INK, 14, 0) : 0;
        int bh = 14 + n1 * 19 + n2 * 14;
        box(top, 6, 68, 244, bh, 8, hex(lighten(col, 85)), hex(col));
        wrap(top, &font_big, 14, 74, 228, m, INK, 19, 1);
        if (n2) wrap(top, &font_small, 14, 74 + n1 * 19 + 3, 228, sub, hex(0x475569), 14, 1);
        int yb = 68 + bh + 6;
        if (hintOn) {
            const char *h = D->kind == K_GAP ? D->cats[cat].hint : D->kind == K_TAG ? (D->cats[cat].hint[0] ? D->cats[cat].hint : D->cats[cat].sub) : HINT_KIND[D->kind == K_SEL ? 1 : D->kind == K_SELTYPE ? 2 : 3];
            char hb[300]; snprintf(hb, sizeof hb, "Indice : %s", h);
            wrap(top, &font_small, 10, yb, 236, hb, hex(0xB45309), 14, 1);
        } else if (mode == M_ATELIER) draw_center(top, &font_small, 128, 170, "Touche « Indice » si tu as besoin d'aide", GREY, 1);
        else {
            const char *hint = (D->kind == K_TAG || D->kind == K_SEL) ? "Touche les mots, puis Valider (bouton A)" :
                               D->kind == K_SELTYPE ? "Touche les mots, puis choisis le type" : D->kind == K_TF ? "Réponds avec le stylet" : "";
            if (D->kind == K_GAP) hint = D->cats[cat].hint;
            if (D->kind == K_SELTYPE && cq->extra[0]) { snprintf(buf, sizeof buf, "Verbe : %s", cq->extra); draw_center(top, &font_small, 128, 142, buf, hex(0x475569), 1); }
            draw_center(top, &font_small, 128, 170, hint, GREY, 1);
        }
    } else { /* S_RESULT */
        if (timeUp) draw_center(top, &font_big, 128, 70, "Temps écoulé !", hex(0xEF4444), 1);
        else if (lastCorrect) {
            if (mode == M_DEFI) snprintf(buf, sizeof buf, "Bravo %s ! +%d pts", P->name, lastPts); else snprintf(buf, sizeof buf, "Bravo %s !", P->name);
            draw_center(top, &font_big, 128, 70, buf, hex(0x22A06B), 1);
        } else draw_center(top, &font_big, 128, 70, "Pas tout à fait...", hex(0xEF4444), 1);
        const char *e = cq->expl;
        char eb[300];
        if (!e[0] && D->kind == K_TAG) {   /* explication automatique : mots attendus et leur nature */
            int n = snprintf(eb, sizeof eb, "Il fallait trouver : ");
            int cnt = 0;
            for (int i = 0; i < ntok && n < 240; i++) if (tagv[i] == cat + 1) {
                char w[40]; int l = 0;
                for (const char *q = toks[i]; *q && l < 38; q++) if (!strchr(".,;:!?\"", *q)) w[l++] = *q;
                w[l] = 0;
                n += snprintf(eb + n, sizeof eb - n, "%s%s", cnt++ ? ", " : "", w);
            }
            snprintf(eb + n, sizeof eb - n, " (%s).", D->cats[cat].label);
            e = eb;
        }
        if (e[0]) wrap(top, &font_small, 8, 96, 240, e, hex(0x334155), 14, 1);
        else draw_center(top, &font_small, 128, 96, "Les bonnes réponses sont en couleur.", hex(0x475569), 1);
    }
}

/* ================= ecran du bas ================= */
static void draw_plain_sentence(px *buf, int y, int showAnswer) {
    const Cat *C = (kind() == K_GAP) ? &G()->cats[cat] : 0;
    int x = 8, lh = 26;
    const Font *f = &font_big;
    for (int i = 0; i < ntok; i++) {
        const char *t = toks[i];
        char pre[40] = "", post[40] = "";
        const char *gap = strstr(t, "___");
        int hasgap = gap != 0;
        if (hasgap) { snprintf(pre, sizeof pre, "%.*s", (int)(gap - t), t); snprintf(post, sizeof post, "%s", gap + 3); }
        const char *mid = hasgap ? (showAnswer ? C->ch[cq->a] : "___") : t;
        int w = text_w(f, hasgap ? pre : t) + (hasgap ? text_w(f, mid) + text_w(f, post) : 0);
        if (x + w > SW - 8 && x > 8) { x = 8; y += lh; }
        if (!hasgap) { draw_text(buf, f, x, y, t, INK, 1); x += w + 6; continue; }
        draw_text(buf, f, x, y, pre, INK, 1); x += text_w(f, pre);
        draw_text(buf, f, x, y, mid, showAnswer ? hex(0x16A34A) : hex(0x2563EB), 1); x += text_w(f, mid);
        draw_text(buf, f, x, y, post, INK, 1); x += text_w(f, post) + 6;
    }
}
static const Rect BTN_SWITCH = {4, 172, 126, 18};
static const Rect BTN_BACK2 = {4, 170, 74, 20};
static Rect palier_btn(int i) { Rect r = {4 + (i % 4) * 63, 40 + (i / 4) * 66, 60, 60}; return r; }
static Rect troph_cell(int i) { Rect r = {4 + (i % 5) * 50, 4 + (i / 5) * 32, 46, 29}; return r; }
static Rect cours_prev(void) { Rect r = {4, 146, 78, 40}; return r; }
static Rect cours_back(void) { Rect r = {89, 146, 78, 40}; return r; }
static Rect cours_next(void) { Rect r = {174, 146, 78, 40}; return r; }

static void draw_bottom(void) {
    fill_rect(bot, 0, 0, SW, SH, hex(0xF8FAFC));
    const GameData *D = G();
    char l2[96];
    switch (state) {
    case S_PROFILES: {
        draw_center(bot, &font_small, 128, 3, "Choisis un joueur", GREY, 1);
        int firstFree = -1;
        for (int i = 0; i < NPROF; i++) {
            Rect r = tile2(i);
            if (SF.p[i].used) {
                int pts = 0, tr = 0;
                for (int g = 0; g < NG; g++) { pts += SF.p[i].g[g].points; for (int t = 0; t < GAMES[g].ntroph; t++) tr += (SF.p[i].g[g].troph >> t) & 1; }
                snprintf(l2, sizeof l2, "%d pts - %d trophées", pts, tr);
                draw_tile(bot, r, SF.p[i].name, l2, 0xEAB308, 4);
            } else if (firstFree < 0) { firstFree = i; draw_tile(bot, r, "+ Nouveau joueur", "Écris ton prénom", 0x22A06B, 0); }
        }
        return;
    }
    case S_KEYB:
        for (int row = 0; row < 4; row++)
            for (int col = 0; col < 10; col++) {
                if (!KB[row][col]) continue;
                Rect r = kbkey(row, col, 0);
                box(bot, r.x, r.y, r.w, r.h, 5, WHITE_, hex(0xCBD5E1));
                draw_center(bot, &font_big, r.x + r.w / 2, r.y + 7, KB[row][col], INK, 1);
            }
        draw_small_button(bot, kbkey(4, 0, 0), "Effacer", hex(0xFEE2E2), hex(0xB91C1C));
        draw_small_button(bot, kbkey(4, 1, 0), "Annuler", hex(0xCBD5E1), INK);
        draw_small_button(bot, kbkey(4, 2, 0), "OK", hex(0x16A34A), WHITE_);
        return;
    case S_HOME:
    case S_TRE:
        draw_center(bot, &font_small, 128, 3, state == S_HOME ? "Choisis une application" : "Touche un jeu à plus de 90 % pour déposer", GREY, 1);
        for (int i = 0; i < 8; i++) {
            Rect r = tile2(i);
            if (state == S_HOME) {
                if (i < NG) {
                    snprintf(l2, sizeof l2, "%s", GS(i)->points ? "" : GAMES[i].sub);
                    if (GS(i)->points) snprintf(l2, sizeof l2, "%s - %d pts", GR_LAB[grade_of(GS(i)->points)], GS(i)->points);
                    draw_tile(bot, r, GAMES[i].label, l2, GAMES[i].col, 0);
                } else { snprintf(l2, sizeof l2, "%d / 7 étapes", P->steps); draw_tile(bot, r, "Carte au Trésor", l2, 0xB8862F, 0); }
            } else {
                if (i == 7) { draw_tile(bot, r, "Retour", "à l'accueil", 0x64748B, 1); continue; }
                int mode_, rt = rate_dep(i);
                if (P->dep[i]) { mode_ = 3; snprintf(l2, sizeof l2, "Déposée"); }
                else if (rt >= SEUIL) { mode_ = 2; snprintf(l2, sizeof l2, "Déposer ! (%d %%)", rt); }
                else if (played(i)) { mode_ = 0; snprintf(l2, sizeof l2, stat_n(i) < 20 ? "Déjà %d réponses (20 mini)" : "Taux : %d %%", stat_n(i) < 20 ? stat_n(i) : rate(i)); }
                else { mode_ = 1; snprintf(l2, sizeof l2, "Pas encore joué"); }
                draw_tile(bot, r, GAMES[i].label, l2, GAMES[i].col, mode_);
            }
        }
        if (state == S_HOME) draw_small_button(bot, BTN_SWITCH, "Changer de joueur", hex(0xCBD5E1), INK);
        return;
    case S_CONFIRM: {
        Rect no = {10, 60, 112, 70}, yes = {134, 60, 112, 70};
        draw_button(bot, no, "Non", hex(0x16A34A), WHITE_);
        draw_button(bot, yes, confirmStage == 0 ? "Oui" : "Oui, effacer", hex(0xDC2626), WHITE_);
        return;
    }
    case S_GMENU: {
        draw_small_button(bot, BTN_BACK, "Accueil", hex(0xCBD5E1), INK);
        if (D->kind == K_SEL) draw_small_button(bot, BTN_LANG, LANGN[GS(gi)->lang > 2 ? 0 : GS(gi)->lang], hex(lighten(D->col, 70)), INK);
        else draw_center(bot, &font_small, 150, 3, "Choisis un mode", GREY, 1);
        static const char *const NM[8] = {"Le Cours", "Révision", "Entraînement", "Atelier guidé", "Défi", "Blitz 3 min", "Mon Bilan", "Mes Trophées"};
        int lock = !GS(gi)->cours;
        for (int i = 0; i < 8; i++) {
            Rect r = tile2(i); r.y += 4;
            int md = 0;
            switch (i) {
            case 0: snprintf(l2, sizeof l2, lock ? "À lire en premier !" : "Revoir les règles"); md = lock ? 4 : 0; break;
            case 1: snprintf(l2, sizeof l2, GS(gi)->nerr ? "%d phrase(s) à revoir" : "Aucune erreur", GS(gi)->nerr); md = GS(gi)->nerr ? 4 : 1; break;
            case 2: snprintf(l2, sizeof l2, "10 questions"); md = lock; break;
            case 3: snprintf(l2, sizeof l2, "Avec indice, étoiles"); md = lock; break;
            case 4: snprintf(l2, sizeof l2, "Score et série"); md = lock; break;
            case 5: snprintf(l2, sizeof l2, "Record : %d", GS(gi)->blitzBestOk); md = lock; break;
            case 6: snprintf(l2, sizeof l2, "Taux : %d %%", rate(gi)); md = lock; break;
            default: snprintf(l2, sizeof l2, "%d / %d", total_trophies(gi), D->ntroph); md = lock; break;
            }
            draw_tile(bot, r, NM[i], l2, D->col, md);
        }
        return;
    }
    case S_PICK: {
        draw_small_button(bot, BTN_BACK, "Retour", hex(0xCBD5E1), INK);
        draw_center(bot, &font_small, 150, 3, pickMode == M_ATELIER ? "Choisis pour l'atelier" : "Choisis une catégorie", GREY, 1);
        for (int i = 0; i < NPICK[gi]; i++) {
            if (D->ncat) {
                const Cat *c = &D->cats[i];
                Rect r = pick_btn(i);
                box(bot, r.x, r.y, r.w, r.h, 8, hex(D->kind == K_GAP ? lighten(c->col, 85) : c->light), hex(c->col));
                int b = GS(gi)->best[i], st = GS(gi)->stars[i];
                int has = pickMode == M_ATELIER ? st : b;
                draw_center(bot, &font_small, r.x + r.w / 2, has ? r.y + 8 : r.y + 18, c->label, INK, 1);
                if (has) {
                    if (pickMode == M_ATELIER) draw_stars3(bot, r.x + r.w / 2, r.y + 35, st, 6);
                    else { char s[16]; snprintf(s, sizeof s, "%d/%d", b, NQ); draw_center(bot, &font_small, r.x + r.w / 2, r.y + 28, s, hex(c->col), 1); }
                }
            } else {
                Rect r = list_btn(i);
                box(bot, r.x, r.y, r.w, r.h, 8, hex(lighten(D->col, 82)), hex(D->col));
                draw_center(bot, &font_big, r.x + r.w / 2, r.y + 8, pick_label(gi, i), INK, 1);
            }
        }
        return;
    }
    case S_PALIER: {
        draw_small_button(bot, BTN_BACK, "Retour", hex(0xCBD5E1), INK);
        draw_center(bot, &font_small, 150, 3, "Choisis ton palier", GREY, 1);
        for (int i = 0; i < 8; i++) {
            Rect r = palier_btn(i);
            int cur = GS(gi)->palier == i + 1;
            box(bot, r.x, r.y, r.w, r.h, 8, hex(lighten(D->col, cur ? 60 : 85)), hex(D->col));
            char s[8]; snprintf(s, sizeof s, "%d", i + 1);
            draw_center(bot, &font_big, r.x + r.w / 2, r.y + 8, s, hex(D->col), 2);
            int nc = i + 2 < D->ncat ? i + 2 : D->ncat;
            snprintf(l2, sizeof l2, "%d cat.", nc);
            draw_center(bot, &font_small, r.x + r.w / 2, r.y + 42, l2, GREY, 1);
        }
        return;
    }
    case S_COURS:
        draw_center(bot, &font_small, 128, 30, "Lis bien, puis tourne les pages.", GREY, 1);
        snprintf(l2, sizeof l2, "Page %d sur %d", coursPage + 1, coursN);
        draw_center(bot, &font_big, 128, 70, l2, hex(D->col), 1);
        draw_center(bot, &font_small, 128, 100, "A : page suivante   B : page précédente", GREY, 1);
        draw_button(bot, cours_prev(), "< Page", coursPage > 0 ? hex(D->col) : hex(0xE2E8F0), coursPage > 0 ? WHITE_ : hex(0x94A3B8));
        draw_button(bot, cours_back(), "Retour", hex(0xCBD5E1), INK);
        draw_button(bot, cours_next(), "Page >", coursPage < coursN - 1 ? hex(D->col) : hex(0xE2E8F0), coursPage < coursN - 1 ? WHITE_ : hex(0x94A3B8));
        return;
    case S_BILAN:
        for (int c = 0; c < NSTAT[gi]; c++) {
            int n = GS(gi)->ok[c] + GS(gi)->err[c], pct = n ? GS(gi)->ok[c] * 100 / n : 0;
            int y = 4 + c * 18;
            uint32_t col = D->ncat ? D->cats[c].col : D->col;
            draw_text(bot, &font_small, 4, y, stat_label(gi, c), INK, 1);
            draw_bar(bot, 92, y + 2, 96, 10, n ? pct : 0, hex(col));
            if (n) snprintf(l2, sizeof l2, "%d %%", pct); else snprintf(l2, sizeof l2, "-");
            draw_text(bot, &font_small, 194, y, l2, GREY, 1);
        }
        draw_small_button(bot, BTN_BACK2, "Retour", hex(0xCBD5E1), INK);
        return;
    case S_TROPH:
        for (int i = 0; i < D->ntroph; i++) {
            Rect r = troph_cell(i);
            int has = (GS(gi)->troph >> i) & 1;
            uint32_t col = tflag_col(D->troph[i].flag);
            box(bot, r.x, r.y, r.w, r.h, 6, has ? hex(lighten(col, 80)) : hex(0xF1F5F9), has ? hex(col) : hex(0xCBD5E1));
            star(bot, r.x + r.w / 2, r.y + 14, 10, has ? hex(col) : hex(0xCBD5E1));
            if (i == trophSel) rrect(bot, r.x, r.y + r.h - 3, r.w, 3, 1, hex(0x1E293B));
        }
        draw_small_button(bot, BTN_BACK2, "Retour", hex(0xCBD5E1), INK);
        return;
    case S_POPUP:
        draw_button(bot, (Rect){60, 70, 136, 50}, "Continuer", hex(0x16A34A), WHITE_);
        return;
    default: break;
    }
    if (state == S_END) {
        draw_button(bot, btn_menu(), mode == M_DEFI || mode == M_BLITZ ? "Menu" : "Menu", hex(0xCBD5E1), INK);
        draw_button(bot, btn_main(), mode == M_REVIEW && !GS(gi)->nerr ? "Terminer" : "Rejouer", hex(cur_col()), WHITE_);
        return;
    }
    int k = D->kind;
    uint32_t col = cur_col();
    if (k == K_TAG || k == K_SEL || k == K_SELTYPE) {
        for (int i = 0; i < ntok; i++) {
            Rect r = wbox[i];
            int cand = (k == K_TAG) ? tagv[i] != 0 : 1;
            const Font *f = pillBig ? &font_big : &font_small;
            int tx = pillBig ? 7 : 6, ty = pillBig ? 4 : 3;
            if (!cand) { draw_text(bot, f, r.x + 2, r.y + ty, toks[i], hex(0x334155), 1); continue; }
            px fill = WHITE_, border = hex(0xCBD5E1), txt = INK;
            if (state == S_PLAY) { if (sel[i]) { fill = hex(col); border = fill; txt = WHITE_; } }
            else {
                int tg = (k == K_TAG) ? tagv[i] == cat + 1 : tagv[i] > 0;
                if (tg) { fill = hex(col); border = fill; txt = WHITE_; if (k == K_SELTYPE && tagv[i] == 2) { fill = hex(lighten(col, 20)); border = hex(col); } }
                else if (sel[i]) { fill = hex(0x1F2937); border = fill; txt = WHITE_; }
            }
            box(bot, r.x, r.y, r.w, r.h, pillBig ? 7 : 5, fill, border);
            draw_text(bot, f, r.x + tx, r.y + ty, toks[i], txt, 1);
        }
        if (k == K_SELTYPE && state == S_PLAY) {
            int ok = nsel() > 0;
            for (int t = 0; t < D->ntypes; t++)
                draw_small_button(bot, typebtn(t, D->ntypes), D->longs[t], ok ? hex(col) : hex(0xE2E8F0), ok ? WHITE_ : hex(0x94A3B8));
        }
        if (k == K_SELTYPE && state == S_RESULT) {
            char s[96];
            snprintf(s, sizeof s, "Ton choix : %s   Réponse : %s", ansT >= 0 ? D->types[ansT] : "-", D->types[cq->a]);
            draw_center(bot, &font_small, 128, 126, s, lastCorrect ? hex(0x16A34A) : hex(0xDC2626), 1);
        }
    } else if (k == K_TF) {
        draw_plain_sentence(bot, 26, 0);
        if (state == S_PLAY) {
            if (stage == 0) for (int t = 0; t < 4; t++) draw_small_button(bot, tfbtn(t), D->types[t], hex(col), WHITE_);
            else {
                char s[64]; snprintf(s, sizeof s, "Type : %s", D->types[ansT]);
                draw_center(bot, &font_small, 128, 106, s, GREY, 1);
                for (int t = 0; t < 2; t++) draw_button(bot, tfform(t), D->forms[t], hex(t ? 0xDC2626 : 0x16A34A), WHITE_);
            }
        } else {
            char s[96];
            if (focus != 1) { snprintf(s, sizeof s, "Type : %s", D->types[cq->a]); draw_center(bot, &font_big, 128, 100, s, ansT == cq->a ? hex(0x16A34A) : hex(0xDC2626), 1); }
            if (focus != 0) { snprintf(s, sizeof s, "Forme : %s", D->forms[cq->b]); draw_center(bot, &font_big, 128, 122, s, ansF == cq->b ? hex(0x16A34A) : hex(0xDC2626), 1); }
            if (!lastCorrect) {
                snprintf(s, sizeof s, "Ta réponse : %s / %s", ansT >= 0 ? D->types[ansT] : "-", ansF >= 0 ? D->forms[ansF] : "-");
                draw_center(bot, &font_small, 128, 142, s, GREY, 1);
            }
        }
    } else { /* K_GAP */
        const Cat *C = &D->cats[cat];
        draw_plain_sentence(bot, 26, state == S_RESULT);
        if (state == S_PLAY) {
            for (int t = 0; t < C->nch; t++) draw_button(bot, C->nch == 2 ? tfform(t) : tfbtn(t), C->ch[t], hex(C->col), WHITE_);
        } else if (!lastCorrect) {
            char s[96]; snprintf(s, sizeof s, "Ta réponse : %s", ansG >= 0 ? C->ch[ansG] : "-");
            draw_center(bot, &font_small, 128, 130, s, hex(0xDC2626), 1);
        }
    }
    int endless = mode == M_DEFI || mode == M_BLITZ;
    const char *menuLabel = endless ? "Fin" : "Menu";
    if (state == S_RESULT) {
        draw_button(bot, btn_menu(), menuLabel, hex(0xCBD5E1), INK);
        draw_button(bot, btn_main(), (!endless && qi == nq - 1) ? "Terminer" : "Suivant", hex(0x1E293B), WHITE_);
    } else if (k == K_TAG || k == K_SEL) {
        draw_button(bot, btn_menu(), menuLabel, hex(0xCBD5E1), INK);
        if (mode == M_ATELIER) draw_button(bot, btn_hint(), "Indice", hintOn ? hex(0xFDE68A) : hex(0xFEF3C7), hex(0xB45309));
        int ok = nsel() > 0;
        draw_button(bot, btn_main(), "Valider", ok ? hex(col) : hex(0xE2E8F0), ok ? WHITE_ : hex(0x94A3B8));
    } else if (k == K_SELTYPE) {
        draw_button(bot, btn_menu(), menuLabel, hex(0xCBD5E1), INK);
        if (mode == M_ATELIER) draw_button(bot, btn_hint(), "Indice", hintOn ? hex(0xFDE68A) : hex(0xFEF3C7), hex(0xB45309));
    } else {
        draw_small_button(bot, BTN_SMALLMENU, menuLabel, hex(0xCBD5E1), INK);
        if (mode == M_ATELIER) draw_small_button(bot, BTN_HINT_TOP, "Indice", hintOn ? hex(0xFDE68A) : hex(0xFEF3C7), hex(0xB45309));
    }
}

/* ================= navigation et boucle ================= */
static void go_home(void) { if (dirtySave) save_now(); state = S_HOME; dirty_all(); }
static void go_gmenu(void) {
    if (dirtySave) save_now();
    state = S_GMENU; dirty_all();
    if (popn > 0) { popAfter = PA_BACK; state = S_POPUP; }
}
static void pop_next(void) {
    if (popn > 0) { memmove(popq, popq + 1, popn - 1); popn--; }
    dirty_all();
    if (popn > 0) return;
    if (popAfter == PA_NEXT) next_q();
    else if (popAfter == PA_END) { state = S_END; }
    else go_gmenu();
}
static void select_profile(int i) { SF.cur = (uint8_t)i; P = &SF.p[i]; save_now(); go_home(); }
static void open_mode(int i) {
    int lock = !GS(gi)->cours;
    if (i == 0) { cours_open(); state = S_COURS; dirty_all(); return; }
    if (i == 1) { if (GS(gi)->nerr) start_review(gi); return; }
    if (lock) { flashT = 90; flashMsg = "Lis d'abord le cours !"; dirty_all(); return; }
    if (i == 2 || i == 3) {
        pickMode = i == 2 ? M_TRAIN : M_ATELIER;
        if (NPICK[gi] == 1) start_series(gi, 0, pickMode); else { state = S_PICK; dirty_all(); }
    } else if (i == 4 || i == 5) {
        pickMode = i == 4 ? M_DEFI : M_BLITZ;
        if (has_pal(gi)) { state = S_PALIER; dirty_all(); } else start_endless(gi, pickMode);
    } else if (i == 6) { state = S_BILAN; dirty_all(); }
    else { trophSel = -1; state = S_TROPH; dirty_all(); }
}
static void deposit(int g) {
    P->dep[g] = 1; animStripe = P->steps; if (P->steps < 7) P->steps++;
    anim = 40; save_now(); dirty_all();
}
static void replay(void) {
    if (mode == M_REVIEW) { if (GS(gi)->nerr) start_review(gi); else go_gmenu(); }
    else if (mode == M_DEFI || mode == M_BLITZ) start_endless(gi, mode);
    else start_series(gi, focus, mode);
}
static void do_validate_or_next(void) {
    if (state == S_PLAY && (kind() == K_TAG || kind() == K_SEL) && nsel() > 0) validate_sel(0);
    else if (state == S_RESULT) go_after_result();
}

void game_init(uint16_t *t, uint16_t *b, unsigned seed) {
    top = t; bot = b;
    srand(seed);
    sdok = plat_save_available();
    load_save();
    for (int i = 0; i < 6; i++) recent[i] = -1;
    gi = 0; mode = M_TRAIN; popn = 0; tick = 0; anim = 0;
    state = S_PROFILES;
    int used = 0; for (int i = 0; i < NPROF; i++) used += SF.p[i].used;
    if (used == 1) { for (int i = 0; i < NPROF; i++) if (SF.p[i].used) { SF.cur = (uint8_t)i; P = &SF.p[i]; } }
    if (used == 1 && SF.p[SF.cur].used) state = S_HOME;
    dirty_all();
}

void game_frame(const Input *in) {
    tick++;
    if (anim > 0) { anim--; dirty_all(); }
    if (flashT > 0) { flashT--; if (!flashT) dirty_all(); }
    /* horloges */
    if (state == S_PLAY && mode == M_DEFI) {
        if (--tleft <= 0) { tleft = 0; timeUp = 1; finish_q(0); }
        else if (tleft % 6 == 0) dirtyT = 1;
    }
    if (mode == M_BLITZ && (state == S_PLAY || state == S_RESULT)) {
        if (--bzleft <= 0) { bzleft = 0; if (state == S_PLAY) enter_end(); else dirtyT = 1; }
        else if (bzleft % 6 == 0) dirtyT = 1;
    }
    if (state == S_KEYB && tick % 20 == 0) dirtyT = 1;
    /* test enseignant sur l'ecran du tresor */
    if (state == S_TRE && in->held_l && in->held_r) {
        if (in->down_y && P->steps < 7) { animStripe = P->steps; P->steps++; anim = 40; save_now(); dirty_all(); }
        if (in->down_b) { P->steps = 0; memset(P->dep, 0, sizeof P->dep); save_now(); dirty_all(); }
    }
    /* touches */
    if (in->down_start) {
        switch (state) {
        case S_PROFILES: case S_POPUP: break;
        case S_KEYB: state = S_PROFILES; dirty_all(); break;
        case S_HOME: state = S_PROFILES; dirty_all(); break;
        case S_TRE: case S_CONFIRM: go_home(); break;
        case S_GMENU: go_home(); break;
        case S_PICK: case S_PALIER: case S_COURS: case S_BILAN: case S_TROPH: go_gmenu(); break;
        default: go_gmenu(); break;
        }
    } else if (in->down_select && state == S_HOME) { state = S_CONFIRM; confirmStage = 0; dirty_all(); }
    else if (in->down_a) {
        if (state == S_POPUP) pop_next();
        else if (state == S_COURS) { if (coursPage < coursN - 1) { coursPage++; dirty_all(); } }
        else if (state == S_END) replay();
        else do_validate_or_next();
    } else if (in->down_b) {
        if (state == S_COURS && coursPage > 0) { coursPage--; dirty_all(); }
        else if (state == S_PLAY && mode == M_ATELIER) { hintOn = 1; dirty_all(); }
    }
    if (in->touch_down) {
        int x = in->tx, y = in->ty;
        switch (state) {
        case S_PROFILES: {
            int firstFree = -1;
            for (int i = 0; i < NPROF; i++) {
                if (SF.p[i].used) { if (inside(tile2(i), x, y)) { select_profile(i); break; } }
                else if (firstFree < 0) {
                    firstFree = i;
                    if (inside(tile2(i), x, y)) { newSlot = i; namebuf[0] = 0; state = S_KEYB; dirty_all(); break; }
                }
            }
            break;
        }
        case S_KEYB: {
            int len = (int)strlen(namebuf);
            for (int row = 0; row < 4; row++)
                for (int col = 0; col < 10; col++)
                    if (KB[row][col] && inside(kbkey(row, col, 0), x, y)) {
                        int l = (int)strlen(KB[row][col]);
                        if (len + l <= 10) { memcpy(namebuf + len, KB[row][col], l + 1); dirty_all(); }
                    }
            if (inside(kbkey(4, 0, 0), x, y) && len > 0) {
                len--; while (len > 0 && (namebuf[len] & 0xC0) == 0x80) len--;
                namebuf[len] = 0; dirty_all();
            } else if (inside(kbkey(4, 1, 0), x, y)) { state = S_PROFILES; dirty_all(); }
            else if (inside(kbkey(4, 2, 0), x, y) && len > 0) {
                Profile *np = &SF.p[newSlot];
                memset(np, 0, sizeof *np);
                memcpy(np->name, namebuf, strlen(namebuf) + 1);
                np->used = 1;
                select_profile(newSlot);
            }
            break;
        }
        case S_HOME:
            if (inside(BTN_SWITCH, x, y)) { state = S_PROFILES; dirty_all(); break; }
            for (int i = 0; i < 8; i++) if (inside(tile2(i), x, y)) {
                if (i == 7) { state = S_TRE; dirty_all(); }
                else { gi = i; flashT = 0; state = S_GMENU; dirty_all(); }
                break;
            }
            break;
        case S_GMENU:
            if (inside(BTN_BACK, x, y)) { go_home(); break; }
            if (G()->kind == K_SEL && inside(BTN_LANG, x, y)) { GS(gi)->lang = (uint8_t)((GS(gi)->lang + 1) % 3); save_now(); dirty_all(); break; }
            for (int i = 0; i < 8; i++) { Rect r = tile2(i); r.y += 4; if (inside(r, x, y)) { open_mode(i); break; } }
            break;
        case S_PICK:
            if (inside(BTN_BACK, x, y)) { state = S_GMENU; dirty_all(); break; }
            for (int i = 0; i < NPICK[gi]; i++) {
                Rect r = G()->ncat ? pick_btn(i) : list_btn(i);
                if (inside(r, x, y)) { start_series(gi, i, pickMode); break; }
            }
            break;
        case S_PALIER:
            if (inside(BTN_BACK, x, y)) { state = S_GMENU; dirty_all(); break; }
            for (int i = 0; i < 8; i++) if (inside(palier_btn(i), x, y)) { GS(gi)->palier = (uint8_t)(i + 1); start_endless(gi, pickMode); break; }
            break;
        case S_COURS:
            if (inside(cours_prev(), x, y) && coursPage > 0) { coursPage--; dirty_all(); }
            else if (inside(cours_next(), x, y) && coursPage < coursN - 1) { coursPage++; dirty_all(); }
            else if (inside(cours_back(), x, y)) go_gmenu();
            break;
        case S_BILAN:
            if (inside(BTN_BACK2, x, y)) go_gmenu();
            break;
        case S_TROPH:
            if (inside(BTN_BACK2, x, y)) { go_gmenu(); break; }
            for (int i = 0; i < G()->ntroph; i++) if (inside(troph_cell(i), x, y)) { trophSel = i; dirty_all(); break; }
            break;
        case S_POPUP: pop_next(); break;
        case S_TRE:
            for (int i = 0; i < 8; i++) if (inside(tile2(i), x, y)) {
                if (i == 7) go_home();
                else if (!P->dep[i] && rate_dep(i) >= SEUIL && P->steps < 7) deposit(i);
                dirty_all(); break;
            }
            break;
        case S_CONFIRM: {
            Rect no = {10, 60, 112, 70}, yes = {134, 60, 112, 70};
            if (inside(no, x, y)) go_home();
            else if (inside(yes, x, y)) {
                if (confirmStage == 0) { confirmStage = 1; dirty_all(); }
                else { memset(P, 0, sizeof *P); save_now(); state = S_PROFILES; dirty_all(); }
            }
            break;
        }
        case S_END:
            if (inside(btn_menu(), x, y)) go_gmenu();
            else if (inside(btn_main(), x, y)) replay();
            break;
        default: { /* S_PLAY, S_RESULT */
            int k = kind(), endless = mode == M_DEFI || mode == M_BLITZ;
            if (state == S_RESULT) {
                if (k == K_TF || k == K_GAP) { if (inside(btn_menu(), x, y) || inside(BTN_SMALLMENU, x, y)) { if (endless) enter_end(); else go_gmenu(); break; } }
                if (inside(btn_menu(), x, y)) { if (endless) enter_end(); else go_gmenu(); }
                else if (inside(btn_main(), x, y)) go_after_result();
                break;
            }
            if (k == K_TAG || k == K_SEL || k == K_SELTYPE) {
                if (inside(btn_menu(), x, y)) { if (endless) enter_end(); else go_gmenu(); }
                else if (mode == M_ATELIER && inside(btn_hint(), x, y)) { hintOn = 1; dirty_all(); }
                else if (k != K_SELTYPE && inside(btn_main(), x, y)) { if (nsel() > 0) validate_sel(0); }
                else {
                    int hit = 0;
                    if (k == K_SELTYPE && nsel() > 0)
                        for (int t = 0; t < G()->ntypes; t++) if (inside(typebtn(t, G()->ntypes), x, y)) { validate_sel(t); hit = 1; break; }
                    if (!hit)
                        for (int i = 0; i < ntok; i++) {
                            Rect r = wbox[i];
                            r.x -= 3; r.y -= 3; r.w += 6; r.h += 6;
                            int cand = (k == K_TAG) ? tagv[i] != 0 : 1;
                            if (cand && inside(r, x, y)) { sel[i] = !sel[i]; dirty_all(); break; }
                        }
                }
            } else if (k == K_TF) {
                if (inside(BTN_SMALLMENU, x, y)) { if (endless) enter_end(); else go_gmenu(); }
                else if (mode == M_ATELIER && inside(BTN_HINT_TOP, x, y)) { hintOn = 1; dirty_all(); }
                else if (stage == 0) {
                    for (int t = 0; t < 4; t++) if (inside(tfbtn(t), x, y)) {
                        ansT = t;
                        if (focus == 0) finish_q(ansT == cq->a); else { stage = 1; dirty_all(); }
                        break;
                    }
                } else if (focus != 1 || 1) {
                    for (int t = 0; t < 2; t++) if (inside(tfform(t), x, y)) { ansF = t; finish_q((focus == 1 ? 1 : ansT == cq->a) && ansF == cq->b); break; }
                }
            } else { /* K_GAP */
                const Cat *C = &G()->cats[cat];
                if (inside(BTN_SMALLMENU, x, y)) { if (endless) enter_end(); else go_gmenu(); }
                else if (mode == M_ATELIER && inside(BTN_HINT_TOP, x, y)) { hintOn = 1; dirty_all(); }
                else for (int t = 0; t < C->nch; t++) {
                    Rect r = C->nch == 2 ? tfform(t) : tfbtn(t);
                    if (inside(r, x, y)) { ansG = t; finish_q(t == cq->a); break; }
                }
            }
        }
        }
    }
    if (dirtyT) { draw_top(); dirtyT = 0; }
    if (dirtyB) { draw_bottom(); dirtyB = 0; }
}
