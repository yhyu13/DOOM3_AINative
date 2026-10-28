# AIGC vision assets

Generated with **GPT Image 2.5** through the local OpenAI-compatible proxy:

- Base URL: `https://llm-proxy.tapsvc.com/v1`
- Model: `openai/gpt-image-2.5-flare`
- Page 2: `POST /v1/images/edits` (image input)
- Page 3: `POST /v1/images/generations` (text only)
- Generated: 2026-10-28

Both files were cropped to 16:9 and resized to 1600×900 JPEG (quality 86) so the single-file deck
stays under 4 MiB. They are inlined by `showcase/build.py`; `--no-aigc` restores the procedural
corridor/network fallback.

## Page 2 — `page2-modern.jpg` (image-to-image)

**Input:** the official DOOM 3 promotional screenshot from the Steam store page (app 9050,
© id Software / Bethesda / ZeniMax). It was downloaded to the operator's temp directory for the
edit; **neither the original nor the unredacted input is in this repository.**

**Prompt:**

> Reimagine this exact DOOM 3 first-person frame as a 2026 AAA FPS in-game shot. Preserve the camera
> angle, the imp demon in the center, the UAC Mars industrial corridor, and the shotgun in the lower
> right. Modern physically based rendering, volumetric haze, HDR, ray-traced reflections on wet
> metal, fine rivets, warm emergency lights and cyan accents, cinematic contrast, highly detailed.
> No text, no logos, no watermark, no HUD overlay, no extra UI, no menu.

**Status:** derivative of the official promo art; **licensing pending**, not a licensed release.
It is in the public repository at the owner's direction. It is a concept vision, not a game capture;
replace it with a licensed/cleared asset if a cleared distribution is required (T042 / issue #2).

## Page 3 — `page3-vision.jpg` (text-to-image)

**Prompt:**

> A cinematic 2026 AI-native game command center on Mars. A large holographic 3D level map of an
> industrial base with rooms, corridors, a reactor and a hell gate; glowing AI route lines; a prompt
> console; three floating translucent panels; cyan and ember lighting; dark metal, volumetric dust,
> depth of field. No text, no logos, no watermark, no HUD overlay, 16:9 widescreen.

**Status:** text-to-image with no game-art input; original AI-generated concept art. Still labelled
`AI-GENERATED VISION · NOT A GAME CAPTURE` in the deck.

## Regeneration

The generation used the local proxy token from the operator's Claude/Kilo settings; no key is stored
in this repository. The page-2 edit needs the original promo image as a local input. The page-3
prompt is text-only. The assets are compressed with Pillow (`ImageOps.fit`, 1600×900, JPEG q86).
