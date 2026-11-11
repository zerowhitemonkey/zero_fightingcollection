THIRD-PARTY COMPONENTS AND LICENSES
===================================

Component                         Where                                   License file
--------------------------------  --------------------------------------  ------------------------------
Geolith (Neo Geo core)            core/                                   Geolith-BSD-3-Clause.txt
  Musashi 68000                   core/m68k/                              Musashi-MIT.txt
  Jolly Good Z80                  core/z80/                               JollyGoodZ80-MIT.txt
  YMFM-C (YM2610)                 core/ymfm/                              YMFM-C-BSD-3-Clause.txt
  Speex resampler                 core/deps/speex/                        Speex-BSD.txt
  miniz                           core/deps/miniz/                        miniz-MIT.txt
FinalBurn Neo (CPS3, PGM, Psikyo) cps3/, pgm/fbn/, psikyo/, blend/fbneo/  FBNeo-license.txt
ymfm (YMF278B, Psikyo)            psikyo/ymfm/                            ymfm-BSD-3-Clause.txt
LakeSnes / FBNeo SNES core        snes/                                   LakeSnes-MIT.txt
MAME (Neo Geo decryption, Psikyo  src/neorom.c, src/neocrypt_tables.h,    MAME-BSD-3-Clause.txt
  video semantics)                tools/neocrypt.py, psikyo/
CRT filter (frutbunn)             src/gfxb_d3d11.c, src/gfxb_gl.c         CRT-shader-notice.txt
SDL2 (runtime DLL, headers)       SDL2.dll / build dependency             SDL2-zlib.txt
stb_image, stb_truetype,          build dependency (ext/)                 stb-MIT-or-PublicDomain.txt
  stb_image_write
Dosis font                        app/assets/fonts/                       Dosis-OFL.txt

IMPORTANT: the FinalBurn Neo license is NON-COMMERCIAL. Any build that includes the CPS3, PGM,
Psikyo or SNES code must be distributed free of charge, without asking for donations, with the
source code available and with the full FBNeo license text included. Because of this, no GPL code
may be added to the same executable (the GPL is incompatible with non-commercial terms).
The project's own code (see LICENSE in the root folder) is MIT and can be reused anywhere.
