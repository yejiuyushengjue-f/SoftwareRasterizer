# CPU Rasterizer 简历项目升级路线

## Summary

本项目定位为图形学核心型本科项目：从零实现 C++20 CPU Rasterizer，展示现代渲染管线的核心机制。当前版本已经具备可运行程序、最终渲染图、法线调试图和多种 Debug Views，后续升级重点应放在可解释、可验证、可量化，而不是单纯堆叠视觉特效。

## 本次目标（已落实）

- 明确项目定位：纯 CPU 软件光栅化渲染器，不依赖 Direct3D、OpenGL 或 Vulkan。
- 明确当前简历表达：视锥裁剪、三角形光栅化、深度测试、透视校正插值、纹理采样、Normal Mapping、多光源着色与 Shadow Mapping。
- 明确展示方式：通过 Final、Albedo、Normal、Depth、UV、Shadow Factor、Light、Light-space Depth 等视图拆解渲染中间结果。
- 明确后续路线：短期、阶段性和长期目标作为后续迭代方向，本次不实现。

## 短期目标（2-3 周，不在本次完成）

- 增加可量化展示层：FPS、帧耗时、三角形数量、Shadow Pass / Main Pass 耗时。
- 增加轻量级调试 HUD 或窗口标题统计，不急于接入 ImGui。
- 更新 README 的验证与性能部分：补充“如何验证正确性”和一张基础性能数据表。
- 产出稳定展示素材：最终渲染、Normal Map、Shadow Debug、性能统计截图。

## 阶段性目标

- 工程可信度：加入 CTest 或轻量单元测试，优先覆盖数学库、OBJ Loader、Framebuffer、ShadowMap。
- 管线可解释性：整理 Shadow Pass、Main Pass 和 Debug View 的代码结构，让渲染流程更容易阅读和讲解。
- 图形学深度：加入 Gamma Correction、Tone Mapping、可调 Shadow Bias、PCF 半径、Normal Strength 等参数。
- 性能方向：探索 Tile-based Rasterization 或多线程渲染，并用短期建立的统计系统对比优化前后帧耗时。

## 长期/最终目标

- 做成小型软件渲染实验平台：能加载模型、切换调试视图、观察中间结果、查看性能指标。
- 最终简历材料包含量化结果，例如指定分辨率和模型复杂度下的优化前后帧耗时变化。
- 最终交付形态包括可运行 exe、清晰 README、截图或动图、技术设计文档、测试或 benchmark 结果。
- 项目边界保持清晰：目标是用 CPU 复现并解释 GPU 渲染管线核心机制，不扩展成完整游戏引擎。
