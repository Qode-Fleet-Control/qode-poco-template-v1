# POCO template

Provisioned from [`Qode-Fleet-Control/fleet-template-v1`](https://github.com/Qode-Fleet-Control/fleet-template-v1) — the fleet
lifecycle contract (`bin/`, `fleet.conf`, `compose.yaml`, deploy workflows) with a POCO (Poco::Net HTTPServer) starter laid on top.

A small HTTP service on the POCO C++ Libraries 1.13 (Debian trixie), built with CMake: a `Poco::Util::ServerApplication` that owns a `Poco::Net::HTTPServer`, with one `HTTPRequestHandler` per route. Routes: `GET /` (plain-text greeting), `GET /health` (`{"status":"ok"}`), `GET /api/time` (`{"time":"<ISO-8601>"}`); anything else is a JSON 404.

## Origin

    hand-written (POCO ships no project generator) — src/main.cpp follows POCO's own Net sample HTTPTimeServer (ServerApplication + HTTPServer + HTTPRequestHandlerFactory, waitForTerminationRequest()); CMakeLists.txt uses POCO's CMake package: find_package(Poco REQUIRED COMPONENTS ...) + Poco::<component> targets


## Run it

### On the fleet

The fleet runs it as containers (the docker runtime): `bin/run` builds the image with
`docker compose build` and then starts it with `docker compose up` in the foreground, publishing `$PORT`.

It listens on `0.0.0.0:$PORT` (default `8080`), read from the environment when the container starts,
and serves at the root of its own hostname (`https://<hash>.<FLEET_APP_DOMAIN>/`). The health check hits `/health`.

### With docker

```sh
PORT=8080 bin/run                 # build + run through compose, Ctrl-C to stop
docker compose up --build             # the same, by hand
curl localhost:8080/health
curl localhost:8080/api/time
```

### Without docker

```sh
# Debian/Ubuntu: sudo apt install build-essential cmake libpoco-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
PORT=8080 ./build/app
# or: FLEET_RUNTIME=process PORT=8080 bin/run
```

`fleet.conf` drives every script in `bin/`:

| step | docker runtime (fleet) | `FLEET_RUNTIME=process` |
|---|---|---|
| install | — | `(none)` |
| build | `docker compose build` | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j` |
| start | `docker compose up --remove-orphans` | `env PORT="$PORT" ./build/app` |

## Layout

- `CMakeLists.txt` — one executable target `app`, linked to `Poco::Foundation`, `Poco::Net`, `Poco::Util`, `Poco::JSON`.
- `src/main.cpp` — the handlers, the factory that routes by path, and the `ServerApplication`.
- `Dockerfile` — `debian:trixie` build stage with `libpoco-dev`; `debian:trixie-slim` runtime with only `libpocofoundation100`, `libpoconet100`, `libpocoutil100`, `libpocojson100`; non-root user `app`.
- `compose.yaml` — service `app`, publishes `${PORT:-8080}:${PORT:-8080}`, fleet variables passed through by name.

## Deviations from stock, and why

- HTTPTimeServer reads its port from a properties file (`HTTPTimeServer.port`, default 9980) and serves one HTML page; this reads `$PORT` from the environment at runtime (default 8080), binds `0.0.0.0`, and routes several paths, JSON via `Poco::JSON`.
- Added a `/health` route for the fleet's health check.
- No `.properties` config file and no command-line option handling (`defineOptions`/`handleOption`), to keep the starter small.

## Verified

2026-10-05, Docker 29.8 on linux/amd64:

- `verify.sh <dir> 46508` (the migrate-docker-runtime skill's end-to-end check) → `run=200 restart=200 containers_after_stop=0`.
- `migrate.py audit <dir>` → `READY`.
- `docker compose up` with `PORT=46508`: `GET /` → 200 `Hello from the POCO template!`, `GET /health` → `{"status":"ok"}`. (`/api/time` was not exercised.)

The no-docker path (`FLEET_RUNTIME=process`) was not run on a host toolchain; it is the same CMake build the image runs.

See `docs/fleet-lifecycle.md` for the lifecycle contract.
