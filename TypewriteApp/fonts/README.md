# Fonts for Typewrite OS (UEFI bitmaps)

Glyphs are rasterized at **24px** with [`convert_font.py`](convert_font.py) (FreeType) into C headers (`*.h` + `*.bin`).

## Bundled typefaces

| Key order (F2) | Header / prefix | Source file | License | Notes |
|------------------|-----------------|-------------|---------|--------|
| 1 Virgil | `virgil.h` | `virgil_fixed.ttf` (repo) | project font | Hand-drawn |
| 2 Inter (“Helvetica” in UI) | `helvetica.h` | `inter.ttf` | SIL OFL 1.1 | Sans UI |
| 3 **Special Elite** | `special_elite.h` | [Google Fonts — Special Elite](https://fonts.google.com/specimen/Special+Elite) | Apache 2.0 | Distressed **typewriter** |
| 4 **Courier Prime** | `courier_prime.h` | [CourierPrime on GitHub](https://github.com/quoteunquoteapps/CourierPrime) | SIL OFL 1.1 | Screenplay / **typewriter** mono |
| 5 **VT323** | `vt323.h` | [Google Fonts — VT323](https://fonts.google.com/specimen/VT323) | SIL OFL 1.1 | Retro **DOS / terminal** |
| 6 **Press Start 2P** | `press_start_2p.h` | [Google Fonts — Press Start 2P](https://fonts.google.com/specimen/Press+Start+2P) | SIL OFL 1.1 | **8-bit arcade** pixel |
| 7 **IBM Plex Mono** | `ibm_plex_mono.h` | [Google Fonts — IBM Plex Mono](https://fonts.google.com/specimen/IBM+Plex+Mono) | SIL OFL 1.1 | **IBM** corporate mono |
| 8 **Share Tech Mono** | `share_tech_mono.h` | [Google Fonts — Share Tech Mono](https://fonts.google.com/specimen/Share+Tech+Mono) | SIL OFL 1.1 | Retro **sci-fi** mono |
| 9 Simple | — | Built into `main.c` | — | 5×8 grid |

Google Fonts files were taken from the [google/fonts](https://github.com/google/fonts) repository (Apache / OFL directories). Courier Prime from the **quoteunquoteapps/CourierPrime** release tree.

Full license texts: see each upstream project; OFL and Apache 2.0 permit redistribution when fonts are not sold alone.

## Regenerating headers

```bash
cd fonts
python3 convert_font.py virgil_fixed.ttf virgil --size 24
python3 convert_font.py inter.ttf helvetica --size 24
# …same pattern for other *.ttf and output base names matching *_*.h
```
