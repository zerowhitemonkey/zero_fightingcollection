# FIGHTING COLLECTION – divided by ZERO

A free Windows app to play classic arcade fighting games the way they deserve to be played today.

## Why this project

I have always loved SNK's fighting games, and the Samurai Shodown saga in particular is a masterpiece in so many ways.

But do you know what I could never stand? What I truly hate, the thing that makes my eyes bleed every single time I see it?

**Those cursed flickering shadows.** There is a circle of hell reserved for whoever came up with that idea.

Even worse: the life bars of Samurai Shodown IV. As big as a Cybertruck, neon green, and strobing like nothing you'd see at the worst rave.

And one last thing: the games with a camera zoom-out, which, because of the technical limits of the time, robbed those masterpiece sprites of their detail.

So I decided to do something about it. The flicker is gone, replaced by solid colours or real transparency. Zooming is handled in a new way that keeps every detail of the sprites when the camera pulls back. And along the way a few more fixes found their place, from hidden bosses to a fairer CPU, all described below.

You bring your own MAME ROM sets; the app does the rest. Your ROM files are never touched: every fix is applied on the fly when a game starts.

> **Free and open.** The project's own code is released under the MIT license: you are free to use it, modify it, fork it and integrate it into other applications, including other front-ends and emulators. Credits (a link back to this repository) are not required by the license, but they are very much appreciated.

## Highlights

- **HD sprite scaling** – when the camera zooms out, scaled sprites are redrawn at screen resolution instead of losing rows and columns like on the original hardware.
- **Flicker removal** – shadows, life bars, weapon trails, character tints and screen strobes that flickered at 30 Hz on the original hardware are steady (important for photosensitive players). Optional **semi-transparent shadows, interface and special effects** (75% / 50%).
- **Unlocked bosses and modes** – for example Grant and Kain in Garou, Musashi and Kagami in The Last Blade, EX mode in The Last Blade 2, Omega Rugal in KOF 2002, Alfred in RBFF2. Each game shows a *PATCHES APPLIED* panel explaining what was changed and how to select the hidden characters.
- **Gameplay fixes** – e.g. Samurai Shodown III CPU AI that respects the chosen difficulty, no items thrown into the arena; fairer CPU blocking in Kizuna Encounter and WeaponLord.
- **ROM sets recognised by content** – zip names do not matter for Neo Geo games; split, merged and non-merged MAME sets all work. *ROM CHECK* lists the sets and BIOS files each game needs and what is missing.
- **CRT filter** (scanlines, blur), integer scaling, monitor aspect options, automatic black-border cropping.
- ARCADE and VERSUS modes, free play, difficulty, region and button mapping per game, controller and keyboard support.
- Optional download of logos and backgrounds from **SteamGridDB** (with your own free API key), or use your own images.

## Supported games

**Neo Geo (MAME sets):** Samurai Shodown I–V and V Special, Savage Reign, Kizuna Encounter, The Last Blade 1 & 2, Ninja Master's, Art of Fighting 1–3, Fatal Fury 1, Special and 3, Real Bout Fatal Fury 1, Special and 2, Garou: Mark of the Wolves, The King of Fighters '94–2003, Waku Waku 7, Galaxy Fight, Rage of the Dragons.
**Capcom CPS3:** Red Earth, JoJo's Bizarre Adventure. **IGS PGM:** Martial Masters. **Psikyo:** Daraku Tenshi – The Fallen Angels. **Super Nintendo:** WeaponLord.

The exact set names are listed in the release `README.txt` and in the app (*OPTIONS → ROM CHECK*). No ROMs or BIOS files are included or distributed with this project.

## Download and use

Get the ready-to-use zip from the **Releases** page, unzip it anywhere, put your MAME ROM sets and BIOS files (`neogeo.zip`, `pgm.zip`) into the `roms` folder and start `FIGHTING COLLECTION.exe`. Full instructions are in the `README.txt` included in the release.

Requirements: Windows 10/11 64-bit, Direct3D 11. A game controller is recommended.

## Building from source

The Windows executable is cross-compiled on Linux with MinGW-w64.

```sh
sudo apt install mingw-w64 make python3
mkdir -p ext && cd ext
curl -LO https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-devel-2.30.9-mingw.tar.gz
tar xzf SDL2-devel-2.30.9-mingw.tar.gz
for f in stb_image.h stb_truetype.h stb_image_write.h; do curl -LO https://raw.githubusercontent.com/nothings/stb/master/$f; done
cd ..
make build/SNKHD.exe
```

Rename `build/SNKHD.exe` to `FIGHTING COLLECTION.exe` and place it next to `SDL2.dll` (from `ext/SDL2-2.30.9/x86_64-w64-mingw32/bin`) and the `app/assets` folder copied as `assets`.

Project layout: `src/` front-end, ROM loading (`neorom.c`), patches and options; `core/` Neo Geo core (Geolith, modified for HD rendering); `cps3/`, `pgm/`, `psikyo/`, `snes/` other systems; `patches/`, `asm/` flicker-removal patches and 68000 routines; `blend/` transparency tables; `tools/` maintenance scripts (ROM database generator, decryption reference).

## Licenses

- **Project code**: MIT – see [`LICENSE`](LICENSE).
- **Third-party components** keep their own licenses – see [`licenses/README.txt`](licenses/README.txt): Geolith (BSD-3), Musashi (MIT), Jolly Good Z80 (MIT), YMFM (BSD-3), miniz (MIT), Speex (BSD), FinalBurn Neo, LakeSnes (MIT), MAME-derived code (BSD-3), SDL2 (zlib), stb (MIT / public domain), Dosis font (OFL), CRT shader (CC BY-NC-SA 3.0).
- **Important:** the FinalBurn Neo license is non-commercial. Builds that include the CPS3, PGM, Psikyo or SNES code must stay free of charge, without donation requests, with the source code available. If you only reuse the project's own code (or the Neo Geo part), the MIT license applies.

## Credits

Created by zerowhitemonkey. Built on the work of the Geolith, FinalBurn Neo, MAME, LakeSnes, Musashi, YMFM, SDL and stb authors – thank you.

This project is not affiliated with SNK, Capcom, IGS, Psikyo, Sega, Nintendo or any other rights holder. All game names and trademarks belong to their respective owners.
