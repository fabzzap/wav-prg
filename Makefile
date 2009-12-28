wav2prg: wav2prg_api.o main.o turbotape.o
	$(CC) -o $@ $^

