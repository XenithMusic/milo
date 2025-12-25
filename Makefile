CXX	  = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
PKGCFG   = pkg-config
DBUS	 = dbus-1

TARGET_DAEMON   = bin/milo
SRC_DAEMON	  = src/main.cpp

TARGET_VIDEO   = bin/milo_display
SRC_VIDEO	  = src/display/main.cpp

DBUS_CFLAGS = $(shell pkg-config --cflags sdbus-c++)
DBUS_LIBS   = -lsdbus-c++

RAY_CFLAGS = $(shell pkg-config --cflags raylib)
RAY_LIBS   = $(shell pkg-config --libs raylib)

ICU_CFLAGS = $(shell pkg-config --cflags icu-uc)
ICU_LIBS   = $(shell pkg-config --libs icu-uc)

X11_CFLAGS = $(shell pkg-config --cflags x11)
X11_LIBS = $(shell pkg-config --libs x11)

all: $(TARGET_DAEMON) $(TARGET_VIDEO)

$(TARGET_DAEMON): $(SRC_DAEMON)
	$(CXX) $(CXXFLAGS) $(DBUS_CFLAGS) $(SRC_DAEMON) -o $(TARGET_DAEMON) -Wl,-Bstatic $(DBUS_LIBS) -Wl,-Bdynamic -lsystemd -lcap 

$(TARGET_VIDEO): $(SRC_VIDEO)
	$(CXX) $(CXXFLAGS) $(RAY_CFLAGS) $(X11_CFLAGS) $(ICU_CFLAGS) $(SRC_VIDEO) -o $(TARGET_VIDEO) $(RAY_LIBS) $(X11_LIBS) $(ICU_LIBS)

clean:
	rm -f $(TARGET_DAEMON)
	rm -f $(TARGET_VIDEO)

installUser: all
	cp $(TARGET_DAEMON) ~/.local/bin/milo
	chmod +x ~/.local/bin/milo
	cp $(TARGET_VIDEO) ~/.local/bin/milo_display
	chmod +x ~/.local/bin/milo_display

installSystem: all
	sudo cp $(TARGET_DAEMON) /usr/local/bin/milo
	sudo chmod +x /usr/local/bin/milo
	sudo cp $(TARGET_VIDEO) /usr/local/bin/milo_display
	sudo chmod +x /usr/local/bin/milo_display

install: installSystem

uninstallUser:
	rm ~/.local/bin/milo
	rm ~/.local/bin/milo_display

uninstallSystem:
	sudo rm /usr/local/bin/milo
	sudo rm /usr/local/bin/milo_display

run: all
	$(TARGET_DAEMON)