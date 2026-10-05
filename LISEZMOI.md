# GrammaQuest Hub — version Nintendo DS

Accueil : 7 jeux (Les déterminants, Nature des mots, Le Sujet, Type & Forme, COD & COI, Circonstanciels,
Homophones) + la Carte au Trésor. Plusieurs joueurs (8 prénoms), saisis au clavier tactile.

## Dans chaque jeu
- **Le Cours** : à lire en premier (débloque les autres modes), pages tournées avec le stylet ou A / B.
- **Révision** : rejoue les phrases où tu t'es trompé (jusqu'à 24 mémorisées).
- **Entraînement** : séries de 10 questions par catégorie.
- **Atelier guidé** : séries de 10 avec bouton Indice, 1 à 3 étoiles selon les erreurs.
- **Défi** : phrases sans fin avec chrono par phrase, points (jusqu'à 45 par phrase), série, paliers 1 à 8
  (Nature des mots, Déterminants, Homophones : plus de catégories à chaque palier).
- **Blitz 3 minutes** : maximum de bonnes réponses en 3 minutes.
- **Mon Bilan** : taux de réussite par catégorie, grade, trophées.
- **Mes Trophées** : 25 trophées par jeu (normaux, vitesse, légendaires, ultime), fenêtres de félicitations, 6 grades.
- Le Sujet : bouton de langue en haut à droite du menu (Français / English / FR + EN).

## Commandes
Stylet = réponses. A = Valider / Suivant / page suivante. B = page précédente / indice (atelier).
START = retour. SELECT (accueil) = effacer le joueur. Sur l'accueil : « Changer de joueur ».

## Carte au Trésor
Touche un jeu dont le taux global est d'au moins 90 % (20 réponses minimum) pour déposer sa carte (une fois par jeu).
Chaque dépôt découvre une bande de l'image « Bravo ! Bon travail de grammaire ! » (7 bandes).
Test enseignant : sur l'écran du trésor, L + R maintenus puis Y = étape suivante, L + R puis B = remise à zéro.

## Sauvegarde
Fichier `GrammaQuest_DS.sav` à la racine de la carte SD. L'accueil indique si la carte est reconnue.

## Compiler / régénérer
`bash build.sh` dans l'image devkitpro/devkitarm (GitHub Actions le fait tout seul à chaque envoi).
`python3 tools/gen_hub.py GrammaQuest_Hub.html` régénère `gamedata.c` et `font_data.c` (Python, Pillow et node requis).
`gcc -fsanitize=address,undefined tools/host_test.c` : test complet sur ordinateur (images dans /tmp).
