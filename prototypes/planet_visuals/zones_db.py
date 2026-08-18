"""Zone database — educational metadata for every labeled feature on
the moon. Loaded by the game's UI to populate the zone-info panel
when the player hovers/clicks a labeled feature in the orbital view.

Data sources (all public domain / freely available):
  * IAU Planetary Nomenclature gazetteer
  * NASA Planetary Fact Sheet — Moon, mission archives
  * USGS Astrogeology unified maps
  * LROC LROC WAC mosaic + LOLA elevation derivatives
  * Various peer-reviewed lunar geology summaries

Numbers are best-effort accuracy; treat as "good enough for an
edutainment game" — for actual research, always go to primary
sources.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, asdict, field


@dataclass
class ZoneInfo:
    # Identity
    name: str
    feature_type: str       # "mare" | "crater" | "landing" | "basin"
    lat: float              # degrees, north positive
    lon: float              # degrees, east positive
    diameter_km: float      # mare/basin/crater diameter; ~1 for landing sites

    # Terrain
    elevation_floor_km: float = 0.0   # relative to mean lunar radius
    elevation_rim_km: float = 0.0
    age_ga: float = 0.0               # billion years (Ga)
    terrain: str = ""                  # 1-2 sentence description

    # Composition / regolith
    dominant_rock: str = ""           # "basalt" | "anorthosite" | "breccia" | etc.
    iron_pct: float | None = None
    titanium_pct: float | None = None
    thorium_ppm: float | None = None
    composition_notes: str = ""

    # Lighting / visibility
    earth_visible: bool = True        # near side?
    permanently_shadowed: bool = False  # polar PSR
    max_sun_altitude_deg: float = 0.0
    lighting_notes: str = ""

    # History / significance
    formed_by: str = ""
    formation_notes: str = ""
    significance: str = ""
    missions: list[str] = field(default_factory=list)
    named_after: str = ""


# === Helper for compact authoring =========================================

def _Z(**kw):
    return ZoneInfo(**kw)


# ===========================================================================
# Major maria — basaltic plains, mostly Imbrian-age (~3.5 Ga)
# ===========================================================================

ZONES_MARE = [
    _Z(name="Mare Imbrium",
       feature_type="mare", lat=32.8, lon=-15.6, diameter_km=1146,
       elevation_floor_km=-2.5, elevation_rim_km=-1.0,
       age_ga=3.5,
       terrain="Vast circular basaltic plain inside a 1145 km impact basin. "
               "Floor is mostly flat lava flows with subdued wrinkle ridges and "
               "a few large embedded craters (Archimedes, Aristillus, Autolycus).",
       dominant_rock="basalt",
       iron_pct=14.0, titanium_pct=2.5, thorium_ppm=8.0,
       composition_notes="Iron- and titanium-rich basalt with an unusually high "
                         "thorium signal in the western part — overlaps with the "
                         "Procellarum KREEP Terrane.",
       earth_visible=True, max_sun_altitude_deg=57,
       lighting_notes="Earth visible from anywhere on the floor. Sun reaches "
                      "high in the lunar sky; surface gets very hot during day.",
       formed_by="impact + basaltic flooding",
       formation_notes="Imbrium impact ~3.85 Ga excavated the basin; basalt "
                       "flooded the floor over the next ~500 My.",
       significance="One of the largest, youngest, and most-studied lunar basins. "
                    "Its ejecta defines the Imbrian stratigraphic system.",
       named_after="Latin: Sea of Showers."),

    _Z(name="Mare Serenitatis",
       feature_type="mare", lat=28.0, lon=17.5, diameter_km=707,
       elevation_floor_km=-2.7, elevation_rim_km=-1.0,
       age_ga=3.8,
       terrain="Roughly circular basaltic plain with concentric mascon "
               "wrinkle ridges. Floor surface is darker than surrounding "
               "highlands; centre has a distinct dark mantling deposit.",
       dominant_rock="basalt",
       iron_pct=15.0, titanium_pct=4.5,
       composition_notes="Higher-titanium basalt than Mare Imbrium. Apollo 17 "
                         "samples include orange-glass beads from pyroclastic "
                         "fountaining.",
       earth_visible=True, max_sun_altitude_deg=62,
       formed_by="impact + basaltic flooding",
       formation_notes="Pre-Nectarian impact basin, flooded by basalt later.",
       significance="Apollo 17 (Taurus-Littrow valley) sampled the southeast "
                    "rim. Returned the youngest mare basalts and the famous "
                    "orange soil.",
       missions=["Apollo 17", "Luna 21"],
       named_after="Latin: Sea of Serenity."),

    _Z(name="Mare Tranquillitatis",
       feature_type="mare", lat=8.5, lon=31.4, diameter_km=873,
       elevation_floor_km=-2.4, elevation_rim_km=-1.0,
       age_ga=3.7,
       terrain="Irregular flat basaltic plain with no clear basin shape. "
               "Surface peppered with small fresh craters and crossed by "
               "low wrinkle ridges.",
       dominant_rock="basalt",
       iron_pct=15.5, titanium_pct=8.0,
       composition_notes="Some of the highest-titanium basalts on the Moon. "
                         "Apollo 11 returned ilmenite-rich samples.",
       earth_visible=True, max_sun_altitude_deg=82,
       lighting_notes="Equatorial — sun passes nearly overhead at noon.",
       formed_by="basaltic flooding",
       formation_notes="No clean basin; basalt fills an irregular older "
                       "depression along several Imbrian-age rims.",
       significance="Apollo 11 (Tranquility Base) — first crewed lunar landing, "
                    "20 July 1969. Surveyor 5 robotic precursor in 1967.",
       missions=["Apollo 11", "Surveyor 5"],
       named_after="Latin: Sea of Tranquillity."),

    _Z(name="Mare Crisium",
       feature_type="mare", lat=17.0, lon=59.1, diameter_km=556,
       elevation_floor_km=-3.7, elevation_rim_km=-0.5,
       age_ga=3.9,
       terrain="Oval basaltic plain inside a clean 556 km impact basin near "
               "the eastern limb. Mascon causes pronounced gravity high.",
       dominant_rock="basalt",
       iron_pct=12.0, titanium_pct=2.0,
       composition_notes="Lower-Ti basalt. Luna 24 returned regolith samples.",
       earth_visible=True, max_sun_altitude_deg=73,
       lighting_notes="Near the limb; Earth low on horizon, libration changes "
                      "visibility appreciably.",
       formed_by="impact + basaltic flooding",
       significance="Isolated, well-defined basin makes it a textbook example. "
                    "Luna 24 (1976) sampled the southwest floor — the last "
                    "Soviet sample-return mission.",
       missions=["Luna 15", "Luna 24"],
       named_after="Latin: Sea of Crises."),

    _Z(name="Mare Frigoris",
       feature_type="mare", lat=55.0, lon=0.0, diameter_km=1446,
       elevation_floor_km=-1.0, elevation_rim_km=+1.0,
       age_ga=3.5,
       terrain="Long, narrow, irregular basaltic plain stretching ~1500 km "
               "east–west across the high northern latitudes. Not basin-shaped.",
       dominant_rock="basalt",
       iron_pct=12.0,
       earth_visible=True, max_sun_altitude_deg=35,
       lighting_notes="High-latitude — sun stays low, shadows are long.",
       formed_by="basaltic flooding",
       significance="One of the few non-basin maria. Borders Mare Imbrium to "
                    "the south. Northern shore approaches the polar zone.",
       named_after="Latin: Sea of Cold."),

    _Z(name="Oceanus Procellarum",
       feature_type="mare", lat=18.4, lon=-57.4, diameter_km=2592,
       elevation_floor_km=-2.5, elevation_rim_km=-0.5,
       age_ga=3.6,
       terrain="The single largest mare. Sprawling basaltic plain covering "
               "most of the western near side. No clean basin shape.",
       dominant_rock="basalt",
       iron_pct=14.0, titanium_pct=3.5, thorium_ppm=12.0,
       composition_notes="Defines the Procellarum KREEP Terrane — anomalously "
                         "high thorium, potassium, and rare-earth elements. "
                         "Geochemically unique on the Moon.",
       earth_visible=True, max_sun_altitude_deg=72,
       formed_by="basaltic flooding (no clean impact basin)",
       formation_notes="Not a single impact event. The Procellarum 'super-basin' "
                       "model is contested; may instead reflect a thinned crust "
                       "and elevated heat flow that allowed prolonged volcanism.",
       significance="Site of Aristarchus crater (brightest spot on the Moon), "
                    "Schroeter's Valley, Marius Hills volcanic complex, and the "
                    "Apollo 12 landing.",
       missions=["Apollo 12", "Surveyor 1", "Surveyor 3", "Luna 9", "Luna 13"],
       named_after="Latin: Ocean of Storms."),

    _Z(name="Mare Nubium",
       feature_type="mare", lat=-21.3, lon=-16.5, diameter_km=715,
       age_ga=3.5,
       terrain="Mid-southern basaltic plain. Flat, with several embedded "
               "craters and the Straight Wall (Rupes Recta) fault.",
       dominant_rock="basalt",
       iron_pct=12.5,
       earth_visible=True, max_sun_altitude_deg=68,
       formed_by="basaltic flooding",
       missions=["Ranger 7"],  # impacted here in 1964
       significance="Ranger 7 returned the first close-up TV images of the "
                    "Moon, July 1964.",
       named_after="Latin: Sea of Clouds."),

    _Z(name="Mare Humorum",
       feature_type="mare", lat=-24.4, lon=-38.6, diameter_km=389,
       age_ga=3.7,
       terrain="Small, neat circular mare in the southwest. Steep rim, "
               "concentric rilles in the surrounding highlands.",
       dominant_rock="basalt",
       earth_visible=True,
       formed_by="impact + basaltic flooding",
       named_after="Latin: Sea of Moisture."),

    _Z(name="Mare Nectaris",
       feature_type="mare", lat=-15.2, lon=35.3, diameter_km=339,
       age_ga=3.9,
       terrain="Smaller basaltic mare in the southeast near side. Rim defines "
               "the Nectarian stratigraphic system.",
       dominant_rock="basalt",
       earth_visible=True,
       formed_by="impact + basaltic flooding",
       significance="Type-locality for the Nectarian period (~3.92–3.85 Ga).",
       named_after="Latin: Sea of Nectar."),

    _Z(name="Mare Vaporum",
       feature_type="mare", lat=13.3, lon=3.6, diameter_km=245,
       terrain="Small mare just north of the equator, between Mare Imbrium "
               "and Mare Tranquillitatis. Borders the Apennine Mountains.",
       dominant_rock="basalt",
       earth_visible=True,
       named_after="Latin: Sea of Vapors."),

    _Z(name="Mare Cognitum",
       feature_type="mare", lat=-10.0, lon=-23.1, diameter_km=350,
       terrain="Basaltic plain south of Copernicus. 'Sea That Has Become Known' "
               "— named after Ranger 7 deliberately impacted here.",
       dominant_rock="basalt",
       missions=["Ranger 7", "Surveyor 3", "Apollo 12", "Apollo 14"],
       earth_visible=True),

    _Z(name="Mare Marginis",
       feature_type="mare", lat=13.3, lon=86.1, diameter_km=420,
       terrain="Basaltic plain on the eastern limb, only fully visible during "
               "favorable libration.",
       dominant_rock="basalt",
       earth_visible=True),  # right at the limb

    _Z(name="Mare Smythii",
       feature_type="mare", lat=-1.3, lon=87.5, diameter_km=373,
       terrain="Limb mare, equatorial. Filled with thin basalt layer over a "
               "crustal thinning.",
       dominant_rock="basalt",
       earth_visible=True),

    _Z(name="Mare Australe",
       feature_type="mare", lat=-38.9, lon=93.0, diameter_km=997,
       terrain="Highly degraded mare straddling the southeast limb. Mostly "
               "buried under highland ejecta.",
       dominant_rock="basalt",
       earth_visible=False),  # mostly far side

    _Z(name="Mare Fecunditatis",
       feature_type="mare", lat=-7.8, lon=51.3, diameter_km=909,
       age_ga=3.4,
       terrain="Large irregular mare in the southeast. Contains the prominent "
               "Messier crater chain.",
       dominant_rock="basalt",
       missions=["Luna 16"],
       earth_visible=True,
       named_after="Latin: Sea of Fertility."),

    _Z(name="Sinus Iridum",
       feature_type="mare", lat=44.1, lon=-31.5, diameter_km=249,
       terrain="Half-circle 'bay' on the northwest edge of Mare Imbrium. "
               "Promontorium Heraclides and Promontorium Laplace cap its arms.",
       dominant_rock="basalt",
       missions=["Chang'e 3"],
       earth_visible=True,
       named_after="Latin: Bay of Rainbows."),

    _Z(name="Sinus Medii",
       feature_type="mare", lat=2.4, lon=1.7, diameter_km=287,
       terrain="Small basaltic 'bay' near the lunar centre, almost on the "
               "0/0 sub-Earth point.",
       dominant_rock="basalt",
       earth_visible=True,
       missions=["Surveyor 4", "Surveyor 6"],
       lighting_notes="Earth always at zenith — most centred of all named "
                      "features as seen from Earth.",
       named_after="Latin: Central Bay."),

    # === Far-side maria + basins ===
    _Z(name="Mare Moscoviense",
       feature_type="mare", lat=27.3, lon=147.9, diameter_km=276,
       terrain="One of the few basaltic-floored areas on the otherwise highland "
               "far side. Located in the Moscoviense impact basin.",
       dominant_rock="basalt",
       earth_visible=False,
       formed_by="impact + thin basalt fill",
       significance="Found by Soviet Luna 3 in 1959 — first far-side imagery.",
       named_after="Latin: Sea of Muscovy / Moscow."),

    _Z(name="Mare Ingenii",
       feature_type="mare", lat=-33.7, lon=163.5, diameter_km=318,
       terrain="Small far-side mare in the Ingenii basin. Hosts unusual "
               "swirl features (Reiner Gamma analog).",
       dominant_rock="basalt",
       earth_visible=False,
       named_after="Latin: Sea of Cleverness."),

    _Z(name="Mare Orientale",
       feature_type="mare", lat=-19.4, lon=-92.8, diameter_km=294,
       age_ga=3.85,
       terrain="Multi-ringed impact basin on the western limb. The youngest "
               "and freshest large basin on the Moon — concentric rings of "
               "Cordillera and Rook Mountains preserved.",
       dominant_rock="basalt (small central fill)",
       earth_visible=False,  # mostly limb / far side
       formed_by="multi-ring impact basin",
       significance="Best-preserved multi-ring basin in the solar system — "
                    "the type example for understanding very large impacts."),

    _Z(name="South Pole–Aitken basin",
       feature_type="basin", lat=-53.0, lon=191.0, diameter_km=2400,
       elevation_floor_km=-9.0, elevation_rim_km=+1.5,
       age_ga=4.3,
       terrain="The largest, deepest, and oldest known impact basin in the "
               "solar system. ~2400 km across, up to 13 km deep relative to "
               "the surrounding highlands. Far-side and southern.",
       dominant_rock="impact-melt breccia + basalt patches",
       iron_pct=8.0, thorium_ppm=2.5,
       composition_notes="Anomalously iron- and thorium-poor (vs. the rest of "
                         "the far side), suggesting it excavated lower crust "
                         "or upper-mantle material.",
       earth_visible=False, permanently_shadowed=False,
       formed_by="oblique mega-impact",
       formation_notes="Pre-Nectarian (~4.3 Ga). Probably the oldest visible "
                       "impact feature on the Moon.",
       significance="Sample-return target for proposed missions (Endurance-A, "
                    "etc.) — would deliver the oldest lunar material.",
       missions=["Chang'e 4 (in Von Karman crater on the floor)"],
       named_after="Bounded by the South Pole and Aitken crater (named after "
                   "Robert Grant Aitken, US astronomer)."),

    _Z(name="Mare Hertzsprung",
       feature_type="mare", lat=0.6, lon=-128.7, diameter_km=536,
       terrain="Small basaltic patch in the floor of the giant Hertzsprung "
               "ringed basin (570 km).",
       dominant_rock="basalt",
       earth_visible=False),
]


# ===========================================================================
# Famous craters
# ===========================================================================

ZONES_CRATER = [
    _Z(name="Tycho",
       feature_type="crater", lat=-43.3, lon=-11.4, diameter_km=86,
       elevation_floor_km=-4.5, elevation_rim_km=+2.5,
       age_ga=0.108,  # ~108 million years — very young
       terrain="Sharp, fresh complex crater with terraced walls, a 1.6-km "
               "central peak, and an extensive radial bright-ray system "
               "extending up to 1500 km. Floor is hummocky impact melt.",
       dominant_rock="impact-melt breccia + anorthositic ejecta",
       earth_visible=True, max_sun_altitude_deg=47,
       formed_by="impact",
       formation_notes="Formed ~108 Ma in the Copernican period. "
                       "Far younger than Copernicus despite similar size.",
       significance="Brightest visible crater from Earth at full moon. Its "
                    "rays cross most of the southern hemisphere. Source of "
                    "many secondary craters across the near side.",
       missions=["Surveyor 7"],  # landed near rim, 1968
       named_after="Tycho Brahe (Danish astronomer, 1546–1601)."),

    _Z(name="Copernicus",
       feature_type="crater", lat=9.6, lon=-20.0, diameter_km=93,
       elevation_floor_km=-3.8, elevation_rim_km=+1.0,
       age_ga=0.8,  # ~800 million years
       terrain="Large complex crater with terraced inner walls (multiple "
               "slump terraces), a central-peak cluster, and a prominent "
               "ray system. Floor has hummocky impact melt and several "
               "small craters.",
       dominant_rock="impact-melt breccia",
       earth_visible=True, max_sun_altitude_deg=80,
       formed_by="impact",
       formation_notes="~800 Ma. Type-locality for the Copernican period.",
       significance="Centerpiece of near-side telescope views; rays reach "
                    "Mare Imbrium and Mare Cognitum. Studied as the textbook "
                    "complex crater.",
       named_after="Nicolaus Copernicus (Polish astronomer, 1473–1543)."),

    _Z(name="Aristarchus",
       feature_type="crater", lat=23.7, lon=-47.4, diameter_km=40,
       age_ga=0.45,
       terrain="Bright, fresh crater perched on a 200-km-wide volcanic plateau "
               "(Aristarchus Plateau). Crater walls reveal layered rock; floor "
               "is bright with subdued central peak.",
       dominant_rock="anorthositic + KREEP-rich",
       thorium_ppm=15.0,
       composition_notes="One of the highest thorium readings on the Moon "
                         "from gamma-ray spectroscopy.",
       earth_visible=True,
       formed_by="impact",
       significance="Brightest large feature on the Moon — easily visible to "
                    "the naked eye. The Aristarchus Plateau is rich in sinuous "
                    "rilles (Schroeter's Valley) and pyroclastic deposits.",
       named_after="Aristarchus of Samos (Greek astronomer, c. 310–230 BC)."),

    _Z(name="Plato",
       feature_type="crater", lat=51.6, lon=-9.4, diameter_km=101,
       elevation_floor_km=-2.0, elevation_rim_km=+1.0,
       age_ga=3.84,  # very old, Imbrian-age
       terrain="Walled plain with a remarkably flat, dark, basalt-flooded "
               "floor — almost no central peak. Sits on the northern shore "
               "of Mare Imbrium.",
       dominant_rock="basalt floor + anorthositic walls",
       earth_visible=True,
       formed_by="impact + later basalt flooding",
       significance="Floor's flat darkness makes Plato a famous 'eye' on the "
                    "lunar surface. Long history of telescopic observation.",
       named_after="Plato (Greek philosopher, c. 428–348 BC)."),

    _Z(name="Clavius",
       feature_type="crater", lat=-58.4, lon=-14.4, diameter_km=231,
       age_ga=4.0,  # pre-Nectarian
       terrain="Massive, ancient walled plain in the southern highlands. "
               "Heavily eroded rim, flat floor punctuated by an arc of "
               "smaller progressively-younger craters (Rutherfurd, Clavius "
               "C/D/N).",
       dominant_rock="anorthositic breccia",
       earth_visible=True, max_sun_altitude_deg=32,
       formed_by="impact",
       significance="One of the oldest large impact features still preserved "
                    "on the near side. Famous in 2001: A Space Odyssey as the "
                    "Clavius Base location.",
       named_after="Christopher Clavius (German Jesuit astronomer, 1538–1612)."),

    _Z(name="Plinius",
       feature_type="crater", lat=15.4, lon=23.7, diameter_km=43,
       age_ga=2.5,
       terrain="Sharp-rimmed Eratosthenian crater straddling the boundary "
               "between Mare Tranquillitatis and Mare Serenitatis. Twin "
               "central peaks; floor is partially flooded by mare basalt.",
       dominant_rock="impact-melt breccia + basalt floor",
       earth_visible=True,
       formed_by="impact",
       named_after="Pliny the Elder (Roman naturalist, 23–79 AD)."),

    _Z(name="Aristoteles",
       feature_type="crater", lat=50.2, lon=17.4, diameter_km=87,
       age_ga=3.5,
       terrain="Large Imbrian-age crater on the south shore of Mare Frigoris. "
               "Terraced walls, hummocky floor, no clear central peak.",
       earth_visible=True,
       formed_by="impact",
       named_after="Aristotle (Greek philosopher, 384–322 BC)."),

    _Z(name="Kepler",
       feature_type="crater", lat=8.1, lon=-38.0, diameter_km=32,
       age_ga=0.625,
       terrain="Small, sharp Copernican-age crater with bright rays in "
               "Oceanus Procellarum. Walls reveal stratification.",
       earth_visible=True,
       formed_by="impact",
       named_after="Johannes Kepler (German astronomer, 1571–1630)."),

    _Z(name="Posidonius",
       feature_type="crater", lat=31.8, lon=29.9, diameter_km=95,
       age_ga=3.9,
       terrain="Old, eroded crater on the northeast rim of Mare Serenitatis. "
               "Notable interior rilles (Rimae Posidonius) on the floor.",
       earth_visible=True),

    _Z(name="Theophilus",
       feature_type="crater", lat=-11.4, lon=26.4, diameter_km=110,
       age_ga=1.0,
       terrain="Young (Copernican-age) sharp-rimmed crater with prominent "
               "central-peak cluster. Forms a striking trio with Cyrillus "
               "and Catharina.",
       earth_visible=True),

    _Z(name="Tsiolkovsky",
       feature_type="crater", lat=-20.4, lon=129.1, diameter_km=185,
       terrain="Most prominent feature on the lunar far side. Very dark "
               "basaltic floor in stark contrast to surrounding bright "
               "highlands; tall central peak in centre of basaltic plain.",
       dominant_rock="basalt floor + anorthositic walls",
       earth_visible=False,
       formed_by="impact + basalt flooding",
       significance="Unique among far-side craters in having a substantial "
                    "mare-basalt floor — most far-side basins lack this.",
       named_after="Konstantin Tsiolkovsky (Russian rocket pioneer, 1857–1935)."),

    _Z(name="Korolev",
       feature_type="crater", lat=-4.4, lon=-157.4, diameter_km=437,
       terrain="Large multi-ring far-side basin. Floor pockmarked with "
               "smaller craters; not flooded by basalt.",
       earth_visible=False,
       formed_by="multi-ring impact",
       named_after="Sergei Korolev (Soviet chief rocket designer, 1907–1966)."),

    _Z(name="Mendeleev",
       feature_type="crater", lat=5.7, lon=140.9, diameter_km=313,
       earth_visible=False,
       named_after="Dmitri Mendeleev (Russian chemist, 1834–1907)."),

    _Z(name="Apollo (crater)",
       feature_type="crater", lat=-36.1, lon=-151.8, diameter_km=494,
       terrain="Far-side double-ringed basin with partially basalt-flooded "
               "floor.",
       earth_visible=False,
       named_after="The Apollo program (commemorative)."),

    _Z(name="Hertzsprung",
       feature_type="crater", lat=1.4, lon=-128.7, diameter_km=570,
       terrain="Multi-ring far-side basin enclosing the small Mare Hertzsprung.",
       earth_visible=False,
       named_after="Ejnar Hertzsprung (Danish astronomer, 1873–1967)."),

    _Z(name="Endymion",
       feature_type="crater", lat=53.6, lon=56.5, diameter_km=125,
       earth_visible=True,
       named_after="Endymion (Greek mythology — the eternally-sleeping "
                   "shepherd loved by the Moon goddess Selene).",
       terrain="Old crater near the northeast limb. Floor is dark basalt "
               "fill — appears black through a telescope.",
       dominant_rock="basalt floor + anorthositic walls"),

    _Z(name="Atlas",
       feature_type="crater", lat=46.7, lon=44.4, diameter_km=87,
       terrain="Forms a famous pair with Hercules in the northeast near side. "
               "Sharp rim, hummocky floor with rilles.",
       earth_visible=True,
       named_after="Atlas (Greek titan)."),

    _Z(name="Hercules",
       feature_type="crater", lat=46.7, lon=39.1, diameter_km=69,
       terrain="Twin to Atlas. Terraced walls and a small central peak.",
       earth_visible=True,
       named_after="Hercules (Greek mythological hero)."),

    _Z(name="Cleomedes",
       feature_type="crater", lat=27.7, lon=56.0, diameter_km=125,
       terrain="Old crater north of Mare Crisium. Subdued rim, floor crossed "
               "by rilles.",
       earth_visible=True),

    _Z(name="Langrenus",
       feature_type="crater", lat=-8.9, lon=60.9, diameter_km=132,
       terrain="Prominent crater on the eastern limb. Terraced walls, central "
               "peak, partly buried in Mare Fecunditatis ejecta.",
       earth_visible=True),

    _Z(name="Petavius",
       feature_type="crater", lat=-25.3, lon=60.4, diameter_km=177,
       terrain="Large crater near the southeast limb with a notable rille "
               "running across its floor (Rima Petavius).",
       earth_visible=True),

    _Z(name="Schickard",
       feature_type="crater", lat=-44.4, lon=-54.6, diameter_km=227,
       terrain="Walled-plain crater with a flat, partially-basalt-flooded "
               "floor. Old (pre-Nectarian).",
       earth_visible=True),

    _Z(name="Bailly",
       feature_type="crater", lat=-66.5, lon=-69.1, diameter_km=300,
       terrain="Largest near-side crater, very old. Heavily eroded rim, "
               "floor pocked with smaller craters.",
       earth_visible=True,
       named_after="Jean Sylvain Bailly (French astronomer, 1736–1793)."),

    _Z(name="Maginus",
       feature_type="crater", lat=-50.0, lon=-6.2, diameter_km=156,
       earth_visible=True,
       terrain="Old, heavily-degraded southern crater riddled with smaller "
               "later impacts."),

    _Z(name="Walter",
       feature_type="crater", lat=-33.1, lon=1.0, diameter_km=140,
       terrain="Large old crater in the southern highlands.",
       earth_visible=True),

    _Z(name="Ptolemaeus",
       feature_type="crater", lat=-9.3, lon=-1.9, diameter_km=153,
       terrain="Old walled-plain crater with a flat, slightly-darker floor. "
               "Forms a famous chain with Alphonsus and Arzachel.",
       earth_visible=True,
       named_after="Ptolemy (Greek astronomer/geographer, c. 100–170 AD)."),

    _Z(name="Alphonsus",
       feature_type="crater", lat=-13.7, lon=-2.8, diameter_km=119,
       terrain="Floor has a low central peak, dark-halo craters (suspected "
               "endogenic), and rilles. Site of Ranger 9 impact (1965) — "
               "returned the first close-up images of a target landing site.",
       missions=["Ranger 9"],
       earth_visible=True),

    _Z(name="Arzachel",
       feature_type="crater", lat=-18.2, lon=-1.9, diameter_km=96,
       terrain="Sharp-rimmed crater south of Alphonsus, with a prominent "
               "central peak and rilles.",
       earth_visible=True,
       named_after="al-Zarqali (Arab astronomer, 1029–1087)."),

    _Z(name="Cyrillus",
       feature_type="crater", lat=-13.3, lon=24.0, diameter_km=98,
       earth_visible=True),

    _Z(name="Catharina",
       feature_type="crater", lat=-18.1, lon=23.4, diameter_km=100,
       earth_visible=True),

    _Z(name="Gagarin",
       feature_type="crater", lat=-19.5, lon=149.2, diameter_km=265,
       earth_visible=False,
       named_after="Yuri Gagarin (Soviet cosmonaut, first human in space)."),

    _Z(name="Daedalus",
       feature_type="crater", lat=-5.9, lon=179.4, diameter_km=93,
       earth_visible=False,
       named_after="Daedalus (Greek mythological inventor)."),

    _Z(name="Jules Verne",
       feature_type="crater", lat=-35.0, lon=147.0, diameter_km=143,
       earth_visible=False,
       named_after="Jules Verne (French author, 1828–1905)."),

    _Z(name="Belyaev",
       feature_type="crater", lat=23.3, lon=143.5, diameter_km=54,
       earth_visible=False,
       named_after="Pavel Belyayev (Soviet cosmonaut)."),
]


# ===========================================================================
# Polar craters (often permanently shadowed → ice deposits)
# ===========================================================================

ZONES_POLAR = [
    _Z(name="Shackleton",
       feature_type="crater", lat=-89.7, lon=110.0, diameter_km=21,
       terrain="Crater straddling the lunar south pole. Interior is "
               "permanently shadowed; exterior rim catches near-constant "
               "sunlight (a 'peak of eternal light').",
       permanently_shadowed=True, earth_visible=True,
       max_sun_altitude_deg=1.5,
       lighting_notes="Inside the crater, the sun never rises. Floor "
                      "temperature stays below ~110 K. Rim peaks see sun "
                      "for 80–90% of the year.",
       formed_by="impact",
       composition_notes="Hydrogen signal from neutron spectrometers and "
                         "direct LCROSS impact (in adjacent Cabeus crater) "
                         "indicate water ice in the regolith.",
       significance="Top candidate site for crewed return missions and ISRU "
                    "(in-situ resource utilization). Artemis-3 target zone. "
                    "Chandrayaan-3 (Vikram lander, 2023) landed nearby.",
       missions=["Chandrayaan-3 (nearby, 2023)"],
       named_after="Sir Ernest Shackleton (British Antarctic explorer, "
                   "1874–1922)."),

    _Z(name="Peary",
       feature_type="crater", lat=88.6, lon=33.0, diameter_km=73,
       terrain="Crater very near the lunar north pole. Like Shackleton, has "
               "permanently-shadowed interior regions and rim peaks with "
               "near-constant illumination.",
       permanently_shadowed=True, earth_visible=True,
       max_sun_altitude_deg=2.0,
       composition_notes="Suspected ice deposits in shadowed regions.",
       significance="North-pole counterpart to Shackleton. Less studied — "
                    "south-pole has hosted more recent missions.",
       named_after="Robert Peary (American Arctic explorer, 1856–1920)."),

    _Z(name="Faustini",
       feature_type="crater", lat=-87.2, lon=77.0, diameter_km=39,
       terrain="South-polar crater. Floor permanently shadowed. Adjacent to "
               "Shackleton; on the proposed Artemis-3 traverse map.",
       permanently_shadowed=True, earth_visible=True,
       composition_notes="Significant hydrogen / suspected water-ice signal.",
       significance="Active candidate for ice-prospecting rover missions."),
]


# ===========================================================================
# Landing sites
# ===========================================================================

ZONES_LANDING = [
    # Apollo
    _Z(name="Apollo 11",
       feature_type="landing", lat=0.7, lon=23.5, diameter_km=1,
       terrain="Flat mare-basalt plain in southwestern Mare Tranquillitatis. "
               "First crewed landing site. Visible boulders, two small craters "
               "(Little West, ~30 m), gentle rolling regolith.",
       dominant_rock="high-Ti mare basalt",
       iron_pct=15.5, titanium_pct=8.5,
       composition_notes="Apollo 11 returned ilmenite-rich basalts dated to "
                         "~3.7 Ga. First samples ever returned from another "
                         "world.",
       earth_visible=True, max_sun_altitude_deg=82,
       lighting_notes="Equatorial — lighting at the time of landing was a "
                      "low morning sun for crew visibility (~10° altitude).",
       formed_by="basaltic flooding",
       formation_notes="Mare basalt erupted ~3.7 Ga, then accumulated regolith "
                       "from micrometeorite gardening over the next 3.7 Gy.",
       significance="20 July 1969 — Neil Armstrong and Buzz Aldrin became the "
                    "first humans to walk on another world. Returned 21.6 kg "
                    "of samples and demonstrated the feasibility of sustained "
                    "human-Moon operations.",
       missions=["Apollo 11"]),

    _Z(name="Apollo 12",
       feature_type="landing", lat=-3.2, lon=-23.4, diameter_km=1,
       terrain="Mare Cognitum / Oceanus Procellarum boundary. Pinpoint "
               "landing within walking distance of Surveyor 3 (which had "
               "soft-landed there 2.5 years earlier).",
       dominant_rock="mare basalt + KREEP-bearing impact ejecta",
       iron_pct=14.5,
       earth_visible=True,
       formed_by="basaltic flooding + Copernicus ray ejecta",
       significance="14 November 1969 — Pete Conrad and Alan Bean. Returned "
                    "Surveyor 3 hardware components for study of long-term "
                    "lunar exposure effects.",
       missions=["Apollo 12", "Surveyor 3"]),

    _Z(name="Apollo 14",
       feature_type="landing", lat=-3.6, lon=-17.5, diameter_km=1,
       terrain="Fra Mauro highlands — a hummocky region thought to be "
               "Imbrium impact ejecta. Crew traversed up Cone Crater rim "
               "via two-wheeled MET cart.",
       dominant_rock="impact breccia + Imbrium ejecta",
       earth_visible=True,
       formed_by="ejecta from the Imbrium impact",
       significance="February 1971 — Alan Shepard, Edgar Mitchell. Returned "
                    "the largest Apollo single sample (Big Bertha, 8.9 kg). "
                    "Shepard's golf shot.",
       missions=["Apollo 14"]),

    _Z(name="Apollo 15",
       feature_type="landing", lat=26.1, lon=3.6, diameter_km=1,
       terrain="Hadley Rille / Apennine Mountain front. Most geologically "
               "diverse Apollo site — mare basalt, highland anorthosite, "
               "and a sinuous rille (a collapsed lava tube). First mission "
               "with the Lunar Roving Vehicle.",
       dominant_rock="mare basalt + anorthosite (at the Apennine front)",
       earth_visible=True,
       formed_by="multi-event: Imbrium impact rim + later basalt floods",
       significance="July–August 1971 — Dave Scott, Jim Irwin. Returned the "
                    "Genesis Rock (anorthosite, 4.1 Ga, mantle-derived). First "
                    "use of the LRV; tripled traverse range.",
       missions=["Apollo 15"]),

    _Z(name="Apollo 16",
       feature_type="landing", lat=-8.9, lon=15.5, diameter_km=1,
       terrain="Descartes Highlands — central nearside highlands. Crew tested "
               "the hypothesis that the bright highlands were volcanic; "
               "samples showed they're actually shock-modified anorthosite.",
       dominant_rock="anorthositic breccia",
       earth_visible=True,
       formed_by="ancient highland crust, lightly modified",
       significance="April 1972 — John Young, Charles Duke. Demonstrated that "
                    "lunar highlands are impact ejecta + primary anorthositic "
                    "crust, not silicic volcanism. Major shift in lunar "
                    "geological understanding.",
       missions=["Apollo 16"]),

    _Z(name="Apollo 17",
       feature_type="landing", lat=20.2, lon=30.8, diameter_km=1,
       terrain="Taurus-Littrow Valley — narrow valley between mountainous "
               "highland blocks at the southeastern Mare Serenitatis rim. "
               "Three-day surface stay, 22-km LRV traverse. Mountains rise "
               "~2–3 km above the valley floor.",
       dominant_rock="basalt + orange pyroclastic glass + anorthosite",
       iron_pct=15.0, titanium_pct=4.5,
       composition_notes="Discovery of orange soil — pyroclastic glass beads "
                         "from a fire-fountain eruption ~3.6 Ga.",
       earth_visible=True, max_sun_altitude_deg=63,
       formed_by="multi-event: Serenitatis basin rim + late mare basalt + "
                 "pyroclastic deposits",
       significance="11–14 December 1972 — last crewed lunar landing. Gene "
                    "Cernan, Jack Schmitt (only scientist on the Moon). "
                    "Returned 110.5 kg of samples — the largest Apollo haul.",
       missions=["Apollo 17"]),

    # Luna landers (USSR)
    _Z(name="Luna 9",
       feature_type="landing", lat=7.1, lon=-64.4, diameter_km=1,
       terrain="Western Oceanus Procellarum, Marius Hills region. Featureless "
               "regolith plain with small craters and rocks.",
       dominant_rock="basalt regolith",
       earth_visible=True,
       significance="3 February 1966 — first soft landing on the Moon. "
                    "Returned the first panoramic images from the lunar "
                    "surface, proving regolith would support a heavy lander."),

    _Z(name="Luna 16",
       feature_type="landing", lat=-0.7, lon=56.4, diameter_km=1,
       terrain="Mare Fecunditatis, smooth basaltic plain.",
       dominant_rock="basalt regolith",
       earth_visible=True,
       significance="20 September 1970 — first robotic sample return. "
                    "Brought 101 g of soil back to Earth."),

    _Z(name="Luna 17",
       feature_type="landing", lat=38.3, lon=-35.0, diameter_km=1,
       terrain="Mare Imbrium — Sinus Iridum / Promontorium Heraclides region. "
               "Lunokhod 1 rover deployed and traversed 10.5 km over 11 lunar "
               "days.",
       earth_visible=True,
       significance="17 November 1970 — first robotic rover on another "
                    "world (Lunokhod 1)."),

    _Z(name="Luna 21",
       feature_type="landing", lat=25.9, lon=30.5, diameter_km=1,
       terrain="LeMonnier crater on the eastern Mare Serenitatis rim. "
               "Lunokhod 2 rover deployed (37 km traverse — record stood "
               "until 2014).",
       earth_visible=True,
       significance="January 1973 — second Lunokhod, longest extraterrestrial "
                    "rover traverse for ~40 years."),

    # Other recent missions
    _Z(name="Chang'e 4",
       feature_type="landing", lat=-45.5, lon=177.6, diameter_km=1,
       terrain="Von Kármán crater on the floor of the South Pole–Aitken "
               "basin — the lowest, oldest region on the Moon.",
       dominant_rock="SPA-basin breccia + small basalt patches",
       earth_visible=False, permanently_shadowed=False,
       lighting_notes="No direct line of sight to Earth — communication "
                      "relayed via the Queqiao satellite at L2.",
       significance="3 January 2019 — first soft landing on the lunar far "
                    "side. Yutu-2 rover (still operational as of 2024) is "
                    "the longest-active lunar rover."),

    _Z(name="Chang'e 5",
       feature_type="landing", lat=43.1, lon=-51.9, diameter_km=1,
       terrain="Mons Rümker volcanic complex in northern Oceanus Procellarum. "
               "Among the youngest mare basalts on the Moon.",
       dominant_rock="young (1.97 Ga) mare basalt",
       earth_visible=True,
       significance="December 2020 — first lunar sample return since Luna 24 "
                    "(1976). Returned 1.7 kg, including the youngest mare "
                    "basalts ever sampled, redating lunar volcanism by ~1 Gy."),

    _Z(name="Chandrayaan-3",
       feature_type="landing", lat=-69.4, lon=32.3, diameter_km=1,
       terrain="High-southern-latitude highlands, near the rim of the south "
               "polar zone. Slope-rich, shadow-cast terrain.",
       earth_visible=True, max_sun_altitude_deg=21,
       lighting_notes="Long shadows, low sun; lander/rover sized lifetime to "
                      "one lunar daylight period (~14 days).",
       significance="23 August 2023 — Vikram lander touchdown made India the "
                    "fourth nation to soft-land on the Moon and the first to "
                    "land near the south pole."),

    _Z(name="Surveyor 3",
       feature_type="landing", lat=-3.0, lon=-23.3, diameter_km=1,
       terrain="Surveyor crater in Oceanus Procellarum.",
       earth_visible=True,
       significance="20 April 1967 — robotic sampler/digger lander. Visited "
                    "by Apollo 12 crew 2.5 years later; pieces returned to "
                    "Earth for long-term-exposure analysis.",
       missions=["Apollo 12"]),
]


# ===========================================================================
# Aggregate
# ===========================================================================

ALL_ZONES = ZONES_MARE + ZONES_CRATER + ZONES_POLAR + ZONES_LANDING


def export_json(path):
    """Write the whole dataset as JSON for the C++ side to load."""
    data = [asdict(z) for z in ALL_ZONES]
    with open(path, "w") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    print(f"  wrote {path} ({len(data)} zones)")


def lookup_by_name(name):
    """Find a zone by name — used by the info-panel renderer."""
    for z in ALL_ZONES:
        if z.name == name:
            return z
    return None


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "..", "src", "assets", "planet",
                        "zones.json")
    out = os.path.normpath(out)
    print(f"== zone database — {len(ALL_ZONES)} zones ==")
    counts = {}
    for z in ALL_ZONES:
        counts[z.feature_type] = counts.get(z.feature_type, 0) + 1
    for k, v in counts.items():
        print(f"  {k:12s} {v:3d}")
    export_json(out)
