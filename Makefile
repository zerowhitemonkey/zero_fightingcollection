CORE := core
CORE_SRC := $(addprefix $(CORE)/,geo.c geo_cd.c geo_disc.c geo_lc8951.c geo_lspc.c geo_m68k.c geo_memcard.c geo_mixer.c geo_neo.c geo_rtc.c geo_serial.c geo_vfs.c geo_ymfm.c geo_z80.c m68k/m68kcpu.c m68k/m68kops.c ymfm/ymfm_adpcm.c ymfm/ymfm_opn.c ymfm/ymfm_ssg.c z80/z80.c deps/miniz/miniz.c deps/speex/resample.c)
CFLAGS_CORE := -O3 -I$(CORE) -I$(CORE)/deps -I$(CORE)/deps/miniz -I$(CORE)/deps/speex -DFLOATING_POINT -DEXPORT=
BUILD := build

# core CPS3 (driver FBNeo adattato, C++ senza eccezioni/RTTI)
CPS3_SRC := cps3/cps3.cpp cps3/sh2.cpp
CXXFLAGS_CPS3 := -O3 -fno-exceptions -fno-rtti -Icps3 -I$(CORE)/deps/miniz -Wno-unused-function

# core PSIKYO (driver FBNeo "psikyosh" adattato, Daraku Tenshi): riusa il core SH-2 di cps3/
PSIKYO_SRC := psikyo/psikyo.cpp psikyo/ymfm/ymfm_opl.cpp psikyo/ymfm/ymfm_pcm.cpp psikyo/ymfm/ymfm_adpcm.cpp
CXXFLAGS_PSIKYO := -O3 -fno-exceptions -fno-rtti -Ipsikyo -Icps3 -I$(CORE)/deps/miniz -Wno-unused-function

# core SNES (LakeSnes/FBNeo, MIT): driver sostituito da snes_core.cpp, coprocessori/MSU1 come stub
SNES_SRC := $(addprefix snes/,apu.cpp cart.cpp cpu.cpp cpu_sa1.cpp cx4.cpp dma.cpp dsp.cpp epsonrtc.cpp glue.cpp gsu.cpp input.cpp msu1.cpp msu1_backend.cpp ppu.cpp sa1.cpp sdd1.cpp snes.cpp snes_other.cpp spc.cpp spc7110.cpp st018.cpp statehandler.cpp snes_core.cpp)
CXXFLAGS_SNES := -O3 -fno-exceptions -fno-rtti -Isnes/shim -Isnes -I$(CORE)/deps/miniz -w

include pgm/pgm.mk
# core ST-V (Mednafen, GPL-2) escluso dalla versione distribuibile: src/stv_stub.c
LIN_OBJ := $(BUILD)/lin/pgm_all.o $(patsubst %.c,$(BUILD)/lin/%.o,$(CORE_SRC)) $(patsubst %.cpp,$(BUILD)/lin/%.o,$(CPS3_SRC)) $(patsubst %.cpp,$(BUILD)/lin/%.o,$(PSIKYO_SRC)) $(patsubst %.cpp,$(BUILD)/lin/%.o,$(SNES_SRC))
WIN_OBJ := $(BUILD)/win/pgm_all.o $(patsubst %.c,$(BUILD)/win/%.o,$(CORE_SRC)) $(patsubst %.cpp,$(BUILD)/win/%.o,$(CPS3_SRC)) $(patsubst %.cpp,$(BUILD)/win/%.o,$(PSIKYO_SRC)) $(patsubst %.cpp,$(BUILD)/win/%.o,$(SNES_SRC))

$(BUILD)/lin/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(CFLAGS_CORE) -c $< -o $@
$(BUILD)/win/%.o: %.c
	@mkdir -p $(dir $@)
	x86_64-w64-mingw32-gcc $(CFLAGS_CORE) -c $< -o $@
$(BUILD)/lin/cps3/%.o: cps3/%.cpp cps3/cps3.h cps3/cps3_video.inc cps3/burnint.h
	@mkdir -p $(dir $@)
	g++ $(CXXFLAGS_CPS3) -c $< -o $@
$(BUILD)/win/cps3/%.o: cps3/%.cpp cps3/cps3.h cps3/cps3_video.inc cps3/burnint.h
	@mkdir -p $(dir $@)
	x86_64-w64-mingw32-g++ $(CXXFLAGS_CPS3) -c $< -o $@
$(BUILD)/lin/psikyo/%.o: psikyo/%.cpp psikyo/psikyo.h psikyo/psikyo_video.inc cps3/burnint.h cps3/sh2_intf.h $(wildcard psikyo/ymfm/*.h)
	@mkdir -p $(dir $@)
	g++ $(CXXFLAGS_PSIKYO) -c $< -o $@
$(BUILD)/win/psikyo/%.o: psikyo/%.cpp psikyo/psikyo.h psikyo/psikyo_video.inc cps3/burnint.h cps3/sh2_intf.h $(wildcard psikyo/ymfm/*.h)
	@mkdir -p $(dir $@)
	x86_64-w64-mingw32-g++ $(CXXFLAGS_PSIKYO) -c $< -o $@

$(BUILD)/lin/snes/%.o: snes/%.cpp $(wildcard snes/*.h) snes/shim/upd7725.h
	@mkdir -p $(dir $@)
	g++ $(CXXFLAGS_SNES) -c $< -o $@
$(BUILD)/win/snes/%.o: snes/%.cpp $(wildcard snes/*.h) snes/shim/upd7725.h
	@mkdir -p $(dir $@)
	x86_64-w64-mingw32-g++ $(CXXFLAGS_SNES) -c $< -o $@

# banco di prova SNES (core da solo)
$(BUILD)/snkhd_snes: src/test_snes.c $(BUILD)/libcore_lin.a
	gcc -no-pie -O2 $(CFLAGS_CORE) -Isnes -Iext src/test_snes.c $(BUILD)/libcore_lin.a -lstdc++ -lm -o $@

$(BUILD)/libcore_lin.a: $(LIN_OBJ)
	ar rcs $@ $^
$(BUILD)/libcore_win.a: $(WIN_OBJ)
	x86_64-w64-mingw32-ar rcs $@ $^

$(BUILD)/snkhd_test: src/test.c src/emu.c src/softdip.c src/rompatch.c src/visual.c src/stv_stub.c $(BUILD)/libcore_lin.a
	gcc -no-pie -O2 $(CFLAGS_CORE) src/test.c src/emu.c src/softdip.c src/rompatch.c src/visual.c src/stv_stub.c $(BUILD)/libcore_lin.a -lstdc++ -lm -o $@

# banco di prova CPS3 (core) e percorso dell'app per i giochi CPS3 (emu.c + cps3cfg + visual)
# banco di prova PSIKYO (core da solo, Daraku Tenshi), vedi src/test_psikyo.c
$(BUILD)/snkhd_psikyo: src/test_psikyo.c $(BUILD)/libcore_lin.a
	gcc -no-pie -O2 $(CFLAGS_CORE) src/test_psikyo.c $(BUILD)/libcore_lin.a -lstdc++ -lm -o $@
$(BUILD)/snkhd_cps3: src/test_cps3.c $(BUILD)/libcore_lin.a
	gcc -no-pie -O2 $(CFLAGS_CORE) src/test_cps3.c $(BUILD)/libcore_lin.a -lm -o $@
$(BUILD)/snkhd_appc3: src/test_appc3.c src/emu.c src/stv_stub.c src/cps3cfg.c src/visual.c src/rompatch.c src/softdip.c $(BUILD)/libcore_lin.a
	gcc -no-pie -O2 $(CFLAGS_CORE) src/test_appc3.c src/emu.c src/stv_stub.c src/cps3cfg.c src/visual.c src/rompatch.c src/softdip.c $(BUILD)/libcore_lin.a -lstdc++ -lm -o $@
# percorso dell'app per i giochi Psikyo (emu.c + psikyocfg + visual), vedi src/test_apppk.c
$(BUILD)/snkhd_pgm: src/test_pgm.c $(BUILD)/libcore_lin.a
	gcc -no-pie -O2 $(CFLAGS_CORE) src/test_pgm.c $(BUILD)/libcore_lin.a -lstdc++ -lm -o $@
$(BUILD)/snkhd_apppk: src/test_apppk.c src/emu.c src/stv_stub.c src/psikyocfg.c src/visual.c src/rompatch.c src/softdip.c $(BUILD)/libcore_lin.a
	gcc -no-pie -O2 $(CFLAGS_CORE) src/test_apppk.c src/emu.c src/stv_stub.c src/psikyocfg.c src/visual.c src/rompatch.c src/softdip.c $(BUILD)/libcore_lin.a -lstdc++ -lm -o $@

APP_COMMON := src/app.c src/gfx.c src/audio.c src/input.c src/settings.c src/softdip.c src/rompatch.c src/emu.c src/visual.c src/cps3cfg.c src/psikyocfg.c src/pgmcfg.c src/stvcfg.c src/stv_stub.c src/neorom.c src/artdl.c
APP_SRC := $(APP_COMMON) src/gfxb_gl.c src/glfn.c
APP_WIN := $(APP_COMMON) src/gfxb_d3d11.c src/extcap.c
SDLW := ext/SDL2-2.30.9/x86_64-w64-mingw32

$(BUILD)/snkhd_lin: $(APP_SRC) $(BUILD)/libcore_lin.a
	gcc -no-pie -O2 -Wall -Wno-unused-function $(CFLAGS_CORE) $(shell sdl2-config --cflags) $(APP_SRC) $(BUILD)/libcore_lin.a $(shell sdl2-config --libs) -lstdc++ -lpthread -lm -o $@

$(BUILD)/SNKHD.exe: $(APP_WIN) $(BUILD)/libcore_win.a $(BUILD)/icon.o
	x86_64-w64-mingw32-gcc -O2 -Wall -Wno-unused-function $(CFLAGS_CORE) -I$(SDLW)/include/SDL2 -Isnes -Dmain=SDL_main $(APP_WIN) $(BUILD)/icon.o $(BUILD)/libcore_win.a -L$(SDLW)/lib -lmingw32 -lSDL2main -lSDL2 -lpsapi -ld3d11 -ldxgi -ldxguid -luuid -lws2_32 -lwinhttp -lwinmm -mwindows -Wl,-Bstatic -lstdc++ -Wl,-Bdynamic -lm -static-libgcc -static-libstdc++ -o $@

$(BUILD)/icon.o: app/icon.rc app/icon.ico
	x86_64-w64-mingw32-windres app/icon.rc -O coff -o $@

# banco di prova di debug con hook istruzioni (BRK=addr,...)
DBG_OBJ := $(patsubst %.c,$(BUILD)/dbg/%.o,$(CORE_SRC))
$(BUILD)/dbg/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(CFLAGS_CORE) -DSNKHD_HOOK -c $< -o $@
$(BUILD)/snkhd_dbg: src/test.c src/emu.c src/stv_stub.c src/softdip.c src/rompatch.c src/visual.c $(BUILD)/lin/pgm_all.o $(DBG_OBJ) $(patsubst %.cpp,$(BUILD)/lin/%.o,$(CPS3_SRC)) $(patsubst %.cpp,$(BUILD)/lin/%.o,$(SNES_SRC)) $(patsubst %.cpp,$(BUILD)/lin/%.o,$(PSIKYO_SRC))
	gcc -O2 $(CFLAGS_CORE) -DSNKHD_HOOK src/test.c src/emu.c src/stv_stub.c src/softdip.c src/rompatch.c src/visual.c $(BUILD)/lin/pgm_all.o $(DBG_OBJ) $(patsubst %.cpp,$(BUILD)/lin/%.o,$(CPS3_SRC)) $(patsubst %.cpp,$(BUILD)/lin/%.o,$(SNES_SRC)) $(patsubst %.cpp,$(BUILD)/lin/%.o,$(PSIKYO_SRC)) -lstdc++ -lm -o $@
# percorso dell'app per i giochi PGM (emu.c + pgmcfg + visual), vedi src/test_apppg.c
$(BUILD)/snkhd_apppg: src/test_apppg.c src/emu.c src/stv_stub.c src/pgmcfg.c src/visual.c src/rompatch.c src/softdip.c $(BUILD)/libcore_lin.a
	gcc -no-pie -O2 $(CFLAGS_CORE) src/test_apppg.c src/emu.c src/stv_stub.c src/pgmcfg.c src/visual.c src/rompatch.c src/softdip.c $(BUILD)/libcore_lin.a -lstdc++ -lm -o $@
