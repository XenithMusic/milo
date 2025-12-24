CXX	  = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
PKGCFG   = pkg-config
DBUS	 = dbus-1

TARGET   = notif_listener
SRC	  = src/main.cpp

DBUS_CFLAGS = $(shell pkg-config --cflags sdbus-c++)
DBUS_LIBS   = $(shell pkg-config --libs sdbus-c++)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(DBUS_CFLAGS) $(SRC) -o $(TARGET) $(DBUS_LIBS)

clean:
	rm -f $(TARGET)