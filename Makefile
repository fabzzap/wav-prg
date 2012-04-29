wav2prg: wav2prg_core.o \
         main.o \
         loaders.o \
         observers.o \
         get_pulse.o \
         write_cleaned_tap.o \
         write_prg.o \
         tapfile_write.o \
         yet_another_getopt.o \
         wav2prg_block_list.o \
         turbotape.o \
         kernal.o \
         novaload.o \
         audiogenic.o \
         pavlodapenetrator.o \
         pavlodaold.o \
         pavloda.o \
         connection.o \
         detective.o \
         turbo220.o \
         freeload.o \
         rackit.o \
         wildsave.o \
         theedge.o \
         maddoctor.o \
         mikrogen.o \
         crl.o \
         snakeload.o \
         snake.o \
         nobby.o \
         atlantis.o \
         jetload.o \
         microload.o \
         novaload_special.o \
         wizarddev.o \
         opera.o

	$(CC) -o $@ $^

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
