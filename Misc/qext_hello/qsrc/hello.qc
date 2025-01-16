/*
 * Example Quake Extension
 *
 * Copyright (C) 1996-1997 Id Software, Inc.
 * Copyright (C) 2025 Tóth János <gomba007@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

float hello_ext;

void() main = {};

void() worldspawn =
{
    precache_model ("progs/player.mdl");

    hello_ext = OpenExtension("hello");
};

void() SetNewParms = {};

void() SetChangeParms = {};

void() StartFrame = {};

void() ClientKill = {};

void() ClientConnect = {};

void() ClientDisconnect = {};

void() PlayerPreThink = {};

void() PlayerPostThink = {};

void() PutClientInServer =
{
    local string str;
    local float num;
    local vector vec;

    self.classname = "player";
    self.origin = '1 2 3';

    str = CallExtensionString(hello_ext, "hello", "world");
    dprint(str); dprint("\n");

    num = 10;
    num = CallExtensionNumber(hello_ext, "hello", num);
    dprint(ftos(num)); dprint("\n");

    vec = CallExtensionVector(hello_ext, "hello", self.origin);
    dprint(vtos(vec)); dprint("\n");

    num = CallExtensionEntity(hello_ext, "hello", self);
    dprint(ftos(num)); dprint("\n");
};
