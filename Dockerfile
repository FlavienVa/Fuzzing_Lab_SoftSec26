FROM aflplusplus/aflplusplus

RUN mkdir -p /home/softsec
WORKDIR /home/softsec

RUN wget -q https://download.sourceforge.net/libpng/libpng-1.2.56.tar.gz && \
    tar xf libpng-1.2.56.tar.gz && \
    rm libpng-1.2.56.tar.gz

RUN patch -p0 -d libpng-1.2.56 \
    < /AFLplusplus/utils/libpng_no_checksum/libpng-nocrc.patch

RUN cd libpng-1.2.56 && \
    CC=afl-clang-fast \
    CFLAGS="-fsanitize=address -g -O1" \
    LDFLAGS="-fsanitize=address" \
    ./configure --disable-shared --prefix=/home/softsec/install && \
    make -j$(nproc) && \
    make install

# use AFL++ PNG test cases as seed corpus
RUN cp -r /AFLplusplus/testcases/images/png /home/softsec/seeds

CMD ["/bin/bash"]