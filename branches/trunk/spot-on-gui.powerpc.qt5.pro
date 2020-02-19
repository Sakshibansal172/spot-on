cache()
include(spot-on-gui-source.pro)
libntl.target = libntl.so
libntl.commands = echo
libntl.depends =
libntru.target = libntru.so
libntru.commands = $(MAKE) -C ../../libNTRU
libntru.depends =
libspoton.target = libspoton.so
libspoton.commands = $(MAKE) -C ../../libSpotOn library
libspoton.depends =

CONFIG		+= qt release warn_on
LANGUAGE	= C++
QT		+= bluetooth concurrent gui multimedia network printsupport \
		   sql websockets widgets

# The function gcry_kdf_derive() is available in version
# 1.5.0 of the gcrypt library.

DEFINES	+= SPOTON_BLUETOOTH_ENABLED \
           SPOTON_LINKED_WITH_LIBGEOIP \
	   SPOTON_LINKED_WITH_LIBNTRU \
           SPOTON_LINKED_WITH_LIBPTHREAD \
           SPOTON_MCELIECE_ENABLED \
	   SPOTON_SCTP_ENABLED \
	   SPOTON_WEBSOCKETS_ENABLED

# Unfortunately, the clean target assumes too much knowledge
# about the internals of libNTL, libNTRU, and libSpotOn.

QMAKE_CLEAN            += Spot-On \
                          ../../libNTL/unix.d/src/*.o \
                          ../../libNTL/unix.d/src/*.lo \
                          ../../libNTRU/*.so ../../libNTRU/src/*.o \
                          ../../libNTRU/src/*.s \
                          ../../libSpotOn/*.o ../../libSpotOn/*.so \
                          ../../libSpotOn/test
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -fPIE -fstack-protector-all -fwrapv \
			  -mabi=altivec -mpowerpc -mtune=powerpc \
                          -pie -O3 \
			  -Wall -Wcast-align -Wcast-qual \
			  -Wextra -Wno-unused-parameter \
			  -Woverloaded-virtual -Wpointer-arith \
                          -Wstack-protector -Wstrict-overflow=5 \
                          -std=c++11
QMAKE_DISTCLEAN        += -r temp .qmake.cache .qmake.stash
QMAKE_EXTRA_TARGETS    = libntl libntru libspoton purge
QMAKE_LFLAGS_RELEASE   += -Wl,-rpath,/usr/local/spot-on/Lib

INCLUDEPATH	+= . ../../. ../../libNTL/unix.d/include \
                   GUI /usr/include/postgresql
LIBS		+= -L../../libNTL/unix.d/src/.libs \
                   -L../../libNTRU -L../../libSpotOn \
		   -lGeoIP -lcrypto -lcurl -lgcrypt \
		   -lgpg-error -lntl -lntru -lpq -lspoton -lssl
MOC_DIR         = temp/moc
OBJECTS_DIR     = temp/obj
PRE_TARGETDEPS  = libntl.so libntru.so libspoton.so
PROJECTNAME	= Spot-On
RCC_DIR         = temp/rcc
TARGET		= Spot-On
TEMPLATE        = app
UI_DIR          = temp/ui

# Prevent qmake from stripping everything.

QMAKE_STRIP	= echo
