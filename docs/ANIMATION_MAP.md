# Animation asset map

## Scope and evidence

This map uses the original installation at `E:\Port\Age of Mythology` and the project's `tools/parse_bar.py` BAR reader. It covers the Greek MVP units: Hoplite, Toxotes, Minotaur, and Villager Greek.

The animation archives `anim\anim.bar` and `anim\anim2.bar` have observed SHA-256 values `43f96d96279fc5b4fe3250eb9d4f6f0384174c1f6a97d3522e0ec2dd6cf79cf0` and `e1c845a13224a623ba78170a608e2da4c456c397a49ac5430cee5479486a0e95`. The model archive `models\models.bar` was indexed for member lookup. REA CLI 6.8.0 rejected the BAR format as unsupported, so the file-level observations below come from the local parser and bounded inspection of extracted bytes, not REA Evidence.

## Script grammar observed

The `.txt` members are declarative animation-selection scripts, not rendered clips.

- `import <file>` brings in shared definitions. Imports resolve case-insensitively within `anim.bar` for all four MVP scripts.
- `define <name> { ... }` declares reusable visual/attachment definitions, such as `Head`, `Spear`, and shield/bow definitions.
- `anim <state> { ... }` declares a state such as `Idle`, `Walk`, `Attack`, or `Death`.
- Nested selector blocks observed include `SetSelector`, `TechLogic`, `VariationLogic`, `TargetLogic`, `InventoryLogic`, `CarryTypeLogic`, and `CinematicLogic`.
- `set hotspot { name ...; version { ... } }` groups a selected visual and its metadata. Inside these blocks, commands include `Visual`, `VisualGranny`, `connect`, `ReplaceTexture`, and event `tag` records.
- `connect <parent-hotspot> <attachment-definition> hotspot` expresses attachment relationships. For example, the Hoplite script connects `RightHand` to `Spear`, `LeftForearm` to `LargeGreekShield`, and `TopOfHead` to `Head`.
- `tag` records carry event timing/flags. Hoplite's Granny attack branch includes `tag Attack 0.61 true` and `tag GenericSound 0.40 true`.

The selector grammar is only mapped at the token/structure level; condition precedence and runtime selection semantics have not been reconstructed.

## MVP script and clip map

Frame counts below are the first `u32` in each BRG `ASET` payload. Each listed BRG was found in `models.bar`; the observed number of `MESI` chunks matched that count.

### Hoplite — `anim\hoplite_anim.txt`

Imports: `greek_large_shield_anim.txt`, `spear_defines_anim.txt`.

| Script state | BRG visual(s) and frame count | `VisualGranny` visual |
|---|---|---|
| Bored | `infantry g hoplite_boreda.brg` — 12 | `Hop_idleA` |
| Idle | `infantry g hoplite_idlea.brg` — 3 | `Hop_idleA` |
| run, Walk | `infantry g hoplite_walka.brg` — 10 | `Hop_WalkA` on Walk |
| Attack | `infantry g hoplite_attacka.brg`, `infantry g hoplite_attackb.brg` — 10 each | `Hop_AttackA` |
| Death | `infantry g hoplite_deatha.brg` — 14 | — |
| Flail | `infantry g hoplite_flaila.brg` — 10 | — |
| Cinematic | — | `Hop_08SirTheTroj` |

The referenced Granny files `hop_idlea.grn`, `hop_walka.grn`, `hop_attacka.grn`, and `hop_08sirthetroj.grn` exist in `models.bar`. The imported definitions supply the spear and Greek large-shield attachment variants.

### Toxotes — `anim\toxotes_anim.txt`

Imports: `greek_archer_shield_anim.txt`, `bow_defines_anim.txt`.

| Script state | BRG visual and frame count | `VisualGranny` visual |
|---|---|---|
| Idle | `archer g toxotes_idlea.brg` — 6 | `Tox_IdleA` |
| walk | `archer g toxotes_walka.brg` — 12 | `Tox_WalkA` |
| RangedAttack | `archer g toxotes_attacka.brg` — 15 | — |
| Death | `archer g toxotes_deatha.brg` — 8 | — |
| Bored | `archer g toxotes_boreda.brg` — 16 | `Tox_IdleA` |
| Flail | `archer g toxotes_flaila.brg` — 6 | — |
| Cinematic | — | `Tox_aomsp03point`, `Tox_cine231signalshota`, `Tox_cine231signalshotb` |

The referenced Granny files (`tox_idlea.grn`, `tox_walka.grn`, and the three cinematic names with `.grn`) exist in `models.bar`. The script's attachment points include `Righthand`/`bow`, `leftforearm`/`GreekArcherShield`, and arrow/fire effect points.

### Minotaur — `anim\minotaur_anim.txt`

Imports: none.

| Script state | BRG visual(s) and frame count |
|---|---|
| Birth | `special g minotaur_birtha.brg` — 13 |
| Idle | `special g minotaur_idlea.brg` — 3 |
| bored | `special g minotaur_boreda.brg` — 13; `special g minotaur_boredb.brg` — 10 |
| Attack | `special g minotaur_attacka.brg` — 15; `special g minotaur_attackb.brg` — 12; `special g minotaur_attackc.brg` — 12 |
| Gore | `special g minotaur_gorea.brg` — 14 |
| Death | `special g minotaur_deatha.brg` — 16 |
| Walk | `special g minotaur_walka.brg` — 12 |
| Flail | `special g minotaur_flaila.brg` — 6 |

No `VisualGranny` command occurs in this script.

### Greek Villager — `anim\villager greek_anim.txt`

Import: `bow_defines_anim.txt`.

| Script state | BRG visual(s) and frame count |
|---|---|
| GatherChop | male 12; female 13 (`villager g *_chopa.brg`) |
| GatherMine | male/female `*_diga.brg` — 9 each; male `*_minea.brg` — 11; female `*_minea.brg` — 15 |
| Attack | male/female `*_chopdown.brg` — 6 each; male `*_handattack.brg` — 10; female — 14 |
| RangedWalkAttack | **Unresolved:** script names male and female `RangedWalkAttackA`; no matching BRG was found in `models.bar` or `models2.bar` |
| RangedAttack | male/female `*_rangedattack.brg` — 12 each |
| Death | male/female `*_deatha.brg` — 13 each |
| Idle | male/female `*_idlea.brg` — 2 each; carry idle male — 2, female — 6 |
| Bored / walk | bored male 19, female 8; walk male 6, female 12 |
| Build | male `villager g male_builda.brg` — 10, `male_diga.brg` — 9, `male_sow.brg` — 7; female `female_builda.brg` — 20, `female_diga.brg` — 9, `female_sow.brg` — 8 |
| GatherFarm / Gatherbush / GatherHunt | male/female `*_farma.brg` — 7; `*_harvesta.brg` — 8; `*_slice.brg` — 10 |
| carry / Pray / flail | carry male/female — 6 each; Pray male A/B — 8/12, female A/B — 10/12; flail male/female — 6 each |
| Cinematic | `vill_cine102woundedone.brg` — 6; `villa_cine102woundedtwo.brg` — 25; `villa_cine102woundedtwoidle.brg` — 4 |

This script has no `VisualGranny` commands. Its `Visual` names resolve to BRG resources except the two `RangedWalkAttackA` references. That is an observed unresolved resource lookup, not proof the state is used in the MVP.

## BRG frame structure observed

The current inventory found 7,744 BRGs across `models.bar` and `models2.bar`: 5,507 contain an `ASET` marker and 2,237 do not. In 5,506 of the 5,507 `ASET` files, the first `u32` of `ASET` equals the count of `MESI` markers. The exception is `sfx a spider_idlea.brg`, which declares one `ASET` frame but contains no `MESI`.

For representative Hoplite, Toxotes, and Minotaur clips:

1. The 28-byte `BANG` header is followed by `ASET` and a 28-byte payload.
2. The first `u32` in that payload matches the repeated `MESI` record count.
3. Each `MESI` record has the same vertex and face counts for a clip. In Hoplite idle, the observed counts are 197 vertices and 140 faces.
4. Reading the first 197 × 3 float32 values after each `MESI` header yields finite values that differ between frames. This supports a per-frame vertex-data interpretation.
5. The first `MESI` data region is 2,302 bytes larger than each later frame region in the inspected Hoplite idle, walk, and death files. The exact meaning of those extra bytes and the remaining `MESI` fields are not yet decoded; do not treat the current `stride` field (12 in idle/walk, 44 in death) as a verified vertex stride.

Example identities:

- `infantry g hoplite_idlea.brg`: 12,151 bytes, SHA-256 `637088de4fd8d0ce789534b1d981e5c71b9e9ab60255a38955bc6f39ba23519e`; ASET=3, MESI=3.
- `infantry g hoplite_walka.brg`: 34,530 bytes, SHA-256 `af4efdc2f05c9e7ad561a22fc5341719b5bc73fccb0180dfa8c05f2545224cbb`; ASET=10, MESI=10.
- `infantry g hoplite_deatha.brg`: 50,636 bytes, SHA-256 `bd198c6d393c6f4c5a8f6bf7867c363df8e0a4dda1811188f7e3ed9859a860e1`; ASET=14, MESI=14.

## Granny rig and hotspot relationship

`VisualGranny <name>` resolves to a `.grn` member by appending `.grn` and matching case-insensitively. The model archives contain 584 `.grn` members in `models.bar` and 287 in `models2.bar`. The Hoplite idle/walk/attack files contain readable joint/object names including `Bip01 sold1 R Hand`, `Bip01 sold1 L Forearm`, `Bip01 sold1 Head`, `bone Spear`, and `bone shield`. The Toxotes idle/walk files contain `Bip01 R Hand`, `bone bow`, `bone arrow`, and `bone string`.

These names line up with the script's named attachment hotspots (hand-to-spear/bow, forearm-to-shield, and head-to-head definition). This is a strong name-level cross-reference, but the Granny node hierarchy, transforms, animation tracks, and actual hotspot-to-node indices have not been decoded. The `.grn` files also contain RAD Game Tools / 3D Studio MAX 4.x metadata strings; printable strings alone are not a complete Granny parser.

`proto.xmb` has no animation-related element or attribute names and the unit records do not directly name these scripts. The script-to-unit association here is based on the script filenames and the visual resource names they select; explicit unit-to-script binding remains unverified.

## Port integration gap and implementation path

The current port:

- Opens `data.bar`, `textures.bar`, and `models.bar`, but not `anim.bar` or `models2.bar`.
- Has no animation-script parser, `ASET` model, `.grn` decoder, or unit animation state.
- In `parse_brg`, skips the `ASET` payload and treats each `MESI` as a separate mesh. `Engine::load_models` uploads only the first Hoplite mesh. `IRenderer` exposes mesh creation/destruction but no mesh-buffer update operation, so no frame can currently be played.

The simplest MVP route is to implement the script-to-BRG clip mapping and a bounded BRG frame decoder first; the observed per-frame vertex data can avoid requiring a full Granny skeleton runtime. Before playback, decode `MESI` chunk boundaries and shared/base data exactly, preserve topology, and validate every frame against reference counts and indices. Add `.grn` skeleton playback later if the desired fidelity requires it. Do not copy all of the 100+ MB Granny archives into the APK as a substitute for a decoder.

## Source differences and open questions

- The four MVP scripts exist in both `anim.bar` and `anim2.bar`. `toxotes_anim.txt` and `minotaur_anim.txt` are byte-identical across those archives; Hoplite and Villager scripts differ.
- `VisualGranny` selection precedence relative to `Visual` has not been established.
- The unresolved Villager `RangedWalkAttackA` visuals need a targeted lookup in other asset versions or a runtime/reference check.
- Full BRG vertex attribute layout and Granny hierarchy/track decoding remain open. No animation playback is implemented by this analysis.
