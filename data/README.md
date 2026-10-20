# 数据

实验使用 9 组共 4392 张 8 位灰度图像，另有 12 张开发图像（6 张开发图及由其构造的 6 张混合主题开发图）。

| 数据 | 张数 | 来源 | 本仓库提供方式 |
|---|---:|---|---|
| 合成文字 | 4000 | 本文生成，1000 组 × 4 种对比度条件（统一黑白、中等、低对比度、明暗混合） | 直接提供 `synthetic_text/` |
| 开发集混合主题 | 6 | 由 6 张开发图构造，仅用于参数选择 | 直接提供 `dev_mix/` |
| 混合主题示例 | 4 | 由 SCID、SIQAD 派生 | 直接提供 `mixed_samples/` |
| SCID | 40 | Ni 等，IEEE TIP 2017 | 脚本重建 |
| SIQAD | 20 | Yang 等，IEEE TIP 2015 | 脚本重建 |
| Kodak | 24 | Kodak Lossless True Color Image Suite | 脚本重建 |
| DIV2K 灰度 | 100 | DIV2K 验证集 0801–0900 | 脚本重建 |
| 跨对比度 | 32 | 8 张源图的 512×512 裁剪 × 4 种对比度 | 脚本重建（需上述数据） |
| 混合主题·拼接 / 分带 | 75 / 75 | 由 SCID、SIQAD 与真实截图构造 | 脚本重建；由真实截图构造的 38 张无法重建 |
| 真实浏览器截图 | 20 | 作者采集 | 不公开（涉及个人浏览内容） |

## 重建步骤

1. 自行获取 SCID（ReferenceSCIs）、SIQAD（reference images）、Kodak（kodim01–24.png）与 DIV2K 验证集高分辨率图像（0801–0900）。
2. 执行：

```bash
python data/scripts/prepare_data.py --scid <SCID/ReferenceSCIs> --siqad <SIQAD/references> \
       --kodak <Kodak 目录> --div2k <DIV2K_valid_HR>
```

脚本以 Pillow `convert('L')` 将彩色原图转为灰度（保持原分辨率，不裁剪、不缩放），生成跨对比度与混合主题派生图像，并按 `checksums/gray_pgm_sha256.json` 逐一校验。作者在本机原始数据上运行时 366 张图像全部一致。

`scripts/reference_*.py` 为原始生成脚本，仅供查阅。合成文字依赖于 macOS 系统字体，在其他平台上无法逐像素复现，故随仓库直接提供生成结果。

## 随仓库提供的子集

| 目录 | 内容 | 用途 |
|---|---|---|
| `synthetic_text/` | 4000 张合成文字图像（1000 组 × 4 条件） | 论文表 6.1、表 6.2 的 Synth 结果 |
| `dev_mix/` | 6 张混合主题开发图像（3 张拼接、3 张分带） | 参数选择，不参与最终评估 |
| `mixed_samples/` | 2 张拼接、2 张分带示例 | 快速验证与图 1.1 的复现入口 |

开发集与混合主题示例由 SCID、SIQAD 派生，版权归原数据集作者，仅限学术使用。

## 许可提示

SCID、SIQAD、Kodak 与 DIV2K 均仅限学术使用，本仓库不重分发其原始图像；`prepare_data.py` 需要使用者自行下载。由这些数据集派生的混合主题图像同样仅限学术使用。
