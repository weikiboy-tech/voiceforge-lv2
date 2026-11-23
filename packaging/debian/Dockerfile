FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends \
    binutils cmake dpkg-dev g++ liblilv-dev lilv-utils lv2-dev ninja-build pkg-config serdi sordi \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
CMD ["sh", "scripts/build.sh"]
