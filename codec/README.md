# 编码器源码

`flif_pcc/` 在 FLIF v0.4（Jon Sneyers、Pieter Wuille）的基础上修改，保留其残差二值化与二进制算术编码器，替换概率组合层并增加跨极性统计。新增模块均位于 `flif_pcc/src/`，按版本列于下表。

| 文件 | 作用 | 版本 |
|---|---|---|
| `structure_memory.hpp`、`match_model.hpp`、`soft_model.hpp`、`palette_model.hpp`、`online_model.hpp`、`probability_transport.hpp` | 五路前序专家：对比度与极性归一化的结构迁移、一维与二维匹配、软上下文、几何条件化调色板、在线 L1 预测，以及串联组合 | v1–v12 |
| `logistic_mixer.hpp` | 三路门控 logistic 混合器、末级混合、两级 APM/SSE | v17 |
| `cm_model.hpp` | 20 个直接上下文模型、4 个邻域一致性上下文及其规范坐标系副本 | v17、v18 |
| `hist_model.hpp` | 8 个值域直方图专家（路径一致性投影）及其规范副本 | v17、v18 |
| `polarity_model.hpp` | 因果极性估计（长水平游程背景与滞回） | v18 |

各模块由环境变量控制，完整参数见 `../experiments/configs/v18_configs.json`。关闭新增模块时输出与对应旧版本逐字节一致：`SMEM_PCC=0` 对应 v17，在此基础上令 `SMEM_LMIX=0` 对应 v12。

## 编译

```bash
bash codec/build.sh
```

依赖 clang++（或 g++）、make 与 zlib；产物为 `codec/flif_pcc/src/flif`。

## 命令行工具

```bash
python codec/pcc_codec.py encode image.png image.pcc --config v18   # 或 v17、v12
python codec/pcc_codec.py decode image.pcc restored.pgm
```

工具仅接受 8 位灰度输入，文件头 8 字节标识配置，其后为 FLIF 码流。未编译时工具自动使用预编译的 `bin/flif_v18_final`。

## 预编译二进制与版本切换

- `../bin/flif_v18_final`：本文方法（macOS arm64）。
- `../baselines/bin/flif_v17_final`、`../baselines/bin/flif_v12repro`：前序编码器。

```bash
# v18：门控混合 + 极性规范化
SMEM_LMIX=4 SMEM_PCC=2 bin/flif_v18_final -e -N -E60 -P0 -C -R0 img.png out.flif
# v17：门控混合，关闭极性规范化
SMEM_LMIX=4 SMEM_PCC=0 bin/flif_v18_final -e -N -E60 -P0 -C -R0 img.png out.flif
# v12：前序串联编码器
SMEM_LMIX=0 bin/flif_v18_final -e -N -E60 -P0 -C -R0 img.png out.flif
```

## 许可

FLIF 的原始许可证文件随源码保留于 `flif_pcc/`（LGPLv3、Apache-2.0、GPLv3），修改部分按相同条款发布。`extern/lodepng` 沿用其自带许可。
