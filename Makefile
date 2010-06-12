wav2prg: wav2prg_api.o \
         main.o \
         turbotape.o \
         kernal.o \
         loaders.o \
         novaload.o \
         dependency_tree.o \
         audiogenic.o \
         pavlodapenetrator.o \
         pavlodaold.o \
         pavloda.o \
         connection.o \
         rackit.o
	$(CC) -o $@ $^

