FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends \
    binutils cmake dpkg-dev file g++ libcairo2-dev liblilv-dev libx11-dev lilv-utils lv2-dev ninja-build pkg-config python3 serdi sordi \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
CMD ["sh", "scripts/build.sh"]
