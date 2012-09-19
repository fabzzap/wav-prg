wav2prg: wav2prg_core.o \
         wav2prg.o \
         loaders.o \
         observers.o \
         get_pulse.o \
         write_cleaned_tap.o \
         write_prg.o \
         yet_another_getopt.o \
         block_list.o \
         create_t64.o \
         audiotap_interface.o

wav2prg.exe: wav2prg_core.o \
             wav2prg.o \
             loaders.o \
             observers.o \
             get_pulse.o \
             write_cleaned_tap.o \
             write_prg.o \
             yet_another_getopt.o \
             block_list.o \
             create_t64.o \
             audiotap_interface.o \
             name_utils.o
	$(LINK.o) $^ $(LDLIBS) -o $@

wavprg.exe:LDLIBS+=-lcomdlg32 -lole32
wavprg.exe:LDFLAGS+=-mwindows
wavprg.exe: wav2prg_core.o \
            loaders.o \
            observers.o \
            get_pulse.o \
            write_cleaned_tap.o \
            write_prg.o \
            yet_another_getopt.o \
            block_list.o \
            create_t64.o \
            audiotap_interface.o \
            t64utils.o \
            name_utils.o \
            prg2wav_core.o \
            prg2wav_utils.o \
            windows_gui/wav2prg_gui.o \
            windows_gui/prg2wav_gui.o \
            windows_gui/wavprg.o \
            windows_gui/wavprg-resources.o
	$(LINK.o) $^ $(LDLIBS) -o $@

windows_gui/wavprg-resources.o: windows_gui/wavprg.rc
	$(WINDRES) --include=windows_gui -o $@ $^

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

