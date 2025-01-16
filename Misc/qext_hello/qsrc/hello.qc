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
