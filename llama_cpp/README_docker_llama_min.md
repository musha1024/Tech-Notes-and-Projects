# 从 Docker 到 llama.cpp 推理（极简版）
> 2025-09-08 07:53 · 3090/24G 为例（SM=86）

## A. 仅一次的准备
1. **Docker + NVIDIA Toolkit**
```bash
sudo apt-get install -y docker.io wget gnupg
wget -qO - https://nvidia.github.io/libnvidia-container/gpgkey | sudo gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg
distribution=ubuntu18.04
wget -qO - https://nvidia.github.io/libnvidia-container/$distribution/libnvidia-container.list |   sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#' |   sudo tee /etc/apt/sources.list.d/nvidia-container-toolkit.list >/dev/null
sudo apt-get update && sudo apt-get install -y nvidia-container-toolkit
sudo nvidia-ctk runtime configure --runtime=docker || true
sudo systemctl restart docker
# GPU 自检（国内加速域名）
docker run --rm --gpus all docker.m.daocloud.io/nvidia/cuda:11.8.0-base-ubuntu22.04 nvidia-smi
```

2. **（可选）镜像加速**
```bash
sudo tee /etc/docker/daemon.json <<'JSON'
{"registry-mirrors":["https://docker.m.daocloud.io"],"dns":["8.8.8.8","8.8.4.4"]}
JSON
sudo systemctl restart docker
```

## B. 准备目录
```bash
mkdir -p /mnt/sda/tmh/llama_docker/{models,cache}
```

## C. Dockerfile（GPU）
`Dockerfile.cuda`：
```dockerfile
FROM docker.m.daocloud.io/nvidia/cuda:11.8.0-devel-ubuntu22.04
ARG DEBIAN_FRONTEND=noninteractive
ARG CUDA_ARCHS=86
RUN apt-get update && apt-get install -y --no-install-recommends git cmake build-essential pkg-config ca-certificates libcurl4-openssl-dev wget && rm -rf /var/lib/apt/lists/*
WORKDIR /opt
RUN git clone https://github.com/ggml-org/llama.cpp && cd llama.cpp &&     cmake -B build -S . -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=${CUDA_ARCHS} &&     cmake --build build -j &&     ln -s /opt/llama.cpp/build/bin/llama-server /usr/local/bin/llama-server
ENV LLAMA_CACHE=/cache
VOLUME ["/cache","/models"]
EXPOSE 8080
CMD ["llama-server","-h"]
```

## D. 构建
```bash
export SM=$(nvidia-smi --query-gpu=compute_cap --format=csv,noheader | head -n1 | awk -F. '{print $1$2}')
docker build -f Dockerfile.cuda --build-arg CUDA_ARCHS=${SM:-86} -t llama-cuda:11.8 .
```

## E. 运行（两选一）
**1) 本地 GGUF**
```bash
# 将 *.gguf 放入 /mnt/sda/tmh/llama_docker/models
docker run --rm -it --gpus all -e CUDA_VISIBLE_DEVICES=0 -p 8080:8080   -v /mnt/sda/tmh/llama_docker/models:/models   -v /mnt/sda/tmh/llama_docker/cache:/cache   llama-cuda:11.8   llama-server -m /models/你的模型.gguf -c 8192 --n-gpu-layers 60 --port 8080
```
**2) 在线拉（-hf）**
```bash
docker run --rm -it --gpus all -e CUDA_VISIBLE_DEVICES=0 -p 8080:8080   -v /mnt/sda/tmh/llama_docker/cache:/cache -e HF_TOKEN=你的token_可选   llama-cuda:11.8   llama-server -hf ggml-org/gemma-3-1b-it-GGUF -c 8192 --n-gpu-layers 60 --port 8080
```

## F. API 自测
```bash
docker run --rm curlimages/curl:8.8.0 -s http://127.0.0.1:8080/v1/chat/completions   -H "Content-Type: application/json" -H "Authorization: Bearer sk-noauth"   -d '{"model":"local","messages":[{"role":"user","content":"你好"}]}'
```

> 备注：24GB 显存建议从 `--n-gpu-layers 60`、`-c 8192` 起步；显存不足时先降 `--n-gpu-layers` 或 `-c`。多 GPU 可试 `--tensor-split`，但 PCIe 多卡常不如单卡稳/快。
