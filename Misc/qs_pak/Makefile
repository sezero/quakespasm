INPUT := gfx/conback.lmp \
	default.cfg

OUTPUT := quakespasm.pak

$(OUTPUT): $(INPUT)
	./mkpak.sh $(INPUT) > $(OUTPUT)

.PHONY: clean
clean:
	rm -f $(OUTPUT)

