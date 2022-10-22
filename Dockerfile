# FROM ubuntu:22.04
FROM redis:latest

WORKDIR /dproxy

COPY ./cJSON /dproxy/cJSON
COPY ./hiredis /dproxy/hiredis
COPY ./libyaml /dproxy/libyaml
COPY ./oniguruma /dproxy/oniguruma
COPY ./src /dproxy/src
COPY ./blacklist.yml /dproxy/blacklist.yml
COPY ./server.yml /dproxy/server.yml
COPY ./start.sh /dproxy/start.sh
COPY ./CMakeLists.txt /dproxy/CMakeLists.txt

RUN chmod +x ./start.sh

RUN apt-get update -y \
    && apt-get install nano -y \
    && apt-get install htop -y \
    && apt-get install build-essential -y \
    && apt-get install cmake -y \
    && apt-get install ninja-build -y \
    && cmake --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S./ -B./build -G Ninja \
    && cmake --build ./build

# ENTRYPOINT /dproxy/start.sh
ENTRYPOINT /bin/sh
