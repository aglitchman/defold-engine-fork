FROM ubuntu:20.04

# Base stuff
RUN \
  apt-get update && \
  apt-get install -y --no-install-recommends \
    openjdk-17-jdk-headless

RUN \
  apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    libssl-dev \
    openssl \
    libtool \
    autoconf \
    automake \
    build-essential \
    uuid-dev \
    libxi-dev \
    libopenal-dev \
    libgl1-mesa-dev \
    libglw1-mesa-dev \
    freeglut3-dev \
    # for clang-13.0.0
    libncurses5 \
    # for python+ctypes
    libffi-dev \
    # for vpx
    nasm

RUN \
  apt-get install -y --no-install-recommends \
    tofrodos \
    wget \
    curl \
    zip \
    unzip \
    tree \
    silversearcher-ag \
    git

# Install latest CMake: spirv-tools requires cmake 3.17.2+
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
RUN apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main' && \
  apt update && \
  apt install cmake

#
# llvm
#
ENV CLANG_VERSION=17
RUN \
  echo "LLVM + CLANG ${CLANG_VERSION}" && \
  wget https://apt.llvm.org/llvm.sh && \
  chmod +x llvm.sh && \
  ./llvm.sh ${CLANG_VERSION} && \
  rm llvm.sh

ENV PATH=/usr/lib/llvm-${CLANG_VERSION}/bin:$PATH
ENV LD_LIBRARY_PATH=/usr/lib/llvm-${CLANG_VERSION}/lib:$LD_LIBRARY_PATH

#
# Add ARM64 architecture
#
RUN dpkg --add-architecture arm64 && sed -i -E 's/^deb /deb [arch=amd64] /' /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports focal main restricted universe multiverse" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports focal-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports focal-security main restricted universe multiverse" >> /etc/apt/sources.list

RUN cat /etc/apt/sources.list

RUN apt-get -y update && \
  apt-get -y install --no-install-recommends \
    g++-aarch64-linux-gnu \
    gcc-aarch64-linux-gnu \
    libxi-dev:arm64 \
    libopenal-dev:arm64



ENV PYENV_ROOT /.pyenv
ENV PATH $PYENV_ROOT/shims:$PYENV_ROOT/bin:$PATH

ENV PYTHON3_VERSION 3.10.4

RUN \
    echo "PYENV" && \
    set -ex \
    && curl https://pyenv.run | bash \
    && pyenv update \
    && pyenv install $PYTHON3_VERSION \
    && pyenv global $PYTHON3_VERSION \
    && pyenv rehash

RUN apt-get autoremove

ENV LC_ALL C.UTF-8

# Add builder user
RUN  useradd -r -u 1000 builder && \
  mkdir -p /var/builder && \
  chown builder: /var/builder && \
  chown builder: $(readlink -f /usr/bin/java) && \
  chmod +s $(readlink -f /usr/bin/java)

USER builder
WORKDIR /home/builder
RUN mkdir -p /home/builder
