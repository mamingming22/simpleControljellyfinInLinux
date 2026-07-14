CFLAGS  = $(shell pkg-config --cflags gtk+-3.0 ayatana-appindicator3-0.1) -Os -s -Wno-deprecated-declarations
LDLIBS  = $(shell pkg-config --libs gtk+-3.0 ayatana-appindicator3-0.1)
TARGET  = jellyfin-ctl

all: $(TARGET)

$(TARGET): jellyfin-ctl.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(TARGET)
