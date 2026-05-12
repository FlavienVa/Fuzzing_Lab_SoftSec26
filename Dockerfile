FROM aflplusplus/aflplusplus

RUN mkdir -p /home/softsec
WORKDIR /home/softsec

RUN apt-get update && apt-get install -y patch && rm -rf /var/lib/apt/lists/*

RUN wget -q https://download.sourceforge.net/libpng/libpng-1.2.56.tar.gz && \
    tar xf libpng-1.2.56.tar.gz && \
    rm libpng-1.2.56.tar.gz

RUN patch -p0 -d libpng-1.2.56 \
    < /AFLplusplus/utils/libpng_no_checksum/libpng-nocrc.patch

RUN cp -r /AFLplusplus/testcases/images/png /home/softsec/seeds

CMD ["/bin/bash"]


