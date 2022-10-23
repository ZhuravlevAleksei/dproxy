# dproxy:0.3.10
FROM redis:latest

WORKDIR /dproxy

COPY ./cJSON /dproxy/cJSON
COPY ./hiredis /dproxy/hiredis
COPY ./libyaml /dproxy/libyaml
COPY ./oniguruma /dproxy/oniguruma
COPY ./src /dproxy/src
COPY ./CMakeLists.txt /dproxy/CMakeLists.txt
COPY ./supervisord.conf /etc/supervisord.conf
COPY ./start.sh /dproxy/start.sh

RUN apt-get update -y \
    && apt-get install -y python3 \
    && apt-get install -y python3-pip \
    && pip3 install supervisor \
    && apt-get install bash -y \
    && apt-get install nano -y \
    && apt-get install htop -y \
    && apt-get install build-essential -y \
    && apt-get install cmake -y \
    && apt-get install ninja-build -y \
    && cmake --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S./ -B./build -G Ninja \
    && cmake --build ./build \
    && apt-get remove htop bash build-essential cmake ninja-build --allow-remove-essential -y \
    && apt-get autoremove --allow-remove-essential -y \
    && apt-get autoclean --allow-remove-essential -y \
    && rm -r ./cJSON ./libyaml ./oniguruma ./src ./CMakeLists.txt \
    ./build/cJSON ./build/CMakeFiles ./build/libyaml ./build/oniguruma

RUN mkdir /dproxy/conf
VOLUME /dproxy/conf

CMD ["/usr/local/bin/supervisord", "-c", "/etc/supervisord.conf"]
