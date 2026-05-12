LIBPNG_DIR        = /home/softsec/libpng-1.2.56
INSTALL_DIR       = /home/softsec/install
INSTALL_DIR_NOSAN = /home/softsec/install_nosan
INSTALL_DIR_VAN   = /home/softsec/install_vanilla

.PHONY: build-instrumented-libpng build-harness build fuzz clean \
        build-libpng-nosan build-harness-nosan fuzz-nosan \
        build-harness-persistent fuzz-persistent \
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
	rm -f png_fuzz png_fuzz_nosan png_fuzz_qemu png_fuzz_persistent
	rm -rf $(INSTALL_DIR) $(INSTALL_DIR_NOSAN) $(INSTALL_DIR_VAN)
	cd $(LIBPNG_DIR) && make distclean || true
	rm -rf findings findings-nosan findings-qemu findings-persistent
	rm -rf minimized

fuzz: build 
	afl-cmin -i ./seeds -o minimized -- ./png_fuzz @@
	afl-fuzz -i minimized -o findings -x /AFLplusplus/dictionaries/png.dict -- ./png_fuzz @@

# No sanitizer build
build-libpng-nosan:
	cd $(LIBPNG_DIR) && \
	make distclean || true && \
	CC=afl-clang-fast \
	CFLAGS="-g -O1" \
	./configure --disable-shared --prefix=$(INSTALL_DIR_NOSAN) && \
	make -j$$(nproc) && \
	make install

build-harness-nosan:
	afl-clang-fast ./src/harness.c \
	-I$(INSTALL_DIR_NOSAN)/include \
	-L$(INSTALL_DIR_NOSAN)/lib \
	-lpng12 -lz -lm \
	-g -O1 \
	-o png_fuzz_nosan

fuzz-nosan: build-libpng-nosan build-harness-nosan
	afl-fuzz -i /home/softsec/seeds -o findings-nosan \
	-x /AFLplusplus/dictionaries/png.dict \
	-- ./png_fuzz_nosan @@

# Persistent mode build
build-harness-persistent:
	afl-clang-fast ./src/harness_persistent.c \
	-I$(INSTALL_DIR)/include \
	-L$(INSTALL_DIR)/lib \
	-lpng12 -lz -lm \
	-fsanitize=address -g -O1 \
	-o png_fuzz_persistent

fuzz-persistent: build-harness-persistent
	afl-fuzz -i /home/softsec/seeds -o findings-persistent \
	-x /AFLplusplus/dictionaries/png.dict \
	-- ./png_fuzz_persistent @@

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