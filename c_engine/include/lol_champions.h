/*
 * lol_champions.h
 *
 * League of Legends champion database with primary and secondary roles.
 * Champions are indexed 1..LOL_CHAMP_COUNT (index 0 is a sentinel).
 *
 * Role constants (0-indexed):
 *   ROLE_TOP     = 0
 *   ROLE_JUNGLE  = 1
 *   ROLE_MID     = 2
 *   ROLE_ADC     = 3
 *   ROLE_SUPPORT = 4
 */

#ifndef LOL_CHAMPIONS_H
#define LOL_CHAMPIONS_H

#include <string.h>

/* Role constants (0-indexed, matching LoL role names) */
#define ROLE_TOP     0
#define ROLE_JUNGLE  1
#define ROLE_MID     2
#define ROLE_ADC     3
#define ROLE_SUPPORT 4

/* Total number of champions in the database */
#define LOL_CHAMP_COUNT 170

typedef struct {
    int  id;
    char name[24];
    int  primary_role;
    int  secondary_role;
} LolChampion;

/*
 * Champion database (IDs 1..LOL_CHAMP_COUNT).
 * Index 0 is a sentinel (unused) with id=0, name="", roles=-1.
 */
static const LolChampion LOL_CHAMPIONS[LOL_CHAMP_COUNT + 1] = {
    /* 0 – sentinel */
    { 0,   "",              -1,           -1          },
    /* 1-10 */
    { 1,   "Aatrox",        ROLE_TOP,     ROLE_JUNGLE },
    { 2,   "Ahri",          ROLE_MID,     ROLE_SUPPORT},
    { 3,   "Akali",         ROLE_MID,     ROLE_TOP    },
    { 4,   "Akshan",        ROLE_MID,     ROLE_TOP    },
    { 5,   "Alistar",       ROLE_SUPPORT, ROLE_JUNGLE },
    { 6,   "Ambessa",       ROLE_TOP,     ROLE_JUNGLE },
    { 7,   "Amumu",         ROLE_JUNGLE,  ROLE_SUPPORT},
    { 8,   "Anivia",        ROLE_MID,     ROLE_SUPPORT},
    { 9,   "Annie",         ROLE_MID,     ROLE_SUPPORT},
    { 10,  "Aphelios",      ROLE_ADC,     ROLE_SUPPORT},
    /* 11-20 */
    { 11,  "Ashe",          ROLE_ADC,     ROLE_SUPPORT},
    { 12,  "Aurelion Sol",  ROLE_MID,     ROLE_TOP    },
    { 13,  "Aurora",        ROLE_MID,     ROLE_TOP    },
    { 14,  "Azir",          ROLE_MID,     ROLE_TOP    },
    { 15,  "Bard",          ROLE_SUPPORT, ROLE_JUNGLE },
    { 16,  "Bel'Veth",      ROLE_JUNGLE,  ROLE_TOP    },
    { 17,  "Blitzcrank",    ROLE_SUPPORT, ROLE_TOP    },
    { 18,  "Brand",         ROLE_SUPPORT, ROLE_MID    },
    { 19,  "Braum",         ROLE_SUPPORT, ROLE_TOP    },
    { 20,  "Briar",         ROLE_JUNGLE,  ROLE_TOP    },
    /* 21-30 */
    { 21,  "Caitlyn",       ROLE_ADC,     ROLE_SUPPORT},
    { 22,  "Camille",       ROLE_TOP,     ROLE_JUNGLE },
    { 23,  "Cassiopeia",    ROLE_MID,     ROLE_TOP    },
    { 24,  "Cho'Gath",      ROLE_TOP,     ROLE_JUNGLE },
    { 25,  "Corki",         ROLE_MID,     ROLE_ADC    },
    { 26,  "Darius",        ROLE_TOP,     ROLE_MID    },
    { 27,  "Diana",         ROLE_JUNGLE,  ROLE_MID    },
    { 28,  "Dr. Mundo",     ROLE_TOP,     ROLE_JUNGLE },
    { 29,  "Draven",        ROLE_ADC,     ROLE_TOP    },
    { 30,  "Ekko",          ROLE_JUNGLE,  ROLE_MID    },
    /* 31-40 */
    { 31,  "Elise",         ROLE_JUNGLE,  ROLE_SUPPORT},
    { 32,  "Evelynn",       ROLE_JUNGLE,  ROLE_MID    },
    { 33,  "Ezreal",        ROLE_ADC,     ROLE_MID    },
    { 34,  "Fiddlesticks",  ROLE_JUNGLE,  ROLE_SUPPORT},
    { 35,  "Fiora",         ROLE_TOP,     ROLE_MID    },
    { 36,  "Fizz",          ROLE_MID,     ROLE_JUNGLE },
    { 37,  "Galio",         ROLE_MID,     ROLE_SUPPORT},
    { 38,  "Gangplank",     ROLE_TOP,     ROLE_MID    },
    { 39,  "Garen",         ROLE_TOP,     ROLE_MID    },
    { 40,  "Gnar",          ROLE_TOP,     ROLE_MID    },
    /* 41-50 */
    { 41,  "Gragas",        ROLE_JUNGLE,  ROLE_TOP    },
    { 42,  "Graves",        ROLE_ADC,     ROLE_JUNGLE },
    { 43,  "Gwen",          ROLE_TOP,     ROLE_JUNGLE },
    { 44,  "Hecarim",       ROLE_JUNGLE,  ROLE_TOP    },
    { 45,  "Heimerdinger",  ROLE_MID,     ROLE_TOP    },
    { 46,  "Hwei",          ROLE_MID,     ROLE_SUPPORT},
    { 47,  "Illaoi",        ROLE_TOP,     ROLE_MID    },
    { 48,  "Irelia",        ROLE_TOP,     ROLE_MID    },
    { 49,  "Ivern",         ROLE_JUNGLE,  ROLE_SUPPORT},
    { 50,  "Janna",         ROLE_SUPPORT, ROLE_MID    },
    /* 51-60 */
    { 51,  "Jarvan IV",     ROLE_JUNGLE,  ROLE_TOP    },
    { 52,  "Jax",           ROLE_TOP,     ROLE_JUNGLE },
    { 53,  "Jayce",         ROLE_TOP,     ROLE_MID    },
    { 54,  "Jhin",          ROLE_ADC,     ROLE_SUPPORT},
    { 55,  "Jinx",          ROLE_ADC,     ROLE_SUPPORT},
    { 56,  "K'Sante",       ROLE_TOP,     ROLE_SUPPORT},
    { 57,  "Kai'Sa",        ROLE_ADC,     ROLE_JUNGLE },
    { 58,  "Kalista",       ROLE_ADC,     ROLE_SUPPORT},
    { 59,  "Karma",         ROLE_SUPPORT, ROLE_MID    },
    { 60,  "Karthus",       ROLE_JUNGLE,  ROLE_MID    },
    /* 61-70 */
    { 61,  "Kassadin",      ROLE_MID,     ROLE_TOP    },
    { 62,  "Katarina",      ROLE_MID,     ROLE_JUNGLE },
    { 63,  "Kayle",         ROLE_TOP,     ROLE_MID    },
    { 64,  "Kayn",          ROLE_JUNGLE,  ROLE_TOP    },
    { 65,  "Kennen",        ROLE_TOP,     ROLE_MID    },
    { 66,  "Kha'Zix",       ROLE_JUNGLE,  ROLE_MID    },
    { 67,  "Kindred",       ROLE_JUNGLE,  ROLE_ADC    },
    { 68,  "Kled",          ROLE_TOP,     ROLE_JUNGLE },
    { 69,  "Kog'Maw",       ROLE_ADC,     ROLE_MID    },
    { 70,  "LeBlanc",       ROLE_MID,     ROLE_SUPPORT},
    /* 71-80 */
    { 71,  "Lee Sin",       ROLE_JUNGLE,  ROLE_TOP    },
    { 72,  "Leona",         ROLE_SUPPORT, ROLE_TOP    },
    { 73,  "Lillia",        ROLE_JUNGLE,  ROLE_SUPPORT},
    { 74,  "Lissandra",     ROLE_MID,     ROLE_SUPPORT},
    { 75,  "Lucian",        ROLE_ADC,     ROLE_MID    },
    { 76,  "Lulu",          ROLE_SUPPORT, ROLE_MID    },
    { 77,  "Lux",           ROLE_SUPPORT, ROLE_MID    },
    { 78,  "Malphite",      ROLE_TOP,     ROLE_SUPPORT},
    { 79,  "Malzahar",      ROLE_MID,     ROLE_SUPPORT},
    { 80,  "Maokai",        ROLE_SUPPORT, ROLE_JUNGLE },
    /* 81-90 */
    { 81,  "Master Yi",     ROLE_JUNGLE,  ROLE_TOP    },
    { 82,  "Mel",           ROLE_SUPPORT, ROLE_MID    },
    { 83,  "Milio",         ROLE_SUPPORT, ROLE_MID    },
    { 84,  "Miss Fortune",  ROLE_ADC,     ROLE_SUPPORT},
    { 85,  "Mordekaiser",   ROLE_TOP,     ROLE_JUNGLE },
    { 86,  "Morgana",       ROLE_SUPPORT, ROLE_JUNGLE },
    { 87,  "Naafiri",       ROLE_JUNGLE,  ROLE_MID    },
    { 88,  "Nami",          ROLE_SUPPORT, ROLE_MID    },
    { 89,  "Nasus",         ROLE_TOP,     ROLE_JUNGLE },
    { 90,  "Nautilus",      ROLE_SUPPORT, ROLE_TOP    },
    /* 91-100 */
    { 91,  "Neeko",         ROLE_MID,     ROLE_SUPPORT},
    { 92,  "Nidalee",       ROLE_JUNGLE,  ROLE_MID    },
    { 93,  "Nilah",         ROLE_ADC,     ROLE_SUPPORT},
    { 94,  "Nocturne",      ROLE_JUNGLE,  ROLE_TOP    },
    { 95,  "Nunu & Willump",ROLE_JUNGLE,  ROLE_SUPPORT},
    { 96,  "Olaf",          ROLE_JUNGLE,  ROLE_TOP    },
    { 97,  "Orianna",       ROLE_MID,     ROLE_SUPPORT},
    { 98,  "Ornn",          ROLE_TOP,     ROLE_SUPPORT},
    { 99,  "Pantheon",      ROLE_SUPPORT, ROLE_TOP    },
    { 100, "Poppy",         ROLE_TOP,     ROLE_JUNGLE },
    /* 101-110 */
    { 101, "Pyke",          ROLE_SUPPORT, ROLE_JUNGLE },
    { 102, "Qiyana",        ROLE_MID,     ROLE_JUNGLE },
    { 103, "Quinn",         ROLE_TOP,     ROLE_ADC    },
    { 104, "Rakan",         ROLE_SUPPORT, ROLE_MID    },
    { 105, "Rammus",        ROLE_JUNGLE,  ROLE_SUPPORT},
    { 106, "Rek'Sai",       ROLE_JUNGLE,  ROLE_TOP    },
    { 107, "Rell",          ROLE_SUPPORT, ROLE_TOP    },
    { 108, "Renata Glasc",  ROLE_SUPPORT, ROLE_MID    },
    { 109, "Renekton",      ROLE_TOP,     ROLE_JUNGLE },
    { 110, "Rengar",        ROLE_JUNGLE,  ROLE_TOP    },
    /* 111-120 */
    { 111, "Riven",         ROLE_TOP,     ROLE_JUNGLE },
    { 112, "Rumble",        ROLE_TOP,     ROLE_MID    },
    { 113, "Ryze",          ROLE_MID,     ROLE_TOP    },
    { 114, "Samira",        ROLE_ADC,     ROLE_SUPPORT},
    { 115, "Sejuani",       ROLE_JUNGLE,  ROLE_SUPPORT},
    { 116, "Senna",         ROLE_SUPPORT, ROLE_ADC    },
    { 117, "Seraphine",     ROLE_SUPPORT, ROLE_MID    },
    { 118, "Sett",          ROLE_TOP,     ROLE_SUPPORT},
    { 119, "Shaco",         ROLE_JUNGLE,  ROLE_SUPPORT},
    { 120, "Shen",          ROLE_TOP,     ROLE_SUPPORT},
    /* 121-130 */
    { 121, "Shyvana",       ROLE_JUNGLE,  ROLE_TOP    },
    { 122, "Singed",        ROLE_TOP,     ROLE_JUNGLE },
    { 123, "Sion",          ROLE_TOP,     ROLE_SUPPORT},
    { 124, "Sivir",         ROLE_ADC,     ROLE_SUPPORT},
    { 125, "Skarner",       ROLE_JUNGLE,  ROLE_TOP    },
    { 126, "Smolder",       ROLE_ADC,     ROLE_MID    },
    { 127, "Sona",          ROLE_SUPPORT, ROLE_MID    },
    { 128, "Soraka",        ROLE_SUPPORT, ROLE_MID    },
    { 129, "Swain",         ROLE_SUPPORT, ROLE_MID    },
    { 130, "Sylas",         ROLE_MID,     ROLE_JUNGLE },
    /* 131-140 */
    { 131, "Syndra",        ROLE_MID,     ROLE_SUPPORT},
    { 132, "Tahm Kench",    ROLE_SUPPORT, ROLE_TOP    },
    { 133, "Taliyah",       ROLE_JUNGLE,  ROLE_MID    },
    { 134, "Talon",         ROLE_JUNGLE,  ROLE_MID    },
    { 135, "Taric",         ROLE_SUPPORT, ROLE_TOP    },
    { 136, "Teemo",         ROLE_TOP,     ROLE_SUPPORT},
    { 137, "Thresh",        ROLE_SUPPORT, ROLE_ADC    },
    { 138, "Tristana",      ROLE_ADC,     ROLE_MID    },
    { 139, "Trundle",       ROLE_JUNGLE,  ROLE_TOP    },
    { 140, "Tryndamere",    ROLE_TOP,     ROLE_JUNGLE },
    /* 141-150 */
    { 141, "Twisted Fate",  ROLE_MID,     ROLE_ADC    },
    { 142, "Twitch",        ROLE_ADC,     ROLE_JUNGLE },
    { 143, "Udyr",          ROLE_JUNGLE,  ROLE_TOP    },
    { 144, "Urgot",         ROLE_TOP,     ROLE_JUNGLE },
    { 145, "Varus",         ROLE_ADC,     ROLE_SUPPORT},
    { 146, "Vayne",         ROLE_ADC,     ROLE_TOP    },
    { 147, "Veigar",        ROLE_MID,     ROLE_SUPPORT},
    { 148, "Vel'Koz",       ROLE_SUPPORT, ROLE_MID    },
    { 149, "Vex",           ROLE_MID,     ROLE_SUPPORT},
    { 150, "Vi",            ROLE_JUNGLE,  ROLE_TOP    },
    /* 151-160 */
    { 151, "Viego",         ROLE_JUNGLE,  ROLE_MID    },
    { 152, "Viktor",        ROLE_MID,     ROLE_TOP    },
    { 153, "Vladimir",      ROLE_MID,     ROLE_TOP    },
    { 154, "Volibear",      ROLE_JUNGLE,  ROLE_TOP    },
    { 155, "Warwick",       ROLE_JUNGLE,  ROLE_TOP    },
    { 156, "Wukong",        ROLE_JUNGLE,  ROLE_TOP    },
    { 157, "Xayah",         ROLE_ADC,     ROLE_SUPPORT},
    { 158, "Xerath",        ROLE_SUPPORT, ROLE_MID    },
    { 159, "Xin Zhao",      ROLE_JUNGLE,  ROLE_TOP    },
    { 160, "Yasuo",         ROLE_MID,     ROLE_TOP    },
    /* 161-170 */
    { 161, "Yone",          ROLE_MID,     ROLE_TOP    },
    { 162, "Yorick",        ROLE_TOP,     ROLE_MID    },
    { 163, "Yuumi",         ROLE_SUPPORT, ROLE_MID    },
    { 164, "Zac",           ROLE_JUNGLE,  ROLE_SUPPORT},
    { 165, "Zed",           ROLE_JUNGLE,  ROLE_MID    },
    { 166, "Zeri",          ROLE_ADC,     ROLE_JUNGLE },
    { 167, "Ziggs",         ROLE_ADC,     ROLE_MID    },
    { 168, "Zilean",        ROLE_SUPPORT, ROLE_MID    },
    { 169, "Zoe",           ROLE_MID,     ROLE_SUPPORT},
    { 170, "Zyra",          ROLE_SUPPORT, ROLE_MID    }
};

/*
 * lolChampName - return the display name for champion id (1..LOL_CHAMP_COUNT).
 * Returns "" for out-of-range ids.
 */
static inline const char *lolChampName(int id) {
    if (id < 1 || id > LOL_CHAMP_COUNT) return "";
    return LOL_CHAMPIONS[id].name;
}

/*
 * lolChampIdByName - linear search for a champion id by exact name match.
 * Returns 0 if not found.
 */
static inline int lolChampIdByName(const char *name) {
    int i;
    for (i = 1; i <= LOL_CHAMP_COUNT; i++) {
        if (strcmp(LOL_CHAMPIONS[i].name, name) == 0) return i;
    }
    return 0;
}

/*
 * lolChampPrimaryRole / lolChampSecondaryRole - role accessors.
 */
static inline int lolChampPrimaryRole(int id) {
    if (id < 1 || id > LOL_CHAMP_COUNT) return ROLE_MID;
    return LOL_CHAMPIONS[id].primary_role;
}

static inline int lolChampSecondaryRole(int id) {
    if (id < 1 || id > LOL_CHAMP_COUNT) return ROLE_TOP;
    return LOL_CHAMPIONS[id].secondary_role;
}

#endif /* LOL_CHAMPIONS_H */
