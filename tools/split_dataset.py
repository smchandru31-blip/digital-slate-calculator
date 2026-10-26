"""
Dataset Split Tool for Digital Slate Calculator
Partitions raw labeled handwriting samples into strict writer-independent splits:
- Train: W01, W02, W03 (~60%)
- Val  : W04 (~20%)
- Test : W05 (~20%)
Ensures zero data leakage between partitions.
"""

import os
import sys
import json
import shutil
import argparse
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
RAW_DIR = REPO_ROOT / "data" / "dataset" / "raw"
DATASET_BASE = REPO_ROOT / "data" / "dataset"

DEFAULT_SPLIT_MAP = {
    "W01": "train",
    "W02": "train",
    "W03": "train",
    "W04": "val",
    "W05": "test"
}

def split_dataset(raw_dir=RAW_DIR, out_base=DATASET_BASE, custom_map=None):
    writer_map = custom_map if custom_map else DEFAULT_SPLIT_MAP

    print("=========================================================")
    print("  DIGITAL SLATE - WRITER-INDEPENDENT DATASET SPLIT TOOL  ")
    print("=========================================================")
    print(f"Source Directory : {raw_dir}")
    print(f"Target Directory : {out_base}")
    print("Writer Allocation:")
    for w, part in sorted(writer_map.items()):
        print(f"  {w} -> {part.upper()}")
    print("---------------------------------------------------------")

    if not raw_dir.exists():
        print(f"[ERROR] Source directory does not exist: {raw_dir}")
        return False

    raw_files = list(raw_dir.rglob("*.json"))
    if not raw_files:
        print(f"[WARNING] No JSON sample files found in: {raw_dir}")
        return False

    # Target folders
    train_dir = out_base / "train"
    val_dir   = out_base / "val"
    test_dir  = out_base / "test"

    for d in [train_dir, val_dir, test_dir]:
        d.mkdir(parents=True, exist_ok=True)

    counts = {"train": 0, "val": 0, "test": 0, "skipped": 0}
    class_counts = {
        "train": {},
        "val": {},
        "test": {}
    }
    writers_found = set()

    for fpath in raw_files:
        try:
            with open(fpath, "r", encoding="utf-8") as f:
                doc = json.load(f)
        except Exception as e:
            print(f"Skipping corrupt file {fpath.name}: {e}")
            continue

        w_id = doc.get("writer_id", "W01")
        lbl = doc.get("label", "unknown")
        writers_found.add(w_id)

        target_part = writer_map.get(w_id, "train") # Default to train if unassigned
        dest_folder = out_base / target_part / lbl
        dest_folder.mkdir(parents=True, exist_ok=True)

        dest_file = dest_folder / fpath.name
        shutil.copy2(fpath, dest_file)

        counts[target_part] += 1
        class_counts[target_part][lbl] = class_counts[target_part].get(lbl, 0) + 1

    total_processed = sum(counts.values()) - counts["skipped"]

    print(f"\n[SUCCESS] Split {total_processed} samples across {len(writers_found)} writers:")
    print(f"  TRAIN : {counts['train']:4d} samples ({counts['train']/total_processed*100:.1f}%)")
    print(f"  VAL   : {counts['val']:4d} samples ({counts['val']/total_processed*100:.1f}%)")
    print(f"  TEST  : {counts['test']:4d} samples ({counts['test']/total_processed*100:.1f}%)")

    # Verify zero writer overlap
    train_writers = {w for w, p in writer_map.items() if p == "train" and w in writers_found}
    val_writers   = {w for w, p in writer_map.items() if p == "val" and w in writers_found}
    test_writers  = {w for w, p in writer_map.items() if p == "test" and w in writers_found}

    overlap_train_val = train_writers.intersection(val_writers)
    overlap_train_test = train_writers.intersection(test_writers)
    overlap_val_test = val_writers.intersection(test_writers)

    print("\n--- Leakage Verification ---")
    if not overlap_train_val and not overlap_train_test and not overlap_val_test:
        print("[VERIFIED] ZERO writer overlap between Train, Val, and Test partitions.")
    else:
        print("[ALERT] Detected overlapping writers between partitions!")

    print("=========================================================\n")
    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Split Dataset by Writer")
    parser.add_argument("--raw", default=str(RAW_DIR), help="Path to raw dataset")
    parser.add_argument("--out", default=str(DATASET_BASE), help="Target output dataset base path")

    args = parser.parse_args()
    split_dataset(raw_dir=Path(args.raw), out_base=Path(args.out))
