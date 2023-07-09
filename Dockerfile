FROM ubuntu:22.04
WORKDIR /build
COPY . .

RUN apt-get update && \
    apt-get install -y cmake git wget libboost-all-dev build-essential libgmp-dev zlib1g-dev libbz2-dev libmysql++-dev
RUN wget -O libdpp.deb https://dl.dpp.dev/
RUN apt-get install -y ./libdpp.deb
RUN rm ./libdpp.deb

WORKDIR /build/bncsutil
RUN mkdir -p build
RUN cmake -G "Unix Makefiles" -B./build -H./
WORKDIR /build/bncsutil/build
RUN make && make install

WORKDIR /build/StormLib/
RUN mkdir -p build
RUN cmake -G "Unix Makefiles" -B./build -H./
WORKDIR /build/StormLib/build
RUN make && make install

WORKDIR /build/CascLib/
RUN mkdir -p build
RUN cmake -G "Unix Makefiles" -B./build -H./
WORKDIR /build/CascLib/build
RUN make && make install

RUN ldconfig

WORKDIR /build/ghost/
RUN make

WORKDIR /ghostpp
RUN mv /build/ghost/ghost++ /ghostpp
RUN rm -rf /build

CMD ["/ghostpp/ghost++"]
