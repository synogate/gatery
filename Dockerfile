FROM ubuntu:22.04
RUN apt-get update && apt-get install build-essential g++-10 libboost-all-dev git libgmp-dev libyaml-cpp-dev curl -y
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 10 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 10 && \
    update-alternatives --config gcc && \
    update-alternatives --config g++
RUN curl -L https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-linux.tar.gz > /tmp/premake-5.0.0-beta2-linux.tar.gz && \
 tar -zxf  /tmp/premake-5.0.0-beta2-linux.tar.gz -C /tmp/ && mv /tmp/premake5 /usr/local/bin/ && rm /tmp/premake-5.0.0-beta2-linux.tar.gz
RUN curl -L https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.gz > /tmp/boost_1_76_0.tar.gz && \
 mkdir -p ~/Documents/software/ && tar -zxf  /tmp/boost_1_76_0.tar.gz -C ~/Documents/software/ && \
 cd ~/Documents/software/boost_1_76_0 && ./bootstrap.sh && ./b2 && ./b2 install &&  rm /tmp/boost_1_76_0.tar.gz
RUN git clone --recursive https://github.com/synogate/gatery_template.git ~/Documents/gatery/hello_world/ && \
    cd ~/Documents/gatery/hello_world/ && premake5 gmake2
CMD exec /bin/bash -c "trap : TERM INT; sleep infinity & wait"
