# llama.cpp 构建/部署核心踩坑速记
> 2025-09-08 07:53

**环境**：Ubuntu 18.04、CUDA 11.8、GCC 7.5、RTX 3090×4

## 关键问题 → 直接解法
1) **CMake 选到旧 nvcc（/usr/bin/nvcc, CUDA 9.x）**
   - 现象：`__CUDACC_VER_MAJOR__=9`、`__float128` 报错
   - 解法：`export PATH=/usr/local/cuda/bin:$PATH CUDACXX=/usr/local/cuda/bin/nvcc`；`cmake -DGGML_CUDA=ON -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc`

2) **缺 CURL 头库**
   - 现象：`Could NOT find CURL`
   - 解法：装 `libcurl4-openssl-dev` 或 `-DLLAMA_CURL=OFF`

3) **GCC 7.5 太旧（<_mm256_set_m128, <charconv>）**
   - 现象：SIMD 内联未声明；`fatal error: charconv`
   - 解法（其一）：装 gcc-11/g++-11（bionic 的 toolchain PPA），并 `-DCUDAHOSTCXX=/usr/bin/g++-11`
   - 解法（其二）：暂关 CPU 高级 SIMD：`-DGGML_AVX512=OFF -DGGML_AVX2=OFF -DGGML_F16C=OFF`

4) **APT 源混杂（mantic/jammy 混入 bionic）**
   - 现象：`gcc-11-base` 版本冲突
   - 解法：删掉非 bionic 源；只保留 bionic 的 `ubuntu-toolchain-r/test`；必要时精确锁版本安装

5) **Docker 权限/ssh-agent 提示**
   - 现象：`Could not open a connection to your authentication agent.`
   - 解法：忽略；确保加入 `docker` 组并重新登录：`id -nG | grep -w docker`

6) **拉镜像超时**
   - 解法：临时前缀 `docker.m.daocloud.io/…`；或在 `/etc/docker/daemon.json` 配置 `registry-mirrors`

7) **容器内无 GPU**
   - 现象：`could not select device driver "" with capabilities: [[gpu]]`
   - 解法：安装并配置 **NVIDIA Container Toolkit**；`sudo nvidia-ctk runtime configure --runtime=docker && sudo systemctl restart docker`；`docker run --gpus all … nvidia-smi` 自检
