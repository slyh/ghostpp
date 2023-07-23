FROM ubuntu:22.04

WORKDIR /build
RUN apt-get update && \
    apt-get install -y \
    cmake \
    git \
    wget \
    libboost-all-dev \
    build-essential \
    libgmp-dev \
    zlib1g-dev \
    libbz2-dev \
    libmysql++-dev

WORKDIR /build/bncsutil
COPY bncsutil .
WORKDIR /build/bncsutil/build
RUN cmake -G "Unix Makefiles" -B . -S ..
RUN make && make install

WORKDIR /build/StormLib
COPY StormLib .
WORKDIR /build/StormLib/build
RUN cmake -G "Unix Makefiles" -B . -S ..
RUN make && make install

WORKDIR /build/CascLib
COPY CascLib .
WORKDIR /build/CascLib/build
RUN cmake -G "Unix Makefiles" -B . -S ..
RUN make && make install

RUN ldconfig

WORKDIR /build/ghost
COPY ghost .
RUN make -j `nproc`

WORKDIR /ghostpp
RUN cp /build/ghost/ghost++ .

CMD ["/ghostpp/ghost++"]
