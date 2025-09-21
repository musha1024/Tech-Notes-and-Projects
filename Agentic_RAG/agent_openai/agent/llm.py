
from transformers import AutoTokenizer, AutoModelForCausalLM
import torch

class QwenLLM:
    def __init__(self, model_path: str, device: str = "auto", dtype: str = "auto",
                 temperature: float = 0.7, top_p: float = 0.95, max_new_tokens: int = 1024):
        self.tokenizer = AutoTokenizer.from_pretrained(model_path, trust_remote_code=True)
        torch_dtype = None
        if dtype == "float16":
            torch_dtype = torch.float16
        elif dtype == "bfloat16":
            torch_dtype = torch.bfloat16
        elif dtype == "float32":
            torch_dtype = torch.float32
        # else: auto

        self.model = AutoModelForCausalLM.from_pretrained(
            model_path,
            torch_dtype=torch_dtype,
            trust_remote_code=True
        )
        if device == "cuda":
            self.model = self.model.to("cuda")
        elif device == "cpu":
            self.model = self.model.to("cpu")
        else:  # auto
            self.model = self.model.to("cuda" if torch.cuda.is_available() else "cpu")

        self.temperature = temperature
        self.top_p = top_p
        self.max_new_tokens = max_new_tokens

    def generate(self, messages):
        prompt = ""
        for m in messages:
            role = m["role"]
            content = m["content"]
            if role == "system":
                prompt += f"[系统]\n{content}\n"
            elif role == "user":
                prompt += f"[用户]\n{content}\n"
            elif role == "assistant":
                prompt += f"[助手]\n{content}\n"
        inputs = self.tokenizer(prompt, return_tensors="pt").to(self.model.device)
        outputs = self.model.generate(
            **inputs,
            do_sample=True,
            temperature=self.temperature,
            top_p=self.top_p,
            max_new_tokens=self.max_new_tokens
        )
        text = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
        if text.startswith(prompt):
            return text[len(prompt):]
        return text

def load_llm(**kwargs):
    """
    统一入口：
    - provider: "qwen" | "openai"（默认 qwen）
    - 当 provider=openai 时，使用 agent.llm_openai.OpenAILLM
      需要：model, temperature, top_p, max_tokens, api_base(可选), api_key(可选/或用环境变量)
    - 当 provider=qwen 时，沿用本地 Qwen 加载逻辑，需要：model_path, device, dtype, temperature, top_p, max_tokens
    """
    provider = kwargs.get("provider", "qwen").lower()
    if provider == "openai":
        from agent.llm_openai import OpenAILLM
        return OpenAILLM(
            model=kwargs.get("model", "gpt-4o-mini"),
            temperature=kwargs.get("temperature", 0.2),
            top_p=kwargs.get("top_p", 0.95),
            max_tokens=kwargs.get("max_tokens", 1024),
            api_base=kwargs.get("api_base"),
            api_key=kwargs.get("api_key"),
        )
    else:
        return QwenLLM(
            model_path=kwargs.get("model_path"),
            device=kwargs.get("device", "auto"),
            dtype=kwargs.get("dtype", "auto"),
            temperature=kwargs.get("temperature", 0.7),
            top_p=kwargs.get("top_p", 0.95),
            max_new_tokens=kwargs.get("max_tokens", 1024),
        )
