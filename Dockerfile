FROM ubuntu:26.04
ARG DEBIAN_FRONTEND=noninteractive
ARG VELLUM_CORE_ONLY=ON
ARG VELLUM_COVERAGE=OFF
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build python3 python3-pip git curl ca-certificates unzip \
    pkg-config libicu-dev libatomic1 libwxgtk3.2-dev libgtk-3-dev clang lld \
    libglib2.0-dev libnss3-dev libnspr4-dev libdbus-1-dev libx11-dev \
    libxcomposite-dev libxdamage-dev libxext-dev libxfixes-dev libxrandr-dev \
    libxkbcommon-dev libgbm-dev libasound2-dev libxtst6 xvfb xauth gcovr \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src/vellum
COPY . .
RUN cmake -S . -B work/docker -DVELLUM_CORE_ONLY=${VELLUM_CORE_ONLY} -DVELLUM_COVERAGE=${VELLUM_COVERAGE} -DCMAKE_BUILD_TYPE=Debug \
    && cmake --build work/docker -j2
CMD ["sh", "-c", "ctest --test-dir work/docker -j \"$(nproc)\" --output-on-failure"]
