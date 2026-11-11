# core PGM (driver FBNeo "pgm" + CPU FBNeo) -> un solo oggetto con i soli simboli pgm_* globali
PGM_CXX_SRC := pgm/pgm_core.cpp $(addprefix pgm/fbn/,m68000_intf.cpp z80_intf.cpp arm7_intf.cpp arm7/arm7.cpp ics2115.cpp v3021.cpp timer.cpp z80/z80.cpp joyprocess.cpp pgm_run.cpp pgm_crypt.cpp pgm_asic27a_type2.cpp)
PGM_C_SRC := pgm/fbn/m68k/m68kcpu.c pgm/fbn/m68k/m68kops.c
PGM_FLAGS := -O2 -w -DLSB_FIRST -Ipgm -Ipgm/fbn -Ipgm/fbn/m68k -Ipgm/fbn/z80 -Ipgm/fbn/arm7 -I$(CORE)/deps/miniz
PGM_HDR := $(wildcard pgm/*.h pgm/*.inc pgm/fbn/*.h)

$(BUILD)/lin/pgm/%.o: pgm/%.cpp $(PGM_HDR)
	@mkdir -p $(dir $@)
	g++ $(PGM_FLAGS) -fno-rtti -c $< -o $@
$(BUILD)/lin/pgm/%.o: pgm/%.c $(PGM_HDR)
	@mkdir -p $(dir $@)
	gcc $(PGM_FLAGS) -c $< -o $@
$(BUILD)/win/pgm/%.o: pgm/%.cpp $(PGM_HDR)
	@mkdir -p $(dir $@)
	x86_64-w64-mingw32-g++ $(PGM_FLAGS) -mcmodel=small -fno-rtti -c $< -o $@
$(BUILD)/win/pgm/%.o: pgm/%.c $(PGM_HDR)
	@mkdir -p $(dir $@)
	x86_64-w64-mingw32-gcc $(PGM_FLAGS) -mcmodel=small -c $< -o $@

PGM_LIN_PARTS := $(patsubst pgm/%.cpp,$(BUILD)/lin/pgm/%.o,$(PGM_CXX_SRC)) $(patsubst pgm/%.c,$(BUILD)/lin/pgm/%.o,$(PGM_C_SRC))
PGM_WIN_PARTS := $(patsubst pgm/%.cpp,$(BUILD)/win/pgm/%.o,$(PGM_CXX_SRC)) $(patsubst pgm/%.c,$(BUILD)/win/pgm/%.o,$(PGM_C_SRC))

$(BUILD)/pgm_keep.txt: pgm/pgmcore.h
	@mkdir -p $(BUILD)
	grep -o 'pgm_[a-z0-9_]*(' pgm/pgmcore.h | tr -d '(' | sort -u > $@
$(BUILD)/lin/pgm_all.o: $(PGM_LIN_PARTS) $(BUILD)/pgm_keep.txt
	ld -r $(PGM_LIN_PARTS) -o $@.tmp
	objcopy -w --keep-global-symbols=$(BUILD)/pgm_keep.txt $@.tmp $@
	rm -f $@.tmp
$(BUILD)/win/pgm_all.o: $(PGM_WIN_PARTS) $(BUILD)/pgm_keep.txt
	x86_64-w64-mingw32-ld -r $(PGM_WIN_PARTS) -o $@.tmp
	# funzioni inline in sezioni COMDAT (es. printf di mingw): restano globali, altrimenti il linker scarta le copie degli altri oggetti
	{ cat $(BUILD)/pgm_keep.txt; x86_64-w64-mingw32-objdump -h $@.tmp | awk '{print $$2}' | grep '^\.text\$$' | sed 's/^\.text\$$//'; } > $@.keep
	x86_64-w64-mingw32-objcopy -w --keep-global-symbols=$@.keep $@.tmp $@
	rm -f $@.keep
	rm -f $@.tmp
