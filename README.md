# 基于门控混合与跨极性共享的屏幕内容无损压缩方法

《多媒体技术基础》课程论文配套的编码器源码、实验数据与复现说明。英文题目：Gated Mixing and Cross-Polarity Sharing for Lossless Screen Content Compression。

> **论文不在本仓库中。** 课程论文（Word 版）随课程作业单独提交，不随本仓库发布；仓库仅提供可复现的源码、数据、实验结果与文档。论文的获取方式见下文“论文说明”。

## 论文说明

- 本仓库**不包含**论文正文（.docx）、论文源稿或排版文件，也不计划随仓库发布；
- 论文与本文档描述的方法、实验一一对应，数据与逐图结果均可在仓库内复现；
- 如需论文全文，请通过课程作业提交系统获取，或通过仓库 Issue 联系作者。

## 仓库内容

```
├── codec/                  研究编码器源码（基于 FLIF v0.4）、构建脚本与命令行工具
├── bin/                    v18 预编译编码器（macOS arm64）
├── baselines/              前序编码器：v17（门控，无极性规范化）与 v12（串联组合）的二进制
├── experiments/
│   ├── configs/            冻结配置（v18_final 及消融）与相对路径数据清单
│   ├── scripts/            v18 与 v17 阶段实际使用的脚本
│   ├── results/            逐图结果（字节数、无损校验、计时）与汇总表
│   └── summarize.py        由逐图结果重算主表
├── data/
│   ├── synthetic_text/     合成文字图像 4000 张
│   ├── dev_mix/            开发集混合主题图像 6 张
│   ├── mixed_samples/      混合主题示例 4 张
│   ├── scripts/            由公开数据集重建实验图像的脚本
│   └── checksums/          实验图像的 SHA-256
└── docs/                   方法说明、结果报告、研究历程、复现指南、文献综述
```

## 主要结果

全部输出经完整解码并逐像素比对，均为无损。相对 JPEG XL（effort 10）的文件中位节省如下。

| 数据 | 张数 | 相对 JPEG XL e10 | 相对 paq8px |
|---|---:|---:|---:|
| SCID 屏幕内容 | 40 | 19.08% | −11.72% |
| SIQAD 屏幕内容 | 20 | 15.95% | −12.95% |
| 真实浏览器截图 | 20 | 8.43% | −8.15% |
| 跨对比度 | 32 | 17.92% | −11.55% |
| 混合主题·拼接 | 75 | 10.79% | −11.12% |
| 混合主题·分带 | 75 | 14.14% | −8.99% |
| 合成文字（1000 组 × 4 条件） | 4000 | 25.77% | 未运行 |
| Kodak / DIV2K 自然图像 | 24 / 100 | 2.78% / 0.37% | −6.14% / 未运行 |

其他结果：

1. 极性代价由去掉极性规范化时的 1.12% 降至 0.73%；在同容量对照下，分带混合主题的纯极性增益为 0.45%（74 胜 0 负）。
2. 不增加新专家、仅将前序串联组合替换为门控混合，98 张留出图像即节省 2.81%。
3. 在 5 张代表图像上编码耗时为 3.9 秒/百万像素，JPEG XL effort 10 为 4.7 秒；平均比特率 0.882 bpp，较 paq8px 高 8%；解码速度约为 paq8px 的 5 至 7 倍。
4. 方法仅支持 8 位灰度；自然图像上与 JPEG XL 最高档基本持平；较 paq8px 仍大约 9% 至 13%；解码速度显著慢于 JPEG XL。

逐图结果位于 `experiments/results/v18/`，执行 `python experiments/summarize.py` 可由逐图数据重算主结果表（论文表 6.1）。

## 编码器版本

三个版本由同一份源码编译，通过环境变量切换，输出逐字节可对应。

| 版本 | 概率组合方式 | 极性规范化 | 关键开关 | 二进制 |
|---|---|---|---|---|
| v12 | 五路专家串联组合（前序编码器） | 无 | `SMEM_LMIX=0` | `baselines/bin/flif_v12repro` |
| v17 | 三路门控混合、末级混合与两级 APM | 无 | `SMEM_LMIX=4`、`SMEM_PCC=0` | `baselines/bin/flif_v17_final` |
| v18 | 门控混合 | 有 | `SMEM_LMIX=4`、`SMEM_PCC=2` | `bin/flif_v18_final` |

各版本的参数、使用命令与复现方式见 [codec/README.md](codec/README.md) 与 [baselines/README.md](baselines/README.md)。

## 使用方法

```bash
pip install -r requirements.txt
bash codec/build.sh                                          # 依赖 clang++/g++、make、zlib
python codec/pcc_codec.py encode data/mixed_samples/band_real01.pgm out.pcc --config v18
python codec/pcc_codec.py decode out.pcc restored.pgm        # 与原图逐像素一致
python experiments/summarize.py                               # 重算主结果表
```

亦可直接调用预编译二进制（macOS arm64）：

```bash
SMEM_LMIX=4 SMEM_PCC=2 bin/flif_v18_final -e -N -E60 -P0 -C -R0 \
    data/dev_mix/devband_SCID_SCI05.pgm out.flif
```

## 数据

合成文字图像、开发集与混合主题示例随仓库提供。SCID、SIQAD、Kodak 与 DIV2K 属第三方数据集，仓库不重分发，需自行下载后执行 `data/scripts/prepare_data.py` 重建实验图像并按 SHA-256 校验；真实浏览器截图涉及个人浏览内容，不公开，仓库仅保留其压缩结果。详见 [data/README.md](data/README.md)。

## 文档

- [docs/方法说明.md](docs/方法说明.md)：极性规范化与门控混合的方法细节、参数选择与对照方案。
- [docs/结果报告.md](docs/结果报告.md)：全部实验表、消融、计时与局限。
- [docs/研究历程.md](docs/研究历程.md)：v1 至 v18 的路线与取舍依据。
- [docs/复现指南.md](docs/复现指南.md)：从数据准备到结果复现的完整步骤。
- [docs/文献综述与查新.md](docs/文献综述与查新.md)：相关工作对比与查新结论。

## 许可

仓库内脚本与文档以 MIT 许可发布，见 [LICENSE](LICENSE)。`codec/flif_pcc/` 基于 FLIF v0.4，沿用其原始许可证（LGPLv3、Apache-2.0、GPLv3），见 [NOTICE.md](NOTICE.md)。第三方数据集版权归原作者所有，仅限学术使用。
