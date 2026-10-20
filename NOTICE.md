# 第三方组件与许可

- `codec/flif_pcc/`：修改自 FLIF v0.4（Jon Sneyers、Pieter Wuille，https://github.com/FLIF-hub/FLIF）。原始许可证文件 `LICENSE`、`LICENSE_LGPL`、`LICENSE_Apache2`、`LICENSE_GPL` 随源码保留，修改部分按相同条款发布；`extern/lodepng` 沿用其自带许可。
- 对照算法为外部软件，本仓库不包含其源码：libjxl 0.12.0、libwebp 1.6.0、CharLS 与 OpenJPEG（通过 imagecodecs 调用）、paq8px v216。
- 数据集 SCID、SIQAD、Kodak、DIV2K 的版权归原作者所有，本仓库只提供重建脚本与校验值；论文中使用的相应图像仅用于学术说明。
