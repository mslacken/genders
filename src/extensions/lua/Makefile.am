# DESTDIR is usually set during make install time, not configure or
# make time, so we work around it with the --with-extension-destdir
# autoconf option.


if WITH_LUA_EXTENSIONS

all: genderslua.c ../../libgenders/libgenders.la
	$(CC) -shared -o genders.so @LUA_LIB@ @LUA_INCLUDE@ \
	-L../../libgenders/.libs -lgenders -I../../libgenders \
	-W -Wall -fPIC genderslua.c  -L/usr/lib64 \
	-D_FORTIFY_SOURCE=2 -fstack-protector-strong -funwind-tables -fasynchronous-unwind-tables -fstack-clash-protection -fwrapv -O2 

install:
	install -D --target-directory=$(DESTDIR)$(luaexecdir) genders.so 

pure_install:
	install -D --target-directory=$(DESTIR)$(luaexecdir) genders.so 

pure-install-local:
	install -D --target-directory=$(DESTDIR)$(luaexecdir) genders.so 
clean:
	rm -rf *.so
endif

EXTRA_DIST = genderslua.c
