FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ninja-build cmake git curl ca-certificates \
    clang lldb clang-tidy pkg-config \
    libsodium-dev librocksdb-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Conan (pipx or pip)
RUN apt-get update && apt-get install -y python3 python3-pip python3-venv pipx && rm -rf /var/lib/apt/lists/* \
    && pipx install conan==2.6.0 \
    && pipx ensurepath

# Create non-root user (VS Code default)
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd -s /bin/bash --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && apt-get update && apt-get install -y sudo \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

USER vscode
WORKDIR /workspaces/astro