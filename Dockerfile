FROM ubuntu:16.04
MAINTAINER Stefano Fioravanzo <stefano.fioravanzo@gmail.com>

# Install.
# RUN \
#   sed -i 's/# \(.*multiverse$\)/\1/g' /etc/apt/sources.list && \
#   apt-get update && \
#   apt-get -y upgrade && \
#   apt-get install -y build-essential && \
#   apt-get install -y software-properties-common && \
#   apt-get install -y byobu curl git htop man unzip vim wget && \
#   rm -rf /var/lib/apt/lists/*

RUN \
    sed -i 's/# \(.*multiverse$\)/\1/g' /etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
    software-properties-common \
    build-essential doxygen git wget unzip python-serial \
    vim byobu curl htop man \
    default-jdk ant srecord iputils-tracepath && \
    rm -rf /var/lib/apt/lists/* && \
    apt-get clean

RUN apt-get update
RUN apt-get -y install build-essential autoconf automake srecord picocom python-serial
RUN apt-get -y install gcc-msp430 msp430-libc msp430mcu binutils-msp430
RUN apt-get -y install gcc-arm-none-eabi libnewlib-arm-none-eabi
# RUN apt-get -y install openjdk-8-jre ant

# Create user, enable X forwarding, add to group dialout
# -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix
RUN export uid=1000 gid=1000 && \
    mkdir -p /home/user && \
    echo "user:x:${uid}:${gid}:user,,,:/home/user:/bin/bash" >> /etc/passwd && \
    echo "user:x:${uid}:" >> /etc/group && \
    echo "user ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers && \
    chmod 0440 /etc/sudoers && \
    chown ${uid}:${gid} -R /home/user && \
    usermod -aG dialout user

COPY contiki.tgz /home/
# extract to /contiki dir
RUN tar zxvf /home/contiki.tgz

# Create user, enable X forwarding, add to group dialout
# -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix
RUN export uid=1000 gid=1000 && \
    mkdir -p /home/user && \
    echo "user:x:${uid}:${gid}:user,,,:/home/user:/bin/bash" >> /etc/passwd && \
    echo "user:x:${uid}:" >> /etc/group && \
    echo "user ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers && \
    chmod 0440 /etc/sudoers && \
    chown ${uid}:${gid} -R /home/user && \
    usermod -aG dialout user

# USER user

# Environment variables
ENV HOME                /home/user
ENV CONTIKI             /contiki
ENV PATH                "${PATH}:${CONTIKI}/bin"
ENV COOJA               ${CONTIKI}/tools/cooja
ENV                     PATH="${HOME}:${PATH}"
WORKDIR                 ${HOME}

# Create Cooja shortcut
RUN echo "#!/bin/bash\nant -Dbasedir=${COOJA} -f ${COOJA}/build.xml run" > ${HOME}/cooja && \
  chmod +x ${HOME}/cooja

# Add project code
ADD . /code
WORKDIR /code/src
