-----------------------------------
-- Player bots: fixed gear per job and level band (custom, not retail)
-- A bot wears the set of the highest band at or below its level. Every item must exist, fit its slot and be
-- wearable by the job at the band's first level; xi.bots.checkGear() verifies that at startup.
-- First draft generated from item_equipment (armour from hand-picked leveling sets, weapons by damage per delay).
-----------------------------------
xi = xi or {}
xi.bots = xi.bots or {}

xi.bots.gearBands = { 1, 10, 20, 30, 40, 51, 61, 71 }

xi.bots.gear =
{
    [xi.job.WAR] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_AXE,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BUTTERFLY_AXE,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.SCALE_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.SCALE_CUISSES,
            [xi.slot.FEET] = xi.item.SCALE_GREAVES,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.GREATAXE,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.MILITARY_AXE,
            [xi.slot.HEAD] = xi.item.BRASS_MASK,
            [xi.slot.BODY] = xi.item.BRASS_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.BRASS_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.BRASS_CUISSES,
            [xi.slot.FEET] = xi.item.BRASS_GREAVES,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.HEAVY_AXE,
            [xi.slot.HEAD] = xi.item.IRON_VISOR,
            [xi.slot.BODY] = xi.item.IRON_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.IRON_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.IRON_CUISSES,
            [xi.slot.FEET] = xi.item.IRON_GREAVES,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.GIGANT_AXE,
            [xi.slot.HEAD] = xi.item.STEEL_VISOR,
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.STEEL_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.HEAVY_DARKSTEEL_AXE,
            [xi.slot.HEAD] = 13878, -- carapace_helm
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.DARKSTEEL_MUFFLERS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.RUNE_CHOPPER,
            [xi.slot.HEAD] = 15262, -- hydra_salade
            [xi.slot.BODY] = xi.item.HYDRA_HAUBERT,
            [xi.slot.HANDS] = 14926, -- hydra_moufles
            [xi.slot.LEGS] = 15597, -- hydra_brayettes
            [xi.slot.FEET] = 15682, -- hydra_sollerets
        },
    },
    [xi.job.MNK] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.CESTI,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BRASS_KNUCKLES,
            [xi.slot.BODY] = xi.item.TUNIC,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLACKS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.METAL_KNUCKLES,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.CLAWS,
            [xi.slot.HEAD] = xi.item.BEETLE_MASK,
            [xi.slot.BODY] = xi.item.BEETLE_HARNESS,
            [xi.slot.HANDS] = xi.item.BEETLE_MITTENS,
            [xi.slot.LEGS] = xi.item.BEETLE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BEETLE_LEGGINGS,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_KNUCKLES,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.PATAS,
            [xi.slot.HEAD] = xi.item.CARAPACE_MASK,
            [xi.slot.BODY] = xi.item.CARAPACE_HARNESS,
            [xi.slot.HANDS] = xi.item.CARAPACE_MITTENS,
            [xi.slot.LEGS] = xi.item.CARAPACE_SUBLIGAR,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.JAMADHARS,
            [xi.slot.HEAD] = 13878, -- carapace_helm
            [xi.slot.BODY] = xi.item.SILK_COAT,
            [xi.slot.HANDS] = 12732, -- silk_cuffs
            [xi.slot.LEGS] = xi.item.SILK_SLOPS,
            [xi.slot.FEET] = xi.item.SILK_PUMPS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.TIGERFANGS,
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
    [xi.job.WHM] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.ASH_CLUB,
            [xi.slot.SUB] = xi.item.LAUAN_SHIELD,
            [xi.slot.BODY] = xi.item.ROBE,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLOPS,
            [xi.slot.FEET] = xi.item.ASH_CLOGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_HAMMER,
            [xi.slot.SUB] = xi.item.MAPLE_SHIELD,
            [xi.slot.BODY] = xi.item.TUNIC,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLACKS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.MACE,
            [xi.slot.SUB] = xi.item.MAHOGANY_SHIELD,
            [xi.slot.HEAD] = xi.item.BRASS_HAIRPIN,
            [xi.slot.BODY] = xi.item.LINEN_ROBE,
            [xi.slot.HANDS] = xi.item.LINEN_CUFFS,
            [xi.slot.LEGS] = xi.item.LINEN_SLOPS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.MACE,
            [xi.slot.SUB] = xi.item.MAHOGANY_SHIELD,
            [xi.slot.HEAD] = xi.item.WOOL_HAT,
            [xi.slot.BODY] = xi.item.WOOL_ROBE,
            [xi.slot.HANDS] = xi.item.WOOL_CUFFS,
            [xi.slot.LEGS] = xi.item.WOOL_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_MACE,
            [xi.slot.SUB] = xi.item.OAK_SHIELD,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.HOLY_MACE,
            [xi.slot.SUB] = xi.item.OAK_SHIELD,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_MACE,
            [xi.slot.SUB] = xi.item.ROUND_SHIELD,
            [xi.slot.HEAD] = xi.item.SILK_HAT,
            [xi.slot.BODY] = xi.item.SILK_COAT,
            [xi.slot.HANDS] = 12732, -- silk_cuffs
            [xi.slot.LEGS] = xi.item.SILK_SLOPS,
            [xi.slot.FEET] = xi.item.SILK_PUMPS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_MACE,
            [xi.slot.SUB] = xi.item.ROUND_SHIELD,
            [xi.slot.HEAD] = 15260, -- hydra_beret
            [xi.slot.BODY] = xi.item.HYDRA_DOUBLET,
            [xi.slot.HANDS] = 14924, -- hydra_gloves
            [xi.slot.LEGS] = 15595, -- hydra_brais
            [xi.slot.FEET] = 15680, -- hydra_gaiters
        },
    },
    [xi.job.BLM] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.ASH_STAFF,
            [xi.slot.BODY] = xi.item.ROBE,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLOPS,
            [xi.slot.FEET] = xi.item.ASH_CLOGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.ASH_POLE,
            [xi.slot.BODY] = xi.item.TUNIC,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLACKS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.HOLLY_POLE,
            [xi.slot.HEAD] = xi.item.BRASS_HAIRPIN,
            [xi.slot.BODY] = xi.item.LINEN_ROBE,
            [xi.slot.HANDS] = xi.item.LINEN_CUFFS,
            [xi.slot.LEGS] = xi.item.LINEN_SLOPS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.ELM_POLE,
            [xi.slot.HEAD] = xi.item.WOOL_HAT,
            [xi.slot.BODY] = xi.item.WOOL_ROBE,
            [xi.slot.HANDS] = xi.item.WOOL_CUFFS,
            [xi.slot.LEGS] = xi.item.WOOL_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.OAK_POLE,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.MUSKETEERS_POLE,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.MAHOGANY_POLE,
            [xi.slot.HEAD] = xi.item.SILK_HAT,
            [xi.slot.BODY] = xi.item.SILK_COAT,
            [xi.slot.HANDS] = 12732, -- silk_cuffs
            [xi.slot.LEGS] = xi.item.SILK_SLOPS,
            [xi.slot.FEET] = xi.item.SILK_PUMPS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.MYTHIC_POLE,
            [xi.slot.HEAD] = 15260, -- hydra_beret
            [xi.slot.BODY] = xi.item.HYDRA_DOUBLET,
            [xi.slot.HANDS] = 14924, -- hydra_gloves
            [xi.slot.LEGS] = 15595, -- hydra_brais
            [xi.slot.FEET] = 15680, -- hydra_gaiters
        },
    },
    [xi.job.RDM] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_SWORD,
            [xi.slot.SUB] = xi.item.LAUAN_SHIELD,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.ROBE,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLOPS,
            [xi.slot.FEET] = xi.item.ASH_CLOGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.SPATHA,
            [xi.slot.SUB] = xi.item.MAPLE_SHIELD,
            [xi.slot.BODY] = xi.item.TUNIC,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLACKS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.LONGSWORD,
            [xi.slot.SUB] = xi.item.MAHOGANY_SHIELD,
            [xi.slot.HEAD] = xi.item.BRASS_HAIRPIN,
            [xi.slot.BODY] = xi.item.LINEN_ROBE,
            [xi.slot.HANDS] = xi.item.LINEN_CUFFS,
            [xi.slot.LEGS] = xi.item.LINEN_SLOPS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.BROADSWORD,
            [xi.slot.SUB] = xi.item.MAHOGANY_SHIELD,
            [xi.slot.HEAD] = xi.item.WOOL_HAT,
            [xi.slot.BODY] = xi.item.WOOL_ROBE,
            [xi.slot.HANDS] = xi.item.WOOL_CUFFS,
            [xi.slot.LEGS] = xi.item.WOOL_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_SWORD,
            [xi.slot.SUB] = xi.item.OAK_SHIELD,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.KNIGHTS_SWORD,
            [xi.slot.SUB] = xi.item.SENTINEL_SHIELD,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_SWORD,
            [xi.slot.SUB] = xi.item.ROUND_SHIELD,
            [xi.slot.HEAD] = xi.item.SILK_HAT,
            [xi.slot.BODY] = xi.item.SILK_COAT,
            [xi.slot.HANDS] = 12732, -- silk_cuffs
            [xi.slot.LEGS] = xi.item.SILK_SLOPS,
            [xi.slot.FEET] = xi.item.SILK_PUMPS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_SWORD,
            [xi.slot.SUB] = 12303, -- gold_buckler
            [xi.slot.HEAD] = 15260, -- hydra_beret
            [xi.slot.BODY] = xi.item.HYDRA_DOUBLET,
            [xi.slot.HANDS] = 14924, -- hydra_gloves
            [xi.slot.LEGS] = 15595, -- hydra_brais
            [xi.slot.FEET] = 15680, -- hydra_gaiters
        },
    },
    [xi.job.THF] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_DAGGER,
            [xi.slot.SUB] = xi.item.LAUAN_SHIELD,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BRASS_DAGGER,
            [xi.slot.SUB] = xi.item.MAPLE_SHIELD,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.LEATHER_VEST,
            [xi.slot.HANDS] = xi.item.LEATHER_GLOVES,
            [xi.slot.LEGS] = xi.item.LEATHER_TROUSERS,
            [xi.slot.FEET] = xi.item.LEATHER_HIGHBOOTS,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.KUKRI,
            [xi.slot.SUB] = xi.item.MAHOGANY_SHIELD,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.MERCENARY_CAPTAINS_KUKRI,
            [xi.slot.SUB] = xi.item.MAHOGANY_SHIELD,
            [xi.slot.HEAD] = xi.item.STUDDED_BANDANA,
            [xi.slot.BODY] = xi.item.STUDDED_VEST,
            [xi.slot.HANDS] = xi.item.STUDDED_GLOVES,
            [xi.slot.LEGS] = xi.item.STUDDED_TROUSERS,
            [xi.slot.FEET] = xi.item.STUDDED_BOOTS,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_KUKRI,
            [xi.slot.SUB] = xi.item.OAK_SHIELD,
            [xi.slot.HEAD] = xi.item.CUIR_BANDANA,
            [xi.slot.BODY] = xi.item.CUIR_BOUILLI,
            [xi.slot.HANDS] = xi.item.CUIR_GLOVES,
            [xi.slot.LEGS] = xi.item.CUIR_TROUSERS,
            [xi.slot.FEET] = xi.item.CUIR_HIGHBOOTS,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.BONE_KNIFE,
            [xi.slot.SUB] = xi.item.OAK_SHIELD,
            [xi.slot.HEAD] = xi.item.RAPTOR_HELM,
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_KNIFE,
            [xi.slot.SUB] = xi.item.ROUND_SHIELD,
            [xi.slot.HEAD] = 13878, -- carapace_helm
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.MISERICORDE,
            [xi.slot.SUB] = xi.item.ROUND_SHIELD,
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
    [xi.job.PLD] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_SWORD,
            [xi.slot.SUB] = xi.item.LAUAN_SHIELD,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.SPATHA,
            [xi.slot.SUB] = xi.item.MAPLE_SHIELD,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.SCALE_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.SCALE_CUISSES,
            [xi.slot.FEET] = xi.item.SCALE_GREAVES,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.LONGSWORD,
            [xi.slot.SUB] = xi.item.MAHOGANY_SHIELD,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.BROADSWORD,
            [xi.slot.SUB] = xi.item.KITE_SHIELD,
            [xi.slot.HEAD] = xi.item.BRASS_MASK,
            [xi.slot.BODY] = xi.item.BRASS_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.BRASS_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.BRASS_CUISSES,
            [xi.slot.FEET] = xi.item.BRASS_GREAVES,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_SWORD,
            [xi.slot.SUB] = xi.item.OAK_SHIELD,
            [xi.slot.HEAD] = xi.item.IRON_VISOR,
            [xi.slot.BODY] = xi.item.IRON_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.IRON_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.IRON_CUISSES,
            [xi.slot.FEET] = xi.item.IRON_GREAVES,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.KNIGHTS_SWORD,
            [xi.slot.SUB] = xi.item.SENTINEL_SHIELD,
            [xi.slot.HEAD] = xi.item.STEEL_VISOR,
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.STEEL_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_SWORD,
            [xi.slot.SUB] = xi.item.ROUND_SHIELD,
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.DARKSTEEL_MUFFLERS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_SWORD,
            [xi.slot.SUB] = xi.item.TOWER_SHIELD,
            [xi.slot.HEAD] = 15262, -- hydra_salade
            [xi.slot.BODY] = xi.item.HYDRA_HAUBERT,
            [xi.slot.HANDS] = 14926, -- hydra_moufles
            [xi.slot.LEGS] = 15597, -- hydra_brayettes
            [xi.slot.FEET] = 15682, -- hydra_sollerets
        },
    },
    [xi.job.DRK] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_ZAGHNAL,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BRASS_ZAGHNAL,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.SCALE_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.SCALE_CUISSES,
            [xi.slot.FEET] = xi.item.SCALE_GREAVES,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.ZAGHNAL,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.SCYTHE,
            [xi.slot.HEAD] = xi.item.BRASS_MASK,
            [xi.slot.BODY] = xi.item.BRASS_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.BRASS_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.BRASS_CUISSES,
            [xi.slot.FEET] = xi.item.BRASS_GREAVES,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_SCYTHE,
            [xi.slot.HEAD] = xi.item.IRON_VISOR,
            [xi.slot.BODY] = xi.item.IRON_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.IRON_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.IRON_CUISSES,
            [xi.slot.FEET] = xi.item.IRON_GREAVES,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.FALCASTRA,
            [xi.slot.HEAD] = xi.item.STEEL_VISOR,
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.STEEL_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.BONE_SCYTHE,
            [xi.slot.HEAD] = 13878, -- carapace_helm
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.DARKSTEEL_MUFFLERS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.BONE_SCYTHE,
            [xi.slot.HEAD] = 15262, -- hydra_salade
            [xi.slot.BODY] = xi.item.HYDRA_HAUBERT,
            [xi.slot.HANDS] = 14926, -- hydra_moufles
            [xi.slot.LEGS] = 15597, -- hydra_brayettes
            [xi.slot.FEET] = 15682, -- hydra_sollerets
        },
    },
    [xi.job.BST] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_AXE,
            [xi.slot.SUB] = xi.item.LAUAN_SHIELD,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BRASS_AXE,
            [xi.slot.SUB] = xi.item.MAPLE_SHIELD,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.SCALE_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.SCALE_CUISSES,
            [xi.slot.FEET] = xi.item.SCALE_GREAVES,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.BATTLEAXE,
            [xi.slot.SUB] = xi.item.MAHOGANY_SHIELD,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.MILITARY_PICK,
            [xi.slot.SUB] = xi.item.MAHOGANY_SHIELD,
            [xi.slot.HEAD] = xi.item.BRASS_MASK,
            [xi.slot.BODY] = xi.item.BRASS_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.BRASS_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.BRASS_CUISSES,
            [xi.slot.FEET] = xi.item.BRASS_GREAVES,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_AXE,
            [xi.slot.SUB] = xi.item.OAK_SHIELD,
            [xi.slot.HEAD] = xi.item.IRON_VISOR,
            [xi.slot.BODY] = xi.item.IRON_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.IRON_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.IRON_CUISSES,
            [xi.slot.FEET] = xi.item.IRON_GREAVES,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.TABAR,
            [xi.slot.SUB] = xi.item.SENTINEL_SHIELD,
            [xi.slot.HEAD] = xi.item.STEEL_VISOR,
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.STEEL_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_AXE,
            [xi.slot.SUB] = xi.item.ROUND_SHIELD,
            [xi.slot.HEAD] = 13878, -- carapace_helm
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.DARKSTEEL_MUFFLERS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_TABAR,
            [xi.slot.SUB] = 12303, -- gold_buckler
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
    [xi.job.BRD] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_DAGGER,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.ROBE,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLOPS,
            [xi.slot.FEET] = xi.item.ASH_CLOGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BRASS_DAGGER,
            [xi.slot.BODY] = xi.item.TUNIC,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLACKS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.MERCENARYS_KNIFE,
            [xi.slot.HEAD] = xi.item.BRASS_HAIRPIN,
            [xi.slot.BODY] = xi.item.LINEN_ROBE,
            [xi.slot.HANDS] = xi.item.LINEN_CUFFS,
            [xi.slot.LEGS] = xi.item.LINEN_SLOPS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.ROYAL_SQUIRES_DAGGER,
            [xi.slot.HEAD] = xi.item.WOOL_HAT,
            [xi.slot.BODY] = xi.item.WOOL_ROBE,
            [xi.slot.HANDS] = xi.item.WOOL_CUFFS,
            [xi.slot.LEGS] = xi.item.WOOL_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MELT_BASELARD,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.BONE_KNIFE,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_KNIFE,
            [xi.slot.HEAD] = xi.item.SILK_HAT,
            [xi.slot.BODY] = xi.item.SILK_COAT,
            [xi.slot.HANDS] = 12732, -- silk_cuffs
            [xi.slot.LEGS] = xi.item.SILK_SLOPS,
            [xi.slot.FEET] = xi.item.SILK_PUMPS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.MISERICORDE,
            [xi.slot.HEAD] = 15260, -- hydra_beret
            [xi.slot.BODY] = xi.item.HYDRA_DOUBLET,
            [xi.slot.HANDS] = 14924, -- hydra_gloves
            [xi.slot.LEGS] = 15595, -- hydra_brais
            [xi.slot.FEET] = 15680, -- hydra_gaiters
        },
    },
    [xi.job.RNG] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_AXE,
            [xi.slot.RANGED] = xi.item.SHORTBOW,
            [xi.slot.BODY] = xi.item.ROBE,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLOPS,
            [xi.slot.FEET] = xi.item.ASH_CLOGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BRASS_AXE,
            [xi.slot.RANGED] = xi.item.SELF_BOW,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.LEATHER_VEST,
            [xi.slot.HANDS] = xi.item.LEATHER_GLOVES,
            [xi.slot.LEGS] = xi.item.LEATHER_TROUSERS,
            [xi.slot.FEET] = xi.item.LEATHER_HIGHBOOTS,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.BATTLEAXE,
            [xi.slot.RANGED] = xi.item.POWER_BOW,
            [xi.slot.HEAD] = xi.item.BRASS_HAIRPIN,
            [xi.slot.BODY] = xi.item.LINEN_ROBE,
            [xi.slot.HANDS] = xi.item.LINEN_CUFFS,
            [xi.slot.LEGS] = xi.item.LINEN_SLOPS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.MILITARY_PICK,
            [xi.slot.RANGED] = xi.item.GREAT_BOW,
            [xi.slot.HEAD] = xi.item.STUDDED_BANDANA,
            [xi.slot.BODY] = xi.item.STUDDED_VEST,
            [xi.slot.HANDS] = xi.item.STUDDED_GLOVES,
            [xi.slot.LEGS] = xi.item.STUDDED_TROUSERS,
            [xi.slot.FEET] = xi.item.STUDDED_BOOTS,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_AXE,
            [xi.slot.RANGED] = xi.item.BATTLE_BOW,
            [xi.slot.HEAD] = xi.item.CUIR_BANDANA,
            [xi.slot.BODY] = xi.item.CUIR_BOUILLI,
            [xi.slot.HANDS] = xi.item.CUIR_GLOVES,
            [xi.slot.LEGS] = xi.item.CUIR_TROUSERS,
            [xi.slot.FEET] = xi.item.CUIR_HIGHBOOTS,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.COMBAT_CASTERS_AXE,
            [xi.slot.RANGED] = xi.item.WAR_BOW,
            [xi.slot.HEAD] = xi.item.RAPTOR_HELM,
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_AXE,
            [xi.slot.RANGED] = xi.item.WAR_BOW,
            [xi.slot.HEAD] = 13878, -- carapace_helm
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.RUNE_AXE,
            [xi.slot.RANGED] = xi.item.WAR_BOW,
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
    [xi.job.SAM] =
    {
        [1] =
        {
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.TACHI,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.SCALE_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.SCALE_CUISSES,
            [xi.slot.FEET] = xi.item.SCALE_GREAVES,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.NODACHI,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.KOTETSU,
            [xi.slot.HEAD] = xi.item.BRASS_MASK,
            [xi.slot.BODY] = xi.item.BRASS_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.BRASS_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.BRASS_CUISSES,
            [xi.slot.FEET] = xi.item.BRASS_GREAVES,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.HOSODACHI,
            [xi.slot.HEAD] = xi.item.IRON_VISOR,
            [xi.slot.BODY] = xi.item.IRON_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.IRON_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.IRON_CUISSES,
            [xi.slot.FEET] = xi.item.IRON_GREAVES,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.ODENTA,
            [xi.slot.HEAD] = xi.item.STEEL_VISOR,
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.STEEL_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.JINDACHI,
            [xi.slot.HEAD] = 13878, -- carapace_helm
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.DARKSTEEL_MUFFLERS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.KAZARIDACHI,
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
    [xi.job.NIN] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.KUNAI,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.WAKIZASHI,
            [xi.slot.SUB] = xi.item.WAKIZASHI,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.LEATHER_VEST,
            [xi.slot.HANDS] = xi.item.LEATHER_GLOVES,
            [xi.slot.LEGS] = xi.item.LEATHER_TROUSERS,
            [xi.slot.FEET] = xi.item.LEATHER_HIGHBOOTS,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.SHINOBI_GATANA,
            [xi.slot.SUB] = xi.item.SHINOBI_GATANA,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.FUKURO,
            [xi.slot.SUB] = xi.item.FUKURO,
            [xi.slot.HEAD] = xi.item.STUDDED_BANDANA,
            [xi.slot.BODY] = xi.item.STUDDED_VEST,
            [xi.slot.HANDS] = xi.item.STUDDED_GLOVES,
            [xi.slot.LEGS] = xi.item.STUDDED_TROUSERS,
            [xi.slot.FEET] = xi.item.STUDDED_BOOTS,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.KODACHI,
            [xi.slot.SUB] = xi.item.KODACHI,
            [xi.slot.HEAD] = xi.item.CUIR_BANDANA,
            [xi.slot.BODY] = xi.item.CUIR_BOUILLI,
            [xi.slot.HANDS] = xi.item.CUIR_GLOVES,
            [xi.slot.LEGS] = xi.item.CUIR_TROUSERS,
            [xi.slot.FEET] = xi.item.CUIR_HIGHBOOTS,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.SAKURAFUBUKI,
            [xi.slot.SUB] = xi.item.SAKURAFUBUKI,
            [xi.slot.HEAD] = xi.item.RAPTOR_HELM,
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.SHINOGI,
            [xi.slot.SUB] = xi.item.SHINOGI,
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.KABUTOWARI,
            [xi.slot.SUB] = xi.item.KABUTOWARI,
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
    [xi.job.DRG] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.HARPOON,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_SPEAR,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.SCALE_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.SCALE_CUISSES,
            [xi.slot.FEET] = xi.item.SCALE_GREAVES,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.FUSCINA,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.SPEAR,
            [xi.slot.HEAD] = xi.item.BRASS_MASK,
            [xi.slot.BODY] = xi.item.BRASS_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.BRASS_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.BRASS_CUISSES,
            [xi.slot.FEET] = xi.item.BRASS_GREAVES,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.HALBERD,
            [xi.slot.HEAD] = xi.item.IRON_VISOR,
            [xi.slot.BODY] = xi.item.IRON_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.IRON_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.IRON_CUISSES,
            [xi.slot.FEET] = xi.item.IRON_GREAVES,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.HEAVY_HALBERD,
            [xi.slot.HEAD] = xi.item.STEEL_VISOR,
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.STEEL_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_LANCE,
            [xi.slot.HEAD] = 13878, -- carapace_helm
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_LANCE,
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
    [xi.job.SMN] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.ASH_STAFF,
            [xi.slot.BODY] = xi.item.ROBE,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLOPS,
            [xi.slot.FEET] = xi.item.ASH_CLOGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.ASH_POLE,
            [xi.slot.BODY] = xi.item.TUNIC,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLACKS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.HOLLY_POLE,
            [xi.slot.HEAD] = xi.item.BRASS_HAIRPIN,
            [xi.slot.BODY] = xi.item.LINEN_ROBE,
            [xi.slot.HANDS] = xi.item.LINEN_CUFFS,
            [xi.slot.LEGS] = xi.item.LINEN_SLOPS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.ELM_POLE,
            [xi.slot.HEAD] = xi.item.WOOL_HAT,
            [xi.slot.BODY] = xi.item.WOOL_ROBE,
            [xi.slot.HANDS] = xi.item.WOOL_CUFFS,
            [xi.slot.LEGS] = xi.item.WOOL_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.OAK_POLE,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.MUSKETEERS_POLE,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.MAHOGANY_POLE,
            [xi.slot.HEAD] = xi.item.SILK_HAT,
            [xi.slot.BODY] = xi.item.SILK_COAT,
            [xi.slot.HANDS] = 12732, -- silk_cuffs
            [xi.slot.LEGS] = xi.item.SILK_SLOPS,
            [xi.slot.FEET] = xi.item.SILK_PUMPS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.MYTHIC_POLE,
            [xi.slot.HEAD] = 15260, -- hydra_beret
            [xi.slot.BODY] = xi.item.HYDRA_DOUBLET,
            [xi.slot.HANDS] = 14924, -- hydra_gloves
            [xi.slot.LEGS] = 15595, -- hydra_brais
            [xi.slot.FEET] = 15680, -- hydra_gaiters
        },
    },
    [xi.job.BLU] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_SWORD,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.ROYAL_ARCHERS_SWORD,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.LEATHER_VEST,
            [xi.slot.HANDS] = xi.item.LEATHER_GLOVES,
            [xi.slot.LEGS] = xi.item.LEATHER_TROUSERS,
            [xi.slot.FEET] = xi.item.LEATHER_HIGHBOOTS,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.FIRE_SWORD,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.BROADSWORD,
            [xi.slot.HEAD] = xi.item.STUDDED_BANDANA,
            [xi.slot.BODY] = xi.item.STUDDED_VEST,
            [xi.slot.HANDS] = xi.item.STUDDED_GLOVES,
            [xi.slot.LEGS] = xi.item.STUDDED_TROUSERS,
            [xi.slot.FEET] = xi.item.STUDDED_BOOTS,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_SWORD,
            [xi.slot.HEAD] = xi.item.CUIR_BANDANA,
            [xi.slot.BODY] = xi.item.CUIR_BOUILLI,
            [xi.slot.HANDS] = xi.item.CUIR_GLOVES,
            [xi.slot.LEGS] = xi.item.CUIR_TROUSERS,
            [xi.slot.FEET] = xi.item.CUIR_HIGHBOOTS,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.FLAME_BLADE,
            [xi.slot.HEAD] = xi.item.RAPTOR_HELM,
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_SWORD,
            [xi.slot.HEAD] = xi.item.SILK_HAT,
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_SWORD,
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
    [xi.job.COR] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.ONION_SWORD,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.ROYAL_ARCHERS_SWORD,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.LEATHER_VEST,
            [xi.slot.HANDS] = xi.item.LEATHER_GLOVES,
            [xi.slot.LEGS] = xi.item.LEATHER_TROUSERS,
            [xi.slot.FEET] = xi.item.LEATHER_HIGHBOOTS,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.DEGEN,
            [xi.slot.RANGED] = xi.item.PLATOON_GUN,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.BROADSWORD,
            [xi.slot.RANGED] = xi.item.PIRATES_GUN,
            [xi.slot.HEAD] = xi.item.STUDDED_BANDANA,
            [xi.slot.BODY] = xi.item.STUDDED_VEST,
            [xi.slot.HANDS] = xi.item.STUDDED_GLOVES,
            [xi.slot.LEGS] = xi.item.STUDDED_TROUSERS,
            [xi.slot.FEET] = xi.item.STUDDED_BOOTS,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.HALLOWED_SWORD,
            [xi.slot.RANGED] = xi.item.MILITARY_GUN,
            [xi.slot.HEAD] = xi.item.CUIR_BANDANA,
            [xi.slot.BODY] = xi.item.CUIR_BOUILLI,
            [xi.slot.HANDS] = xi.item.CUIR_GLOVES,
            [xi.slot.LEGS] = xi.item.CUIR_TROUSERS,
            [xi.slot.FEET] = xi.item.CUIR_HIGHBOOTS,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.MUSKETEERS_SWORD,
            [xi.slot.RANGED] = xi.item.MILITARY_GUN,
            [xi.slot.HEAD] = xi.item.RAPTOR_HELM,
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.WISE_WIZARDS_ANELACE,
            [xi.slot.RANGED] = xi.item.MILITARY_GUN,
            [xi.slot.HEAD] = xi.item.SILK_HAT,
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.RUNE_BLADE,
            [xi.slot.RANGED] = xi.item.MILITARY_GUN,
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
    [xi.job.PUP] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.CAT_BAGHNAKHS,
            [xi.slot.BODY] = xi.item.ROBE,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLOPS,
            [xi.slot.FEET] = xi.item.ASH_CLOGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.FREESWORDS_BAGHNAKHS,
            [xi.slot.BODY] = xi.item.TUNIC,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLACKS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.COUGAR_BAGHNAKHS,
            [xi.slot.HEAD] = xi.item.BRASS_HAIRPIN,
            [xi.slot.BODY] = xi.item.LINEN_ROBE,
            [xi.slot.HANDS] = xi.item.LINEN_CUFFS,
            [xi.slot.LEGS] = xi.item.LINEN_SLOPS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.CLAWS,
            [xi.slot.HEAD] = xi.item.WOOL_HAT,
            [xi.slot.BODY] = xi.item.WOOL_ROBE,
            [xi.slot.HANDS] = xi.item.WOOL_CUFFS,
            [xi.slot.LEGS] = xi.item.WOOL_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.KATARS,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.PATAS,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.JAMADHARS,
            [xi.slot.HEAD] = xi.item.SILK_HAT,
            [xi.slot.BODY] = xi.item.SILK_COAT,
            [xi.slot.HANDS] = 12732, -- silk_cuffs
            [xi.slot.LEGS] = xi.item.SILK_SLOPS,
            [xi.slot.FEET] = xi.item.SILK_PUMPS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.TIGERFANGS,
        },
    },
    [xi.job.DNC] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_KNIFE,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_KNIFE,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.LEATHER_VEST,
            [xi.slot.HANDS] = xi.item.LEATHER_GLOVES,
            [xi.slot.LEGS] = xi.item.LEATHER_TROUSERS,
            [xi.slot.FEET] = xi.item.LEATHER_HIGHBOOTS,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.KUKRI,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.MERCENARY_CAPTAINS_KUKRI,
            [xi.slot.HEAD] = xi.item.STUDDED_BANDANA,
            [xi.slot.BODY] = xi.item.STUDDED_VEST,
            [xi.slot.HANDS] = xi.item.STUDDED_GLOVES,
            [xi.slot.LEGS] = xi.item.STUDDED_TROUSERS,
            [xi.slot.FEET] = xi.item.STUDDED_BOOTS,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_KUKRI,
            [xi.slot.HEAD] = xi.item.CUIR_BANDANA,
            [xi.slot.BODY] = xi.item.CUIR_BOUILLI,
            [xi.slot.HANDS] = xi.item.CUIR_GLOVES,
            [xi.slot.LEGS] = xi.item.CUIR_TROUSERS,
            [xi.slot.FEET] = xi.item.CUIR_HIGHBOOTS,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.BONE_KNIFE,
            [xi.slot.HEAD] = xi.item.RAPTOR_HELM,
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_KNIFE,
            [xi.slot.HEAD] = 13878, -- carapace_helm
            [xi.slot.BODY] = xi.item.RAPTOR_JERKIN,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.RAPTOR_TROUSERS,
            [xi.slot.FEET] = xi.item.RAPTOR_LEDELSENS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.BEETLE_KNIFE,
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
    [xi.job.SCH] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.ASH_STAFF,
            [xi.slot.BODY] = xi.item.ROBE,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLOPS,
            [xi.slot.FEET] = xi.item.ASH_CLOGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.ASH_POLE,
            [xi.slot.BODY] = xi.item.TUNIC,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLACKS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.HOLLY_POLE,
            [xi.slot.HEAD] = xi.item.BRASS_HAIRPIN,
            [xi.slot.BODY] = xi.item.LINEN_ROBE,
            [xi.slot.HANDS] = xi.item.LINEN_CUFFS,
            [xi.slot.LEGS] = xi.item.LINEN_SLOPS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.ELM_POLE,
            [xi.slot.HEAD] = xi.item.WOOL_HAT,
            [xi.slot.BODY] = xi.item.WOOL_ROBE,
            [xi.slot.HANDS] = xi.item.WOOL_CUFFS,
            [xi.slot.LEGS] = xi.item.WOOL_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.OAK_POLE,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.MUSKETEERS_POLE,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.MAHOGANY_POLE,
            [xi.slot.HEAD] = xi.item.SILK_HAT,
            [xi.slot.BODY] = xi.item.SILK_COAT,
            [xi.slot.HANDS] = 12732, -- silk_cuffs
            [xi.slot.LEGS] = xi.item.SILK_SLOPS,
            [xi.slot.FEET] = xi.item.SILK_PUMPS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.MYTHIC_POLE,
            [xi.slot.HEAD] = 15260, -- hydra_beret
            [xi.slot.BODY] = xi.item.HYDRA_DOUBLET,
            [xi.slot.HANDS] = 14924, -- hydra_gloves
            [xi.slot.LEGS] = 15595, -- hydra_brais
            [xi.slot.FEET] = 15680, -- hydra_gaiters
        },
    },
    [xi.job.GEO] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.ASH_CLUB,
            [xi.slot.BODY] = xi.item.ROBE,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLOPS,
            [xi.slot.FEET] = xi.item.ASH_CLOGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.BRONZE_HAMMER,
            [xi.slot.BODY] = xi.item.TUNIC,
            [xi.slot.HANDS] = xi.item.CUFFS,
            [xi.slot.LEGS] = xi.item.SLACKS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.MACE,
            [xi.slot.HEAD] = xi.item.BRASS_HAIRPIN,
            [xi.slot.BODY] = xi.item.LINEN_ROBE,
            [xi.slot.HANDS] = xi.item.LINEN_CUFFS,
            [xi.slot.LEGS] = xi.item.LINEN_SLOPS,
            [xi.slot.FEET] = xi.item.SOLEA,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.MACE,
            [xi.slot.HEAD] = xi.item.WOOL_HAT,
            [xi.slot.BODY] = xi.item.WOOL_ROBE,
            [xi.slot.HANDS] = xi.item.WOOL_CUFFS,
            [xi.slot.LEGS] = xi.item.WOOL_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_MACE,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.HOLY_MACE,
            [xi.slot.HEAD] = xi.item.VELVET_HAT,
            [xi.slot.BODY] = xi.item.VELVET_ROBE,
            [xi.slot.HANDS] = xi.item.VELVET_CUFFS,
            [xi.slot.LEGS] = xi.item.VELVET_SLOPS,
            [xi.slot.FEET] = 14150, -- kingdom_clogs
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_MACE,
            [xi.slot.HEAD] = xi.item.SILK_HAT,
            [xi.slot.BODY] = xi.item.SILK_COAT,
            [xi.slot.HANDS] = 12732, -- silk_cuffs
            [xi.slot.LEGS] = xi.item.SILK_SLOPS,
            [xi.slot.FEET] = xi.item.SILK_PUMPS,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_MACE,
            [xi.slot.HEAD] = 15260, -- hydra_beret
            [xi.slot.BODY] = xi.item.HYDRA_DOUBLET,
            [xi.slot.HANDS] = 14924, -- hydra_gloves
            [xi.slot.LEGS] = 15595, -- hydra_brais
            [xi.slot.FEET] = 15680, -- hydra_gaiters
        },
    },
    [xi.job.RUN] =
    {
        [1] =
        {
            [xi.slot.MAIN] = xi.item.RUSTY_GREATSWORD,
            [xi.slot.HEAD] = xi.item.BRONZE_CAP,
            [xi.slot.BODY] = xi.item.BRONZE_HARNESS,
            [xi.slot.HANDS] = xi.item.BRONZE_MITTENS,
            [xi.slot.LEGS] = xi.item.BRONZE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BRONZE_LEGGINGS,
        },
        [10] =
        {
            [xi.slot.MAIN] = xi.item.CLAYMORE,
            [xi.slot.HEAD] = xi.item.LEATHER_BANDANA,
            [xi.slot.BODY] = xi.item.SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.SCALE_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.SCALE_CUISSES,
            [xi.slot.FEET] = xi.item.SCALE_GREAVES,
        },
        [20] =
        {
            [xi.slot.MAIN] = xi.item.TWO_HANDED_SWORD,
            [xi.slot.HEAD] = xi.item.BONE_MASK,
            [xi.slot.BODY] = xi.item.BONE_HARNESS,
            [xi.slot.HANDS] = xi.item.BONE_MITTENS,
            [xi.slot.LEGS] = xi.item.BONE_SUBLIGAR,
            [xi.slot.FEET] = xi.item.BONE_LEGGINGS,
        },
        [30] =
        {
            [xi.slot.MAIN] = xi.item.TWO_HANDED_SWORD,
            [xi.slot.HEAD] = xi.item.BRASS_MASK,
            [xi.slot.BODY] = xi.item.BRASS_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.BRASS_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.BRASS_CUISSES,
            [xi.slot.FEET] = xi.item.BRASS_GREAVES,
        },
        [40] =
        {
            [xi.slot.MAIN] = xi.item.MYTHRIL_CLAYMORE,
            [xi.slot.HEAD] = xi.item.IRON_VISOR,
            [xi.slot.BODY] = xi.item.IRON_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.IRON_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.IRON_CUISSES,
            [xi.slot.FEET] = xi.item.IRON_GREAVES,
        },
        [51] =
        {
            [xi.slot.MAIN] = xi.item.GREATSWORD,
            [xi.slot.HEAD] = xi.item.STEEL_VISOR,
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.STEEL_FINGER_GAUNTLETS,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [61] =
        {
            [xi.slot.MAIN] = xi.item.DARKSTEEL_CLAYMORE,
            [xi.slot.HEAD] = 13878, -- carapace_helm
            [xi.slot.BODY] = xi.item.STEEL_SCALE_MAIL,
            [xi.slot.HANDS] = xi.item.RAPTOR_GLOVES,
            [xi.slot.LEGS] = xi.item.STEEL_CUISSES,
            [xi.slot.FEET] = xi.item.STEEL_GREAVES,
        },
        [71] =
        {
            [xi.slot.MAIN] = xi.item.CRUDE_SWORD,
            [xi.slot.HEAD] = xi.item.HYDRA_TIARA,
            [xi.slot.BODY] = xi.item.HYDRA_HARNESS,
            [xi.slot.HANDS] = xi.item.HYDRA_MITTENS,
            [xi.slot.LEGS] = xi.item.HYDRA_TIGHTS,
            [xi.slot.FEET] = xi.item.HYDRA_SPATS,
        },
    },
}
