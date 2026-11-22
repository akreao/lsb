-- Player bots on the map process, for /sea (custom, not retail). The map server rewrites it every few seconds; the
-- search server lists its rows next to real players. In memory only: a restart empties it.
DROP TABLE IF EXISTS `player_bots_search`;
CREATE TABLE `player_bots_search` (
  `charid` int(10) unsigned NOT NULL,
  `charname` varchar(15) NOT NULL,
  `partyid` int(10) unsigned NOT NULL DEFAULT '0', -- the party leader's charid, 0 when on its own
  `pos_zone` smallint(5) unsigned NOT NULL DEFAULT '0',
  `nation` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `rank` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `mjob` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `sjob` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `mlvl` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `slvl` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `settings` int(10) unsigned NOT NULL DEFAULT '0', -- as chars.settings (the seeking party flag, anonymity)
  PRIMARY KEY (`charid`)
) ENGINE=MEMORY DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
