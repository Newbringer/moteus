FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

# Use old-releases since 18.04 is EOL
RUN sed -i -E 's#http://(archive|security)\\.ubuntu\\.com/ubuntu/?#http://old-releases.ubuntu.com/ubuntu/#g' /etc/apt/sources.list \
    && apt-get update -y \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        git \
        unzip \
        zip \
        build-essential \
        openjdk-11-jdk-headless \
        python3 \
        python3-distutils \
        libtinfo5 \
    && rm -rf /var/lib/apt/lists/*

# Install Bazelisk to manage Bazel versions automatically
ARG BAZELISK_VERSION=v1.21.0
RUN curl -fsSL -o /usr/local/bin/bazel https://github.com/bazelbuild/bazelisk/releases/download/${BAZELISK_VERSION}/bazelisk-linux-amd64 \
    && chmod +x /usr/local/bin/bazel

# Prepare workspace
WORKDIR /workspace

# Avoid git safety complaints when running as root in containers
RUN git config --global --add safe.directory /workspace || true

# Default command: build moteus firmware with UART protocol enabled using the target (stm32g4) toolchain
CMD ["bash", "-lc", "./tools/bazel build --config=target //fw:moteus --define MOTEUS_ENABLE_UART_PROTOCOL=1"]


