CC := gcc
CFLAGS := -shared -fPIC

hello_x86_64.so: hello.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.so
	rm -rf progdefs.h
