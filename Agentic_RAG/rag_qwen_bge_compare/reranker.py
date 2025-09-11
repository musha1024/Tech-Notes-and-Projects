from sentence_transformers.cross_encoder import CrossEncoder
class Reranker:
    def __init__(self, model_path: str, device: str = "cpu"):
        self.model = CrossEncoder(model_path, device=device)
    def rerank(self, query, items, top_r=4):
        if not items: return items
        scores = self.model.predict([(query, it["text"]) for it in items]).tolist()
        for it, s in zip(items, scores):
            it["rerank_score"] = float(s)
        items.sort(key=lambda x: x.get("rerank_score", 0.0), reverse=True)
        return items[:top_r]
