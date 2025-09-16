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
            device_map="auto" if device == "auto" else None
        )
        if device != "auto" and device in {"cuda", "cpu"}:
            self.model.to(device)
        self.temperature = temperature
        self.top_p = top_p
        self.max_new_tokens = max_new_tokens

    def generate(self, prompt: str) -> str:
        input_ids = self.tokenizer(prompt, return_tensors="pt").to(self.model.device)
        with torch.no_grad():
            outputs = self.model.generate(
                **input_ids,
                do_sample=True,
                temperature=self.temperature,
                top_p=self.top_p,
                max_new_tokens=self.max_new_tokens,
                eos_token_id=self.tokenizer.eos_token_id,
            )
        text = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
        # 返回生成的后缀部分（去掉前缀提示词）
        if text.startswith(prompt):
            return text[len(prompt):]
        return text

def load_llm(model_path: str, device: str = "auto", dtype: str = "auto",
             temperature: float = 0.7, top_p: float = 0.95, max_new_tokens: int = 1024) -> QwenLLM:
    return QwenLLM(
        model_path=model_path,
        device=device,
        dtype=dtype,
        temperature=temperature,
        top_p=top_p,
        max_new_tokens=max_new_tokens,
    )
