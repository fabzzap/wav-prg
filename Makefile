cmdline/wav2prg: wav2prg_core/wav2prg_core.o \
         wav2prg_core/loaders.o \
         wav2prg_core/observers.o \
         wav2prg_core/get_pulse.o \
         wav2prg_core/write_cleaned_tap.o \
         wav2prg_core/write_prg.o \
         wav2prg_core/wav2prg_block_list.o \
         wav2prg_core/create_t64.o \
         wav2prg_core/audiotap_interface.o \
         common_core/name_utils.o \
         common_core/yet_another_getopt.o \
         cmdline/wav2prg.o

wav2prg.exe: wav2prg_core/wav2prg_core.o \
         cmdline/wav2prg.o \
         wav2prg_core/loaders.o \
         wav2prg_core/observers.o \
         wav2prg_core/get_pulse.o \
         wav2prg_core/write_cleaned_tap.o \
         wav2prg_core/write_prg.o \
         wav2prg_core/wav2prg_block_list.o \
         wav2prg_core/create_t64.o \
         wav2prg_core/audiotap_interface.o \
         common_core/name_utils.o \
         common_core/yet_another_getopt.o
	$(LINK.o) $^ $(LDLIBS) -o $@

wavprg.exe:LDLIBS+=-lcomdlg32 -lole32
wavprg.exe:LDFLAGS+=-mwindows
wavprg.exe: wav2prg_core/wav2prg_core.o \
            wav2prg_core/loaders.o \
            wav2prg_core/observers.o \
            wav2prg_core/get_pulse.o \
            wav2prg_core/write_cleaned_tap.o \
            wav2prg_core/write_prg.o \
            wav2prg_core/wav2prg_block_list.o \
            wav2prg_core/create_t64.o \
            wav2prg_core/audiotap_interface.o \
            prg2wav_core/t64utils.o \
            prg2wav_core/prg2wav_core.o \
            prg2wav_core/prg2wav_utils.o \
            common_core/block_list.o \
            common_core/name_utils.o \
            windows_gui/wav2prg_gui.o \
            windows_gui/prg2wav_gui.o \
            windows_gui/wavprg.o \
            windows_gui/wavprg-resources.o
	$(LINK.o) $^ $(LDLIBS) -o $@

windows_gui/wavprg-resources.o: windows_gui/wavprg.rc
	$(WINDRES) --include=windows_gui -o $@ $^

CFLAGS=-I common_core

cmdline/wav2prg.o:CFLAGS+=-I wav2prg_core
wav2prg_core/%.o:CFLAGS+=-I wav2prg_core
windows_gui/%.o:CFLAGS+=-I wav2prg_core -I prg2wav_core

ifdef AUDIOTAP_HDR
  CFLAGS+=-I $(AUDIOTAP_HDR)
endif
ifdef DEBUG
  CFLAGS+=-g
endif

LDLIBS=-laudiotap
ifdef AUDIOTAP_LIB
  LDLIBS+=-L$(AUDIOTAP_LIB)
endif

ifdef USE_RPATH
  ifneq ($(AUDIOTAP_LIB),)
    WAV2PRG_RPATH=$(AUDIOTAP_LIB)
    ifneq ($(TAPENCODER_LIB),)
      WAV2PRG_RPATH := $(WAV2PRG_RPATH):$(TAPENCODER_LIB)
    endif
  else
    WAV2PRG_RPATH=$(TAPENCODER_LIB)
  endif
  ifneq ($(WAV2PRG_RPATH),)
    LDLIBS+=-Wl,-rpath=$(WAV2PRG_RPATH)
  endif
endif

wav2prg:LDLIBS+=-ldl

clean:
	rm -f common_core/*.o wav2prg_core/*.o prg2wav_core/*.o windows_gui/*.o cmdline/*.o \
	common_core/*~ wav2prg_core/*~ prg2wav_core/*~ windows_gui/*~ cmdline/*~

