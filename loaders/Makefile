%.dll %.so: %.c
	$(CC) $(CFLAGS) -shared -iquote.. $(LDFLAGS) -o $@ $^

%.dll:LDFLAGS+=-nostdlib
