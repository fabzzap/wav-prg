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

cmdline/wav2prg wav2prg.exe: wav2prg_core/wav2prg_core.o \
         wav2prg_core/loaders.o \
         wav2prg_core/observers.o \
         wav2prg_core/get_pulse.o \
         wav2prg_core/write_cleaned_tap.o \
         wav2prg_core/write_prg.o \
         wav2prg_core/wav2prg_block_list.o \
         wav2prg_core/create_t64.o \
         wav2prg_core/audiotap_interface.o \
         common_core/name_utils.o \
         cmdline/yet_another_getopt.o \
         cmdline/wav2prg.o

cmdline/prg2wav prg2wav.exe: prg2wav_core/prg2wav_core.o \
         prg2wav_core/t64utils.o \
         prg2wav_core/prg2wav_utils.o \
         common_core/name_utils.o \
         common_core/block_list.o \
         cmdline/yet_another_getopt.o \
         cmdline/progressmeter.o \
         cmdline/process_input_files.o \
         cmdline/prg2wav.o

wav2prg.exe prg2wav.exe:
	$(LINK.o) $^ $(LDLIBS) -o $@

WINDRES=windres

wavprg.exe:LDLIBS+=-lcomdlg32 -lole32
wavprg.exe:LDFLAGS+=-mwindows
windows_gui/wavprg-resources.o: windows_gui/wavprg.rc
	$(WINDRES) --include=common_core -o $@ $^
ifdef HTMLHELP
  windows_gui/%.o:CFLAGS += -DHAVE_HTMLHELP -I"$(HTMLHELP)/include"
  wavprg.exe:LDLIBS += -lhtmlhelp -L"$(HTMLHELP)/lib"
endif

CFLAGS=-I common_core

cmdline/wav2prg.o:CFLAGS+=-I wav2prg_core
cmdline/prg2wav.o cmdline/process_input_files.o cmdline/progressmeter.o:CFLAGS+=-I prg2wav_core
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
    LDLIBS+=-Wl,-rpath=$(AUDIOTAP_LIB)
  endif
endif

ifdef ENABLE_PAUSE
  cmdline/prg2wav.o:CFLAGS+=-DENABLE_PAUSE
  cmdline/prg2wav:LDLIBS += -pthread
endif

cmdline/wav2prg:LDLIBS+=-ldl

HHC=hhc

docs\wavprg.chm: docs\wavprg-doc.hhp docs\contacts.htm docs\credits.htm docs\intro.htm docs\license.htm docs\main.htm docs\main.png docs\prg2wav.png docs\prg2wav_intro.htm docs\prg2wav_main.htm docs\wav2prg.png docs\wav2prg_intro.htm docs\wav2prg_main.htm
	$(HHC) $<
	
clean:
	rm -f common_core/*.o wav2prg_core/*.o prg2wav_core/*.o windows_gui/*.o cmdline/*.o \
	common_core/*~ wav2prg_core/*~ prg2wav_core/*~ windows_gui/*~ cmdline/*~
