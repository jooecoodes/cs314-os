FROM debian:stable-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        gcc \
        libc6-dev \
        strace \
        make \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /lab

CMD ["/bin/bash"]
