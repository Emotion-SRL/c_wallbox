FROM debian:buster

# Usa i repository archiviati
RUN echo "deb http://archive.debian.org/debian buster main" > /etc/apt/sources.list && \
    echo "deb http://archive.debian.org/debian-security buster/updates main" >> /etc/apt/sources.list

RUN apt update && apt install -y \
    git wget subversion build-essential libncurses5-dev \
    zlib1g-dev gawk flex quilt git-core unzip libssl-dev \
    python-dev python-pip libxml-parser-perl file rsync \
    time sudo \
    && rm -rf /var/lib/apt/lists/*

# Crea utente builder
RUN useradd -m -s /bin/bash builder && \
    echo "builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

WORKDIR /home/builder

# Clona come builder
USER builder
RUN git clone https://github.com/OnionIoT/source.git

WORKDIR /home/builder/source
