# RAG 对比包（Base vs RAG）

新增：
- **更长输出**：`max_new_tokens` 默认 512，避免半句被截断。
- **输出终清理**：`post_finalize()` 去掉多余引号、把 `R A G` 折叠为 `RAG`，并在 `<|im_end|>` / `</s>` 处安全截断。
- 继续保留 v2 的：采样告警清理、中文去空格、bad_words_ids、无括号模板、FP16 默认。

用法：
```bash
pip install -r requirements.txt --no-cache-dir
bash run_build.sh
bash run_compare.sh
```
