#!/bin/bash
# run_agent_openai.sh
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)/local_agent_qwen"
VENV_DIR="$PROJECT_DIR/.venv"

# 检查API KEY
if [ -z "$OPENAI_API_KEY" ]; then
  echo "[ERROR] 缺少 OPENAI_API_KEY 环境变量"
  echo "  用法: OPENAI_API_KEY=sk-xxx ./run_agent_openai.sh"
  exit 1
fi

# 虚拟环境
if [ ! -d "$VENV_DIR" ]; then
  python3 -m venv "$VENV_DIR"
fi
source "$VENV_DIR/bin/activate"

pip install --upgrade pip
pip install -r "$PROJECT_DIR/requirements.txt"

echo "[INFO] 启动基于 OpenAI 的 Agent ..."
python "$PROJECT_DIR/cli.py"
