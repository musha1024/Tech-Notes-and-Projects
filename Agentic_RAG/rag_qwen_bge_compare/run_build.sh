#!/usr/bin/env bash
set -euo pipefail
# pip install -r requirements.txt --no-cache-dir
python document_processor.py --in_dir ./data/raw --out_dir ./data/processed --chunk_size 900 --overlap 120
python chroma_builder.py --processed_dir ./data/processed --chroma_dir ./chroma_db --collection default
echo "[OK] Build finished."
