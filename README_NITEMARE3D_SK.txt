SLADE 3.2.0 + podpora Nitemare 3-D
=================================

Tento balík bol zostavený z aktuálneho SLADE master commitu
351fd983fa986423c1825c87128691c27e0817aa.

LINUX BUILD
-----------
Súbory slade a slade.pk3 musia zostať v rovnakom priečinku. Spustenie:

  chmod +x slade
  ./slade

Ide o Linux x86-64 build, nie Windows EXE.

OTVÁRANIE NITEMARE 3-D SÚBOROV
------------------------------
Použi File > Open alebo súbor pretiahni do okna SLADE. Podporované sú:

  IMG.1, IMG.2, IMG.3
  MAP.1, MAP.2, MAP.3
  UIF.DAT, SND.DAT

OBJECTS.1-3 a WALLS.1-3 sú textové definície; SLADE ich otvorí ako text.
GAME.PAL je PCX a SLADE ho otvorí vstavaným PCX dekóderom.

ROZSAH PODPORY
--------------
IMG sa rozdelí na jednotlivé IMAGE####.N3I a obrázky sa zobrazia v grafickom
náhľade. MAP sa rozdelí na MAP01.N3M atď.; MAP.1 obsahuje aj MAP11.N3M
(demo mapa E1M11). UIF/SND sa rozdelia podľa svojich 16-bitových dĺžok a
32-bitových offsetov. PCX a MIDI položky sa rozpoznajú automaticky.

ZÁPIS A BEZPEČNOSŤ
------------------
MAP: každý level musí zostať presne 8192 bajtov.
DAT: položka môže mať najviac 65535 bajtov.
IMG: položky sa nesmú pridávať, odstraňovať ani meniť ich veľkosť. Interné
lookup tabuľky IMG nie sú úplne zdokumentované; SLADE rizikový zápis odmietne.

Táto verzia poskytuje archívový/resource editor a 2D náhľad mapy. Samostatný
Nitemare mapový režim s pomenovanými štetcami podľa OBJECTS/WALLS zatiaľ nie
je implementovaný.
