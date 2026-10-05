/* Genere par tools/gen_hub.py - ne pas modifier a la main */
#ifndef GAMEDATA_H
#define GAMEDATA_H
#define MAXTOK 18
enum { K_TAG, K_SEL, K_SELTYPE, K_TF, K_GAP };
typedef struct { const char *text, *extra, *expl; unsigned char tags[MAXTOK]; unsigned char a, b, cat; } Q;
typedef struct { const char *label, *find, *plural, *sub, *hint; unsigned col, light; const char *ch[4]; int nch; } Cat;
typedef struct { const char *id, *name, *desc; int flag; } Troph;   /* flag : 0 normal, 1 vitesse, 2 legendaire, 3 ultime */
typedef struct {
  const char *id, *label, *sub; unsigned col; int kind;
  int ncat; const Cat *cats; const Q *qs; int nq;
  int ntypes; const char *const *types; const char *const *longs;
  int nforms; const char *const *forms;
  int ntroph; const Troph *troph; const char *cours, *cours_en;
} GameData;
#define NGAMES 7
extern const GameData GAMES[NGAMES];
#endif
