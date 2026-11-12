# FIGHTING COLLECTION – divided by ZERO

A free Windows app to play classic arcade fighting games the way they deserve to be played today.

## Why this project

I have always loved SNK's fighting games, and the Samurai Shodown saga in particular is a masterpiece in so many ways.

But do you know what I could never stand? What I truly hate, the thing that makes my eyes bleed every single time I see it?

Those cursed flickering shadows! Yep, there's a special place in hell reserved for whoever came up with that idea.
<img width="1216" height="896" alt="03 original samsho3 shadows" src="https://github.com/user-attachments/assets/df42ae13-bcf6-48f8-8635-46aad549617d" />

Even worse: the life bars of Samurai Shodown IV. As big as a Cybertruck, neon green, and strobing like nothing you'd see at the worst rave.

Another problem: the games with a camera zoom-out, which, because of the technical limits of the time, robbed those masterpiece sprites of their detail.
<img width="1216" height="896" alt="05 original samsho3 zoomout" src="https://github.com/user-attachments/assets/099e1e7d-e2a9-4fcc-b4cd-7f02c98395c0" />

So I decided to do something about it. The flicker is gone, replaced by solid colours or real transparency.
Zooming is handled in a new way that keeps every detail of the sprites when the camera pulls back 
(until now, only Digital Eclipse’s "Samurai Shodown Neo Geo Collection" had done this, applying it solely to *Samurai Shodown* 1 through 5).
<img width="1920" height="1080" alt="04 zero samsho3 shadows" src="https://github.com/user-attachments/assets/cb992854-6f0d-41a5-88f5-a560bbe765c5" />

<img width="1920" height="1080" alt="06 zero samsho3 zoomout" src="https://github.com/user-attachments/assets/7f74e7e6-a505-42f2-bf00-f10e06ab0417" />

And along the way a few more fixes found their place, from hidden bosses to a fairer CPU, all described below.

You bring your own MAME ROM sets; the app does the rest. Your ROM files are never touched: every fix is applied on the fly when a game starts.

**Free and open.**
The project's own code is released under the MIT license: you are free to use it, modify it, fork it and integrate it into other applications, 
including other front-ends and emulators. Credits (a link back to this repository) are not required by the license, but they are very much appreciated.

## Features

- HD sprite scaling: when the camera zooms out, scaled sprites are redrawn at screen resolution instead of losing rows and columns like on the original hardware.
- Flicker removal: shadows, life bars, weapon trails, character tints and screen strobes that flickered at 30 Hz on the original hardware are steady (important for photosensitive players). Optional semi-transparent shadows, interface and special effects (75% / 50%).
- Unlocked bosses and modes: for example Grant and Kain in Garou, Musashi and Kagami in The Last Blade, EX mode in The Last Blade 2, Omega Rugal in KOF 2002, Alfred in RBFF2. Each game shows a *PATCHES APPLIED* panel explaining what was changed and how to select the hidden characters.
- Gameplay tweaks: e.g. Samurai Shodown III CPU AI that respects the chosen difficulty, no items thrown into the arena; fairer CPU blocking in Kizuna Encounter Bosses and WeaponLord.
- ROM sets recognised by content: zip names do not matter for Neo Geo games; split, merged and non-merged MAME sets all work. *ROM CHECK* lists the sets and BIOS files each game needs and what is missing.
- CRT filter: scanlines, blur, integer scaling, monitor aspect options, automatic black-border cropping.
- ARCADE and VERSUS modes, free play, difficulty, region and button mapping per game, controller and keyboard support.
- Optional download of logos and backgrounds from SteamGridDB (with your own free API key), or use your own images.
<img width="960" height="540" alt="01 FCHD splash screen" src="https://github.com/user-attachments/assets/bb72dc2c-7093-4506-ada8-4ddcd7e46a32" />

<img width="960" height="540" alt="02 FCHD selection screen" src="https://github.com/user-attachments/assets/c5d75503-3e9c-4706-807f-0791e907bd30" />

## Supported games

**Neo Geo (MAME sets):**

- Samurai Shodown I–V and V Special
- Savage Reign
- Kizuna Encounter
- The Last Blade 1 & 2
- Ninja Master's
- Art of Fighting 1–3
- Fatal Fury 1, Special and 3
- Real Bout Fatal Fury 1, Special and 2
- Garou: Mark of the Wolves
- The King of Fighters '94–2003
- Waku Waku 7
- Galaxy Fight
- Rage of the Dragons

**Capcom CPS3:** Red Earth, JoJo's Bizarre Adventure. 

**IGS PGM:** Martial Masters. 

**Psikyo:** Daraku Tenshi – The Fallen Angels. 

**Super Nintendo:** WeaponLord.

The exact set names are listed in the release `README.txt` and in the app (*OPTIONS → ROM CHECK*). No ROMs or BIOS files are included or distributed with this project.


## Download and use

Get the ready-to-use zip from the Releases page, unzip it anywhere, put your MAME ROM sets and BIOS files (`neogeo.zip`, `pgm.zip`) into the `roms` folder and start `FIGHTING COLLECTION.exe`. Full instructions are in the `README.txt` included in the release.

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
make
```

The ready-to-use app is created in `build/FIGHTING COLLECTION/`: `FIGHTING COLLECTION.exe`, `SDL2.dll`, the `assets` folder, the `roms` folder for your ROM sets and an empty `steamgriddb_key.txt`.

Project layout: `src/` front-end, ROM loading (`neorom.c`), patches and options; `core/` Neo Geo core (Geolith, modified for HD rendering); `cps3/`, `pgm/`, `psikyo/`, `snes/` other systems; `patches/`, `asm/` flicker-removal patches and 68000 routines; `blend/` transparency tables; `tools/` maintenance scripts (ROM database generator, decryption reference).

## Licenses

- Project code: MIT – see [`LICENSE`](LICENSE).
- Third-party components keep their own licenses – see [`licenses/README.txt`](licenses/README.txt): Geolith (BSD-3), Musashi (MIT), Jolly Good Z80 (MIT), YMFM (BSD-3), miniz (MIT), Speex (BSD), FinalBurn Neo, LakeSnes (MIT), MAME-derived code (BSD-3), SDL2 (zlib), stb (MIT / public domain), Dosis font (OFL), CRT shader (CC BY-NC-SA 3.0).
- Important: the FinalBurn Neo license is non-commercial. Builds that include the CPS3, PGM, Psikyo or SNES code must stay free of charge, without donation requests, with the source code available. If you only reuse the project's own code (or the Neo Geo part), the MIT license applies.

## Credits

Created by zerowhitemonkey. Built on the work of the Geolith, FinalBurn Neo, MAME, LakeSnes, Musashi, YMFM, SDL and stb authors – thank you.

## How this project was made

All the code in this project was written by Claude, Anthropic's AI model, over many working sessions.

**My part (Zero):**
- the idea and the vision: which games to include, what to fix and how the result should look and feel;
- direction of the work: priorities, design choices, menus, options and every "no, not like this, try again";
- testing: playing the games, spotting bugs, glitches and anything that didn't feel right, and comparing behaviour with the original games;
- providing the material to work with and checking the results on real setups;
- final decisions on everything that went into the release.

**Claude's part:**
- writing all the source code: the front-end, the menus and options, ROM loading and decryption, the cache and the artwork download;
- integrating and adapting the emulation cores;
- analysing the original games' code to find the cause of the flickering, the zoom behaviour and the other issues, and writing the patches that fix them;
- the HD sprite scaling, transparency effects and the CRT filter;
- the build tools, the documentation and the licence checks.

In short: I knew exactly what I wanted to see on screen, and Claude made it happen, one iteration at a time.

This project is not affiliated with SNK, Capcom, IGS, Psikyo, Sega, Nintendo or any other rights holder. All game names and trademarks belong to their respective owners.
