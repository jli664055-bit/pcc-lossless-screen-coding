# 实验

## 目录

- `configs/v18_configs.json`：冻结配置。`v18_final` 为本文方法，`abl_noPCC_v17`、`abl_capacity`、`abl_hard`、`abl_nonorm` 为消融配置。
- `configs/manifest_all_relative.json`：4392 张实验图像与开发图像的清单，路径相对仓库根目录。
- `configs/v17_ablation.json`：v17 阶段的逐级消融配置。
- `results/v18/`：
  - `ours_*.json`：本文方法及各消融的逐图结果，字段为 `bytes`、`lossless`、`enc_s`、`dec_s`。
  - `base_*.json`：已有算法的逐图结果，字段 `source` 标明本次运行或沿用前序阶段同参数记录。
  - 汇总文件：`v18_vs_existing.csv`、`v18_ablation.csv`、`bpp_v18.json`、`polarity_penalty.json`。
  - 计时：`serial_v18_cpu.json`、`serial_extra_cpu.json`；`serial_v18.json` 为一次高负载下的墙钟计时，已弃用，仅作存档。
- `results/v17/`：v17 门控上下文混合阶段的全部结果。
- `results/earlier/`：v11、v12 阶段复用的基线记录与清单。
- `scripts/`：v18 与 v17 阶段实际使用的脚本。`scripts/v18/run_ours.py` 已适配仓库目录结构；其余脚本中的部分路径对应作者本地工作区布局，作为研究过程记录保留。

## 复现

```bash
python experiments/summarize.py              # 论文表 6.1
python experiments/summarize.py --polarity   # 图 1.1 的极性代价
```

重跑逐图实验（需先按 `data/README.md` 准备数据）：

```bash
python experiments/scripts/v18/run_ours.py v18_final dev_mix --jobs 4
python experiments/scripts/v18/run_ours.py v18_final SCID,SIQAD,Real20,Kodak,Contrast32,MixHalf,MixBand,Synth,DIV2K100g --jobs 6
```

脚本可恢复：已完成的逐图记录会被跳过。

## 说明

所有压缩结果均经完整解码与逐像素比对。计时在 macOS arm64（10 核、16 GB 内存）上串行测得。`configs/manifest_all_relative.json` 中缺失的公开数据集目录需先按 `data/README.md` 重建。
