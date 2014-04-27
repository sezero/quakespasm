#!/bin/sh

input_files="gfx/conback.lmp default.cfg"
output_pak="quakespasm.pak"

#can use qpakman to generate
qpakman $input_files -o $output_pak
