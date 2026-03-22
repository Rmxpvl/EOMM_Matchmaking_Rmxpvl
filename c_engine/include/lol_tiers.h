/*
 * lol_tiers.h
 *
 * League of Legends rank (tier/division) system.
 *
 * Tiers:  IRON < BRONZE < SILVER < GOLD < PLATINUM < DIAMOND < MASTER
 * Divisions within each tier (except Master): IV (3) > III (2) > II (1) > I (0)
 *
 * MMR ranges:
 *   Iron:     400 – 700
 *   Bronze:   700 – 1100
 *   Silver:  1100 – 1500
 *   Gold:    1500 – 1900
 *   Platinum:1900 – 2300
 *   Diamond: 2300 – 2700
 *   Master:  2700+
 */

#ifndef LOL_TIERS_H
#define LOL_TIERS_H

typedef enum {
    TIER_IRON     = 0,
    TIER_BRONZE   = 1,
    TIER_SILVER   = 2,
    TIER_GOLD     = 3,
    TIER_PLATINUM = 4,
    TIER_DIAMOND  = 5,
    TIER_MASTER   = 6
} TierEnum;

/* MMR boundaries (lower bound is inclusive) */
#define TIER_IRON_MMR_MIN      400.0f
#define TIER_BRONZE_MMR_MIN    700.0f
#define TIER_SILVER_MMR_MIN   1100.0f
#define TIER_GOLD_MMR_MIN     1500.0f
#define TIER_PLATINUM_MMR_MIN 1900.0f
#define TIER_DIAMOND_MMR_MIN  2300.0f
#define TIER_MASTER_MMR_MIN   2700.0f

/* Number of divisions per tier (IV=3 through I=0); Master has none */
#define TIER_DIVISIONS 4

/*
 * mmrToTier - return the tier corresponding to a given visible MMR value.
 * Values below TIER_IRON_MMR_MIN are clamped to TIER_IRON.
 */
static inline TierEnum mmrToTier(float mmr) {
    if (mmr >= TIER_MASTER_MMR_MIN)   return TIER_MASTER;
    if (mmr >= TIER_DIAMOND_MMR_MIN)  return TIER_DIAMOND;
    if (mmr >= TIER_PLATINUM_MMR_MIN) return TIER_PLATINUM;
    if (mmr >= TIER_GOLD_MMR_MIN)     return TIER_GOLD;
    if (mmr >= TIER_SILVER_MMR_MIN)   return TIER_SILVER;
    if (mmr >= TIER_BRONZE_MMR_MIN)   return TIER_BRONZE;
    return TIER_IRON;
}

/*
 * mmrToDivision - return division (0=I, 1=II, 2=III, 3=IV) within a tier.
 * Master has no division (always returns 0).
 * Division boundaries split each 400-MMR tier band into four equal quarters.
 */
static inline int mmrToDivision(float mmr, TierEnum tier) {
    float tier_min;
    float tier_width;

    if (tier == TIER_MASTER) return 0;

    switch (tier) {
        case TIER_IRON:     tier_min = TIER_IRON_MMR_MIN;     tier_width = 300.0f; break;
        case TIER_BRONZE:   tier_min = TIER_BRONZE_MMR_MIN;   tier_width = 400.0f; break;
        case TIER_SILVER:   tier_min = TIER_SILVER_MMR_MIN;   tier_width = 400.0f; break;
        case TIER_GOLD:     tier_min = TIER_GOLD_MMR_MIN;     tier_width = 400.0f; break;
        case TIER_PLATINUM: tier_min = TIER_PLATINUM_MMR_MIN; tier_width = 400.0f; break;
        case TIER_DIAMOND:  tier_min = TIER_DIAMOND_MMR_MIN;  tier_width = 400.0f; break;
        default:            return 0;
    }

    float offset = mmr - tier_min;
    if (offset < 0.0f) offset = 0.0f;
    float div_width = tier_width / (float)TIER_DIVISIONS;
    int div = (int)(offset / div_width);
    /* Division index: 0=I (top), 3=IV (bottom). Higher MMR in tier = lower div index. */
    if (div >= TIER_DIVISIONS) div = TIER_DIVISIONS - 1;
    return (TIER_DIVISIONS - 1) - div;
}

/*
 * tierName - return a human-readable string for a tier.
 */
static inline const char *tierName(TierEnum tier) {
    switch (tier) {
        case TIER_IRON:     return "Iron";
        case TIER_BRONZE:   return "Bronze";
        case TIER_SILVER:   return "Silver";
        case TIER_GOLD:     return "Gold";
        case TIER_PLATINUM: return "Platinum";
        case TIER_DIAMOND:  return "Diamond";
        case TIER_MASTER:   return "Master";
        default:            return "Unknown";
    }
}

#endif /* LOL_TIERS_H */
