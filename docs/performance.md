# CPU 光栅化器性能分析

## 执行摘要

本报告对当前采用 screen-tiled multithreaded 的渲染器，与 single-worker
基线版本进行了对比测量。两组 benchmark 都使用相同的 32x32 tile 布局，唯一变量
只有活跃 tile worker 数量。

在上述固定测量负载下，32 worker 将主屏幕渲染 main pass 的平均耗时从 96.5760 ms 降至
12.0115 ms，平均提升约 8.04x。端到端的 `Renderer::render` 外层总耗时平均值从
97.8303 ms 降至 13.2458 ms，平均提升约 7.39x。shadow pass 基本不变，因为它不在
screen tile worker pipeline 内。

## 当前 Tile 多线程设计

渲染器会在主屏幕 pass 开始前，先把 framebuffer 按 screen space 切分为多个 tile。
`RenderSettings::tileSize` 默认值为 32，因此在 960x540 framebuffer 下会得到 30 列、
17 行，共 510 个 tile。

worker 实现基于通用 `sr::ThreadPool`：

- tile 以 `ScreenTile` 矩形的形式存放在 `RenderContext::tiles` 中。
- `ThreadPool::ensureWorkerCount` 会按需扩展常驻 worker 集合，其数量上限同时受
  tile 总数和 `std::thread::hardware_concurrency()` 限制。
- `ThreadPool::parallelFor` 在每次 pass 中复用这些线程，并通过共享的 atomic item
  cursor 让 worker 动态领取下一个 tile。
- 同一 tile index 只会被领取一次；当 tile 数量不足或没有后台 worker 时，会自动退化为
  串行执行。

只有主屏幕相关的 pass 会使用这些 tile worker：

- Depth prepass：将已准备好的三角形光栅化到 framebuffer depth buffer。
- Color pass：执行 depth testing、插值、material sampling、lighting、shadow lookup、
  debug view 选择以及最终像素写入。

以下工作仍然是串行的，或不在 tile worker 的加速范围内：

- Shadow-map rasterization。
- Geometry preparation、clipping、screen-space triangle setup，以及 tile list 创建。
- HUD 绘制与窗口 present。

这意味着当前优化主要命中占主导地位的逐像素 screen work，但无法带来整帧意义上的
线性加速。

## Benchmark 方法

性能测量使用独立的测量入口，对相同渲染路径分别采样 1 worker 与
32 workers 的结果。测量过程直接调用 `Renderer::render`，以避免 HUD 平滑统计对原始
耗时数据的干扰。

测量条件如下：

- Build：Release。
- Framebuffer：960x540。
- Render mode：Final。
- Scene：默认 `TestScene` 与默认 `RenderSettings`。
- Tile size：两组测试均为 32x32。
- Workers compared：1 worker vs 32 workers。
- Warmup：每组先跑 20 帧。
- Samples：每组采样 120 帧。
- Timer source：使用原始 `RendererStats`，并在 `Renderer::render` 外层再加一层
  `std::chrono::steady_clock` 测量。
- HUD 的读数未作为数据源，因为 HUD 通过 `PerformanceMonitor` 展示的是平滑值
  (smoothed values)。

运行环境记录如下：

- Logical processors：32。
- Processor identifier：`AMD64 Family 25 Model 97 Stepping 2, AuthenticAMD`。

## 结果

| 指标 | 1 worker 平均值 | 32 workers 平均值 | 平均提升 | 1 worker 中位数 | 32 workers 中位数 | 中位数提升 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Main pass | 96.5760 ms | 12.0115 ms | 8.04x | 91.1996 ms | 11.9497 ms | 7.63x |
| Shadow pass | 0.8707 ms | 0.8809 ms | 0.99x | 0.8441 ms | 0.8574 ms | 0.98x |
| 外层 `Renderer::render` 总耗时 | 97.8303 ms | 13.2458 ms | 7.39x | 92.4154 ms | 13.1730 ms | 7.02x |

| 指标 | 1 worker 最小值 | 1 worker 最大值 | 32 workers 最小值 | 32 workers 最大值 |
| --- | ---: | ---: | ---: | ---: |
| Main pass | 84.1019 ms | 133.7486 ms | 10.3567 ms | 14.2478 ms |
| Shadow pass | 0.7572 ms | 1.1766 ms | 0.7688 ms | 1.2414 ms |
| 外层 `Renderer::render` 总耗时 | 85.5954 ms | 135.1652 ms | 11.4776 ms | 15.4905 ms |

## 分析

main pass 获得了非常明显的收益，因为它的大部分成本来自可按独立 tile 拆分的逐像素
工作。32 worker 运行时将 main pass 平均耗时减少了 84.5645 ms，最终只剩下单 worker
main pass 成本的大约 12.4%。

整次 render 调用的提升略低于 main pass，因为它仍然包含串行部分。外层
`Renderer::render` 的实测平均加速比为 7.39x，而单看 main pass 的平均加速比为 8.04x。

shadow pass 基本保持不变：1 worker 时为 0.8707 ms，32 workers 时为 0.8809 ms。
这与实现机制一致：shadow-map 绘制走的是独立的串行 rasterization 路径，并不会调用
screen tile workers。

因此可以明确地说，在当前测量条件下，32 worker 相比 1 worker：

- main pass 平均提升约 8.04x；
- 外层 `Renderer::render` 平均提升约 7.39x；
- shadow pass 基本不变。

扩展性是有价值的，但并非线性。可能的限制因素包括：

- tile 分发前仍存在串行 setup 工作。
- 动态领取显著缓解了负载不平衡，但 framebuffer 写入与几何覆盖分布仍会带来尾延迟。
- framebuffer depth/color 写入带来的 memory bandwidth 与 cache pressure。
- worker 唤醒与收尾同步引入的每 pass 开销。

## 优化建议

1. 继续将 32x32 的 tiled worker 路径作为默认方案。它在当前渲染器中带来了最大的实测
   收益：main pass 为 8.04x，外层 render call 为 7.39x。
2. 增加一个常驻 benchmark，或提供仅开发者使用的 worker-count override。当前代码没有
   用于 single-worker 对比的运行时开关，这会让后续性能回归更难量化。
3. 如果后续场景复杂度继续上升，可继续评估更细粒度的任务切分或 work stealing。当前
   持久线程池配合 atomic tile cursor 已能降低不均匀几何分布下的尾延迟，但仍有继续优化
   空间。
4. 在并行化 shadow-map rasterization 之前，先单独 profile 它。该场景下 shadow 成本很小，
   除非未来引入更重的 shadow workload，否则它未必是下一步最优的优化方向。
5. 结合 `mainPassMilliseconds`、外层 render elapsed time 与 scene settings 一起追踪。
   单一 FPS 数字可能掩盖问题究竟来自 screen tiles、shadow rendering、geometry setup，
   还是 presentation。

## 验证

已使用 Release 测试套件完成验证，全部测试通过（1/1）。
