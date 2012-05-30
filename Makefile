wav2prg: wav2prg_core.o \
         wav2prg.o \
         loaders.o \
         observers.o \
         get_pulse.o \
         write_cleaned_tap.o \
         write_prg.o \
         yet_another_getopt.o \
         wav2prg_block_list.o \
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
             wav2prg_block_list.o \
             create_t64.o \
             audiotap_interface.o
	$(LINK.o) $^ $(LDLIBS) -o $@

t.o: wavprg.rc
	windres $(CPPFLAGS) -o $@ $^

t: CPPFLAGS = -D_WIN32_IE=0x0400
t: LDLIBS = -lcomctl32

t: gui_main.o \
   t.o \
   loaders.o \
   dependency_tree.o \
   turbotape.o \
   kernal.o \
   novaload.o \
   audiogenic.o \
   pavlodapenetrator.o \
   pavlodaold.o \
   pavloda.o \
   connection.o \
   rackit.o

ifdef AUDIOTAP_HDR
  CFLAGS+=-I $(AUDIOTAP_HDR)
endif
ifdef DEBUG
  CFLAGS+=-g
endif

LDLIBS=-laudiotap
ifdef AUDIOTAP_LIB
  LDLIBS+=-L$(AUDIOTAP_LIB)
  ifdef USE_RPATH
    LDLIBS+=-Wl,-rpath=$(AUDIOTAP_LIB)
  endif
endif

wav2prg:LDLIBS+=-ldl

