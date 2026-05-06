LIBPNG_DIR = /home/softsec/libpng-1.2.56

.PHONY: build-instrumented-libpng build-harness build fuzz

build-instrumented-libpng:
	cd $(LIBPNG_DIR) && \
	CC=afl-clang-fast \
	CXX=afl-clang-fast++ \
	CFLAGS="-fsanitize=address -g -O1" \
	LDFLAGS="-fsanitize=address" \
	./configure --disable-shared --prefix=/home/softsec/install && \
	make -j$$(nproc) && \
	make install

build-harness:
	afl-clang-fast ./src/harness.c \
	-I/home/softsec/install/include \
	-L/home/softsec/install/lib \
	-lpng12 -lz -lm \
	-fsanitize=address -g -O1 \
	-o png_fuzz


build: build-instrumented-libpng build-harness


fuzz: build 
	afl-fuzz -i seeds -o findings -x /opt/AFLplusplus/dictionaries/png.dict -- ./png_fuzz @@


