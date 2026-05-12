LIBPNG_DIR        = /home/softsec/libpng-1.2.56
INSTALL_DIR       = /home/softsec/install
INSTALL_DIR_VAN   = /home/softsec/install_vanilla

.PHONY: build-instrumented-libpng build-harness build fuzz clean \
        build-vanilla-libpng build-harness-qemu fuzz-qemu

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

clean:
	rm -f png_fuzz png_fuzz_qemu
	rm -rf $(INSTALL_DIR) $(INSTALL_DIR_VAN)
	cd $(LIBPNG_DIR) && make distclean || true
	rm -rf findings findings-qemu

fuzz: build 
	afl-cmin -i ./seeds -o minimized -- ./png_fuzz @@
	afl-fuzz -i minimized -o findings -x /AFLplusplus/dictionaries/png.dict -- ./png_fuzz @@

# QEMU mode
build-vanilla-libpng:
	cd $(LIBPNG_DIR) && \
	make distclean || true && \
	CC=gcc \
	CFLAGS="-g -O1" \
	./configure --disable-shared --prefix=$(INSTALL_DIR_VAN) && \
	make -j$$(nproc) && \
	make install

build-harness-qemu:
	gcc ./src/harness.c \
	-I$(INSTALL_DIR_VAN)/include \
	-L$(INSTALL_DIR_VAN)/lib \
	-lpng12 -lz -lm \
	-g -O1 \
	-o png_fuzz_qemu

fuzz-qemu: build-vanilla-libpng build-harness-qemu
	afl-cmin -Q -i ./seeds -o minimized -- ./png_fuzz_qemu @@
	afl-fuzz -Q \
	-i minimized \
	-o findings-qemu \
	-x /AFLplusplus/dictionaries/png.dict \
	-- ./png_fuzz_qemu @@