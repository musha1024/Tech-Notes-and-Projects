#!/usr/bin/env bash
set -euo pipefail
python rag_compare.py --chroma_dir ./chroma_db --collection default
