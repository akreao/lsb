DROP TABLE IF EXISTS `char_trust_ascension`;
CREATE TABLE `char_trust_ascension` (
  `charid` int(10) unsigned NOT NULL,
  `alter_ego` blob DEFAULT NULL, -- Upgrade level per attribute, indexed as in s2c 0x08E
  `celestite` blob DEFAULT NULL, -- Registered celestite bits and the six sets, laid out as in s2c 0x08F
  PRIMARY KEY (`charid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
