.PHONY: clean run qsrc src hello

hello: qsrc src
	mkdir -p hello
	cp qsrc/progs.dat hello/progs.dat
	cp src/hello_x86_64.so hello/hello_x86_64.so

qsrc:
	make -C qsrc

src: qsrc
	cp qsrc/progdefs.h src/progdefs.h
	make -C src

clean:
	rm -rf hello/*
	make clean -C qsrc
	make clean -C src

run: hello
	quakespasm --args -nolauncher -basedir $(PWD) -game hello \
		+map start +developer 1 \
		-width 1024 -height 768 -bpp 32 -window -nosound
