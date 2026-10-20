# 前序编码器

论文所称前序编码器，指本课题组在同一条研究线上迭代得到的编码器：在 FLIF 残差二值化框架上叠加五路专家（结构迁移、一维与二维匹配、软上下文、几何条件化调色板、在线 L1 预测）。前序编码器与本文方法共用同一份源码，差异仅由环境变量控制。

| 版本 | 概率组合方式 | 极性规范化 | 关键开关 | 二进制 | 论文中的位置 |
|---|---|---|---|---|---|
| v12 | 五路专家串联组合、逐级加权合并 | 无 | `SMEM_LMIX=0` | `bin/flif_v12repro` | 第 3 节；第 6.3 节内部消融的基准 |
| v17 | 三路门控混合、末级混合与两级 APM | 无 | `SMEM_LMIX=4`、`SMEM_PCC=0` | `bin/flif_v17_final` | 第 6.2、6.3 节“去掉极性规范化”消融 |
| v18 | 三路门控混合、末级混合与两级 APM | 有 | `SMEM_LMIX=4`、`SMEM_PCC=2` | `../../bin/flif_v18_final` | 本文方法 |

## 使用

预编译二进制为 macOS arm64，命令行参数与 `bin/flif_v18_final` 一致。

```bash
# 前序编码器（v12，串联组合）
SMEM_LMIX=0 baselines/bin/flif_v12repro -e -N -E60 -P0 -C -R0 image.pgm out.flif
SMEM_LMIX=0 baselines/bin/flif_v12repro -d out.flif restored.pgm

# v17（门控混合，关闭极性规范化）
SMEM_LMIX=4 SMEM_PCC=0 baselines/bin/flif_v17_final -e -N -E60 -P0 -C -R0 image.pgm out.flif
```

## 其他平台重建

前序编码器不需要单独的历史源码，与 v18 共用 `codec/flif_pcc`：

```bash
bash codec/build.sh
SMEM_LMIX=0 codec/flif_pcc/src/flif -e -N -E60 -P0 -C -R0 image.pgm out.flif        # v12
SMEM_LMIX=4 SMEM_PCC=0 codec/flif_pcc/src/flif -e -N -E60 -P0 -C -R0 image.pgm out.flif  # v17
```

参数的完整定义见 `experiments/configs/v18_configs.json` 中的 `base_env` 与 `abl_noPCC_v17`，命令行工具 `codec/pcc_codec.py --config v12|v17|v18` 已封装上述组合。

## 与 v18 的逐字节关系

关闭新增模块时输出与旧版本逐字节一致：`SMEM_PCC=0` 得到 v17；在 v17 基础上令 `SMEM_LMIX=0` 得到 v12。因此消融实验无需切换二进制，仅需切换环境变量。
