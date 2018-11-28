FROM ubuntu:17.10
LABEL maintainer="TBD"

#Setup parameters
ARG BUILD_DATE
ARG VCS_REF
LABEL org.label-schema.build-date=$BUILD_DATE \
    org.label-schema.docker.dockerfile="extra/Dockerfile" \
    org.label-schema.license="GPLv3" \
    org.label-schema.name="motion" \
    org.label-schema.url="https://motion-project.github.io/" \
    org.label-schema.vcs-ref=$VCS_REF \
    org.label-schema.vcs-type="Git" \
    org.label-schema.vcs-url="https://github.com/Motion-Project/motion.git"

#Install base packages
RUN DEBIAN_FRONTEND="noninteractive" apt-get update && apt-get install -y --no-install-recommends \
    autoconf automake build-essential pkgconf libtool libzip-dev libjpeg-dev \
    git libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libavdevice-dev \
    libwebp-dev gettext libmicrohttpd-dev ca-certificates imagemagick curl wget tzdata \
    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libavdevice-dev ffmpeg && \
    apt-get --quiet autoremove --yes && \
    apt-get --quiet --yes clean && \
    rm -rf /var/lib/apt/lists/*
    
RUN git clone https://github.com/Motion-Project/motion.git  && \
   cd motion  && \
   autoreconf -fiv && \
   ./configure && \
   make clean && \
   make && \
   make install && \
   cd .. && \
   rm -fr motion

# R/W needed for motion to update configurations
VOLUME /usr/local/etc/motion
# R/W needed for motion to update Video & images
VOLUME /var/lib/motion

CMD test -e /usr/local/etc/motion/motion.conf || \    
    cp /usr/local/etc/motion/motion-dist.conf /usr/local/etc/motion/motion.conf 

CMD [ "motion", "-n" ]
