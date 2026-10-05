# Original admitted item descriptions

The battle Items menu and Minnie Storage consume an independent checked
`data/opening.encdetails` resource. It covers the admitted BaseballCap and
AsthmaSpray descriptions in English and Simplified Chinese, actual session
nickname, original Asthma image, current spray doses and source item value.
This adds presentation, not item-use/status-healing actions or new inventory IDs.
Original Items capability flags and the save schema remain unchanged.

`tools/item_details.py` extracts pinned source/translation/Description/TextTools
semantics into offline IR, converts the original image through real tex3ds,
compiles ENCITD01 and checks its pack/receipt before staging. JSON and source
receipts stay outside RomFS. Stable catalog role 29 resolves the pack; the
shared loader checks sections, capability, source identity, tokens and bounds,
then binds the existing Items definitions. The 3DS image admission validates
converted texture length/CRC, dimensions and format before GPU-frame use.

Only the existing source font catalog is used: both supported languages already
cover the required description/dose scalars, including NBSP and em dash. The
composer consumes actual font measurements and clips at the existing panels;
no font or replacement art is taken from the developer environment.

Adaptations are explicit: the pinned TextTools path spells the Asthma file in
lowercase, while its real source filename is titlecase; the unique source match
is validated. Chinese empty-separator wrapping keeps each image as one typed
atom measured by the original two-em-dash substitute, rather than splitting
raw BBCode tags. Thus exact Chinese line-break parity with that source defect
is not claimed. Bitmap-font image baseline follows the source zero descent.

Manual parser/resource/composition positive and negative cases are retained.
Routine test suites/sanitizers remain disabled. Cross-build/package results,
emulator display and physical-device acceptance are separate evidence levels.
