#!/usr/bin/env bash
set -euo pipefail

# Build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j

# Run (adjust credentials as needed)
./redis_mysql_ms \
  --port 8081 \
  --redis-host 127.0.0.1 --redis-port 6379 --redis-db 0 \
  --mysql-host 127.0.0.1 --mysql-port 3306 \
  --mysql-user root --mysql-password "" --mysql-db cpp_server \
  --mysql-pool-size 4 --default-ttl 300