%.so: %.c
	$(CC) $(CFLAGS) -shared $(LDFLAGS) -o $@ $^

%.dll: %.o dll_entry_point.o
	$(CC) -nostdlib -shared -o $@ $^

CFLAGS=-iquote../wav2prg_core -iquote../common_core
ifdef LINUX64BIT
  CFLAGS+=-fPIC
endif
ifdef DEBUG
  CFLAGS+=-g
endif

clean:
	rm -f *.dll *.so *.o *~

