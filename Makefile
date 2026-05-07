CC      = afl-clang-fast
CFLAGS  = -fsanitize=address -g -O1 -I/home/softsec/install/include
LDFLAGS = -fsanitize=address -L/home/softsec/install/lib -lpng12 -lz

TARGET  = fuzzing_png_activate
SRC     = src/harness.c

.PHONY: build fuzz clean

build: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

fuzz: $(TARGET)
	mkdir -p /home/softsec/host/findings
	afl-fuzz -i /home/softsec/seeds -o /home/softsec/host/findings -- ./$(TARGET) @@

clean:
	rm -f $(TARGET)