FROM ubuntu:16.04
MAINTAINER Stefano Fioravanzo <stefano.fioravanzo@gmail.com>

RUN sed -i 's/# \(.*multiverse$\)/\1/g' /etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
    software-properties-common \
    build-essential doxygen git wget unzip python-serial \
    bc vim byobu curl htop man \
    default-jdk ant srecord iputils-tracepath && \
    rm -rf /var/lib/apt/lists/* && \
    apt-get clean

RUN apt-get update && \
    apt-get -y install build-essential autoconf automake srecord picocom python-serial && \
    apt-get -y install gcc-msp430 msp430-libc msp430mcu binutils-msp430 && \
    apt-get -y install gcc-arm-none-eabi libnewlib-arm-none-eabi
# RUN apt-get -y install openjdk-8-jre ant

COPY ./contiki/contiki.tgz /home/
# extract to /contiki dir
RUN tar zxvf /home/contiki.tgz -C / && \
    rm /home/contiki.tgz

# Environment variables
ENV HOME                /code
ENV CONTIKI             /contiki
ENV PATH                "${PATH}:${CONTIKI}/bin"
ENV COOJA               ${CONTIKI}/tools/cooja

# Add project code
ADD . /code
WORKDIR /code
