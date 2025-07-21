# Use Debian Bookworm Slim as the base image
FROM debian:bookworm-slim

# Prevent interactive prompts during package installs
ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# -------------------------------------------------------------------
# 1. Install core utilities and package management tools
# -------------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        software-properties-common \
        ca-certificates \
        curl \
        wget \
        gnupg \
        unzip \
        dos2unix \
        debconf-utils \
        xfonts-utils \
        fontconfig \
        cabextract && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# -------------------------------------------------------------------
# 2. Install development tools and dependencies
# -------------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        gdb \
        cmake \
        git \
        python3 \
        python3-venv \
        python3-pip \
        libboost-all-dev \
        cppcheck \
        valgrind \
        passwd \
        lsb-release && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# -------------------------------------------------------------------
# 3. Install documentation and LaTeX tools
# -------------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        doxygen \
        doxygen-latex \
        graphviz \
        latexmk \
        texlive-latex-base \
        texlive-fonts-recommended \
        texlive-xetex \
        xindy && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# -------------------------------------------------------------------
# 4. Install fonts from Debian repositories
# -------------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        fonts-cantarell \
        fonts-roboto \
        fonts-freefont-otf && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# -------------------------------------------------------------------
# 5. Install Ubuntu font manually & explicitly set the correct permissions after extracting
# -------------------------------------------------------------------
RUN mkdir -p /usr/share/fonts/truetype/ubuntu && \
    wget -q https://assets.ubuntu.com/v1/0cef8205-ubuntu-font-family-0.83.zip -O /tmp/ubuntu-font.zip && \
    unzip /tmp/ubuntu-font.zip -d /usr/share/fonts/truetype/ubuntu && \
    chmod -R a+rX /usr/share/fonts/truetype/ubuntu && \
    rm /tmp/ubuntu-font.zip && \
    fc-cache -f -v

# -------------------------------------------------------------------
# 6. Install Microsoft core fonts manually from SourceForge
# -------------------------------------------------------------------
# RUN mkdir -p /usr/share/fonts/truetype/msttcorefonts && \
#     cd /tmp && \
#     for font in \
#         andale32.exe \
#         arial32.exe \
#         arialb32.exe \
#         comic32.exe \
#         courie32.exe \
#         georgi32.exe \
#         impact32.exe \
#         times32.exe \
#         trebuc32.exe \
#         verdan32.exe \
#         webdings.exe; do \
#         wget -q https://downloads.sourceforge.net/corefonts/$font && \
#         cabextract -d /usr/share/fonts/truetype/msttcorefonts $font; \
#     done && \
#     rm -f /tmp/*.exe && \
#     fc-cache -f
RUN echo "deb http://deb.debian.org/debian bookworm main contrib non-free" > /etc/apt/sources.list && \
    echo "deb http://security.debian.org/debian-security bookworm-security main contrib non-free" >> /etc/apt/sources.list && \
    echo "deb http://deb.debian.org/debian bookworm-updates main contrib non-free" >> /etc/apt/sources.list && \
    apt-get update && \
    echo "ttf-mscorefonts-installer msttcorefonts/accepted-mscorefonts-eula select true" | debconf-set-selections && \
    apt-get install -y --no-install-recommends ttf-mscorefonts-installer && \
    fc-cache -f -v && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# -------------------------------------------------------------------
# 7. Download PlantUML
# -------------------------------------------------------------------
WORKDIR /plantuml
RUN wget https://netcologne.dl.sourceforge.net/project/plantuml/plantuml.jar

# Create alias for PlantUML
RUN echo 'alias plantuml="java -jar /plantuml/plantuml.jar"' >> /etc/bash.bashrc

# -------------------------------------------------------------------
# 8. Install Clang and LLVM
# -------------------------------------------------------------------
ARG CLANG_VER=16
WORKDIR /tmp
RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh ${CLANG_VER} all && \
    rm -f llvm.sh

# Set selected clang version as default
RUN ln -s /usr/bin/clang-${CLANG_VER} /usr/bin/clang && \
    ln -s /usr/bin/clang-tidy-${CLANG_VER} /usr/bin/clang-tidy && \
    ln -s /usr/bin/clang-format-${CLANG_VER} /usr/bin/clang-format && \
    ln -s /usr/bin/scan-build-${CLANG_VER} /usr/bin/scan-build && \
    ln -s /usr/bin/run-clang-tidy-${CLANG_VER} /usr/bin/run-clang-tidy && \
    ln -s /usr/bin/clang-apply-replacements-${CLANG_VER} /usr/bin/clang-apply-replacements

# -------------------------------------------------------------------
# 9. Set up Python environment and install requirements
# -------------------------------------------------------------------
WORKDIR /tmp
COPY requirements.txt .
RUN python3 -m venv /opt/venv && \
    /opt/venv/bin/pip install --no-cache-dir -r requirements.txt && \
    ln -s /opt/venv/bin/python /usr/local/bin/python && \
    ln -s /opt/venv/bin/pip /usr/local/bin/pip
ENV PATH="/opt/venv/bin:$PATH"

# -------------------------------------------------------------------
# 10 Install Embedded Development and Debugging Tools
# -------------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ninja-build \
        openocd \
        gcc-arm-none-eabi \
        libnewlib-arm-none-eabi \
        gdb-multiarch && \
    ln -s /usr/bin/gdb-multiarch /usr/bin/arm-none-eabi-gdb && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# -------------------------------------------------------------------
# 11 Add Buildroot tools and optional embedded dependencies
# -------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    which sed make binutils diffutils gcc g++ bash patch gzip bzip2 \
    perl tar cpio rsync file bc findutils \
    libncurses5-dev qtbase5-dev qtchooser libglib2.0-dev libgtk2.0-dev libglade2-dev \
    bzr cvs mercurial openssh-client subversion default-jdk \
    asciidoc w3m dblatex \
    python3-matplotlib python3-aiohttp \
    ccache \
    python-is-python3 \
    usbutils \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# -------------------------------------------------------------------
# 12. Install misc developer tools (ImageMagick, FFmpeg, etc.)
# -------------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        imagemagick \
        ffmpeg \
        net-tools \
        iputils-ping \
        tree \
        htop \
        jq \
        lsof \
        zip \
        p7zip-full && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# -------------------------------------------------------------------
# 13. Add non-root user and enable passwordless sudo
# -------------------------------------------------------------------
RUN adduser --disabled-password --gecos '' vscode && \
    apt-get update && \
    apt-get install -y --no-install-recommends sudo && \
    usermod -aG sudo vscode && \
    echo "vscode ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/vscode && \
    chmod 0440 /etc/sudoers.d/vscode && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# -------------------------------------------------------------------
# 14. Copy Devcontainer entry point script to set right permissions for sh scripts that will be used from windows in linux devcontainer
# -------------------------------------------------------------------
COPY entrypoint.sh /usr/local/bin/devcontainer-entrypoint.sh
RUN chmod +x /usr/local/bin/devcontainer-entrypoint.sh

# -------------------------------------------------------------------
# 14. Final setup
# -------------------------------------------------------------------
# devcontainer-entrypoint.sh moved to postCreateCommand (devcontainer.json)
# ENTRYPOINT ["/usr/local/bin/devcontainer-entrypoint.sh"]
ENTRYPOINT ["/bin/bash", "-c", "while true; do sleep 1000; done"]