# Built by .github/workflows/deploy.yml and pushed to Artifact Registry.
#
# POCO 1.13 (Poco::Net HTTPServer) on Debian trixie. Multi-stage: the build
# stage has the compiler and libpoco-dev (which pulls every POCO module); the
# runtime stage installs only the four POCO shared libraries the binary links
# and runs as a non-root user. The port is read from $PORT when the container
# starts, not at build time.
FROM debian:trixie AS build
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential cmake ninja-build libpoco-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY src ./src
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build \
 && install -D build/app /out/app

FROM debian:trixie-slim AS runtime
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      libpocofoundation100 libpoconet100 libpocoutil100 libpocojson100 \
 && rm -rf /var/lib/apt/lists/* \
 && useradd -r -u 10001 app
WORKDIR /app
ARG BUILD_ID=""
ENV PORT=8080 BUILD_ID=$BUILD_ID
COPY --from=build /out/app /app/app
EXPOSE 8080
USER app
ENTRYPOINT ["/app/app"]
