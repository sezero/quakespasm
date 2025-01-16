.PHONY: clean run qsrc src hello

hello: qsrc src
	mkdir -p hello
	cp qsrc/progs.dat hello/progs.dat
	cp src/hello_x86_64.so hello/hello_x86_64.so
	cp src/hello_x86_64.dll hello/hello_x86_64.dll

qsrc:
	make -C qsrc

src: qsrc
	cp qsrc/progdefs.h src/progdefs.h
	cd src; make -f Makefile.linux64
	cd src; make -f Makefile.w64

clean:
	rm -rf hello/*
	make clean -C qsrc
	make clean -C src

run: hello
	quakespasm --args -nolauncher -basedir $(PWD) -game hello \
		+extensions 1 +developer 1 +map start \
		-width 1024 -height 768 -bpp 32 -window -nosound
