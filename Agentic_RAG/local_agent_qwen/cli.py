import argparse
import sys
from agent.agent import LocalAgent
from agent.llm import load_llm
from agent.memory import BufferMemory
import yaml
from pathlib import Path

def load_config():
    cfg_path = Path(__file__).parent / "config.yaml"
    with open(cfg_path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)

def make_agent():
    cfg = load_config()
    llm = load_llm(
        model_path=cfg.get("model_path"),
        device=cfg.get("device", "auto"),
        dtype=cfg.get("dtype", "auto"),
        temperature=cfg.get("temperature", 0.7),
        top_p=cfg.get("top_p", 0.95),
        max_new_tokens=cfg.get("max_tokens", 1024),
    )
    memory = BufferMemory(capacity=10)
    agent = LocalAgent(llm=llm, memory=memory)
    return agent

def cmd_chat_once(agent: LocalAgent, content: str):
    reply = agent.chat(content)
    print(reply)

def cmd_chat_loop(agent: LocalAgent):
    print("==== Local Agent (Qwen) ====")
    print("输入你的问题（'exit' 退出）：")
    while True:
        try:
            user = input("\n❓ 问题: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n再见！")
            break
        if user.lower() in {"exit", "quit"}:
            print("再见！")
            break
        reply = agent.chat(user)
        print("\n—— 回答 ——")
        print(reply)

def main():
    parser = argparse.ArgumentParser(description="Local Agent (Qwen)")
    sub = parser.add_subparsers(dest="cmd")

    p_chat = sub.add_parser("chat", help="与 Agent 对话")
    p_chat.add_argument("-c", "--content", type=str, help="单轮提问内容")

    args = parser.parse_args()
    agent = make_agent()

    if args.cmd == "chat":
        if args.content:
            cmd_chat_once(agent, args.content)
        else:
            cmd_chat_loop(agent)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
