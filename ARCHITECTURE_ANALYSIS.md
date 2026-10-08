# PVideoPlayer 架构分析报告

> 分析范围：仓库当前可见源码、qmake 工程配置、资源文件及已生成的构建目录。  
> 分析视角：面向后续重构、功能拓展和长期维护的软件架构评估。  
> 结论等级：源码事实优先；无法由仓库确认的内容均以“假设/待补充”标注。

## 1. 执行摘要

PVideoPlayer 当前是一个 **Qt Widgets 桌面播放器外壳 + FFmpeg 解码管线 + SDL 音视频输出** 的单进程应用。它不是 QtMultimedia 播放器，也不是插件化架构；更准确地说，是以 `MainWindow` 为应用编排中心、以全局单例 `VideoCtrl` 为播放核心的 **混合式单体架构**。

项目已经具备播放器的核心闭环：

- 本地媒体文件和部分网络 URL 加入播放列表；
- FFmpeg demux/decode、音视频同步、seek、暂停、倍速、逐帧和音量控制；
- SDL 音频输出和嵌入 Qt 控件的视频渲染；
- 播放模式、拖拽、全屏、断点续播、播放历史、音频提取；
- QWidget `.ui` 与 QSS 资源化界面。

主要架构风险集中在以下几点：

1. `VideoCtrl` 同时承担媒体引擎、线程生命周期、时钟/队列、SDL 渲染、音频输出、业务命令和 UI 状态通知，成为高风险“上帝对象”。
2. `MainWindow`、`Playlist`、`Show`、`CtrlBar` 之间通过大量信号槽直接互连，播放流程和 UI 业务规则分散，状态一致性依赖连接顺序和连接类型。
3. FFmpeg/SDL 的 C 结构体、裸指针、手工释放和跨线程共享状态直接暴露在头文件和核心对象中，异常路径、停止/切换文件和设备失效场景的验证成本较高。
4. qmake 工程直接绑定仓库内的 FFmpeg 4.2.1/SDL2 二进制，当前交付和跨平台能力明显偏向 Windows；依赖版本、ABI、运行库和 DLL 分发尚未形成可治理的依赖层。
5. 配置、播放列表、播放历史写入临时目录，且配置服务是全局函数集合，数据模型和持久化策略难以演进。

建议不要一次性重写。优先建立 `PlayerSession/PlaybackService` 业务边界、引入 RAII 和线程命令模型，再逐步把 FFmpeg/SDL 实现移入基础设施层。插件化应在接口稳定后实施，而不是当前阶段直接拆 DLL。

## 2. 项目定位与现状梳理

### 2.1 可验证的技术事实

| 项目 | 当前实现 |
|---|---|
| 构建系统 | qmake，`PVideoPlayer.pro` |
| C++ 标准 | C++17 |
| Qt UI | Qt Widgets，Qt Designer `.ui`，QSS |
| Qt 版本证据 | 构建目录名为 `Qt_6_7_2_MinGW_64_bit-Debug` |
| 媒体底层 | FFmpeg 4.2.1 头文件/库 |
| 音视频输出 | SDL2；视频通过 `SDL_CreateWindowFrom(WId)` 嵌入 Qt 控件 |
| 播放线程 | `std::thread`；读取、音频解码、视频解码、播放循环各有线程职责 |
| UI 通信 | Qt signals/slots，部分显式 `DirectConnection`、`QueuedConnection` |
| 持久化 | `QSettings` INI |
| 全局状态 | `VideoCtrl::GetInstance()` 单例，另有全局 FFmpeg/SDL 状态和互斥量 |

依据文件：[`PVideoPlayer.pro`](./PVideoPlayer.pro)、[`main.cpp`](./main.cpp)、[`videoctrl.h`](./videoctrl.h)、[`datactrl.h`](./datactrl.h)。

### 2.2 已实现功能模块

| 模块 | 现有能力 | 主要代码 |
|---|---|---|
| 应用启动/窗口 | QApplication、无边框窗口、拖动、最大化/全屏、菜单 | [`main.cpp`](./main.cpp)、[`mainwindow.cpp`](./mainwindow.cpp) |
| 播放列表 | 添加/移除/清空、双击播放、拖拽、列表循环/顺序/单曲/随机 | [`playlist.cpp`](./playlist.cpp)、[`medialist.cpp`](./medialist.cpp) |
| 播放控制 | 播放/暂停/停止、前后切换、seek、音量、倍速、逐帧 | [`ctrlbar.cpp`](./ctrlbar.cpp)、[`videoctrl.cpp`](./videoctrl.cpp) |
| 媒体引擎 | demux、音视频解码、队列、时钟同步、seek、流切换基础 | [`videoctrl.cpp`](./videoctrl.cpp)、[`datactrl.h`](./datactrl.h) |
| 视频显示 | SDL renderer/texture、等比缩放、嵌入 QWidget、最后一帧保留 | [`show.cpp`](./show.cpp)、[`videoctrl.cpp`](./videoctrl.cpp) |
| 音频输出 | SDL audio callback、重采样、音量和倍速相关处理 | [`videoctrl.cpp`](./videoctrl.cpp)、[`sonic.cpp`](./sonic.cpp) |
| 用户体验 | QSS、FontAwesome、Toast、快捷键、拖放 | [`guiutils.cpp`](./guiutils.cpp)、[`res/qss/`](./res/qss/) |
| 数据持久化 | 音量、播放列表、播放位置、播放历史 | [`configutils.cpp`](./configutils.cpp) |
| 媒体处理 | 从视频中提取音频 | [`videoctrl.cpp`](./videoctrl.cpp) |

### 2.3 架构分类

当前可归类为：

- **部署形态：** 单进程、单体桌面应用；
- **UI 形态：** QWidget 组件组合，不是 QML/Qt Quick；
- **播放引擎：** 自研 FFmpeg + SDL 管线，不是 QtMultimedia 封装；
- **逻辑组织：** 有明显的 UI/播放底层分区，但没有稳定的领域服务层和端口/适配器层；
- **扩展机制：** 无运行时插件发现、版本协商或插件 SDK；
- **通信风格：** Qt 观察者式信号槽 + 直接调用的混合方式；
- **线程模型：** 播放核心内部多线程，控制命令和共享状态未统一为串行执行模型。

因此，“分层”目前只是文件和类的自然分组，并非严格依赖倒置的分层架构。

## 3. 模块与依赖分析

### 3.1 逻辑模块图

```text
QApplication
    |
MainWindow --------------- ConfigUtils/QSettings
  |     |       |
  |     |       +-- Title
  |     +---------- CtrlBar ---- signals ----+
  +---------------- Playlist ---------------|--> VideoCtrl singleton
                         |                  |
                         +-- MediaList      +--> FFmpeg / SDL / Sonic
                                            |
Show <---- signals/status ------------------+
  |
  +-- Qt label WId <---- SDL_CreateWindowFrom
```

主播放路径大致为：

1. `Playlist` 产生 `SigPlay(filePath)`；
2. `MainWindow` 将其转给 `Show::SigPlay`；
3. `Show::OnPlay` 取得 `label->winId()`，直接调用 `VideoCtrl::start_play`；
4. `VideoCtrl` 创建 `VideoState`，启动读取/解码/播放线程；
5. `VideoCtrl` 通过信号通知 `CtrlBar`、`Show`、`MainWindow` 和 `Playlist`；
6. 播放结束信号再次驱动播放列表切换或保存播放位置。

### 3.2 职责边界

| 类/模块 | 当前职责 | 评价 |
|---|---|---|
| `MainWindow` | UI 装配、窗口行为、菜单、快捷键、播放历史/续播、跨组件连接 | 编排职责过多，已接近应用服务和控制器的混合体 |
| `Playlist` | 列表 UI、媒体条目校验、持久化、播放模式和“下一项”业务 | UI 模型、输入校验、播放队列领域逻辑未分离 |
| `MediaList` | QListWidget、右键菜单、文件选择 | 文件选择和 UI 控件职责耦合 |
| `Show` | 视频容器、拖放、键盘/鼠标事件、Toast、尺寸同步、启动播放 | 既是视图又是播放入口和部分控制器 |
| `CtrlBar` | 控件初始化、音量状态、时间显示、播放模式菜单、用户命令 | 视图持有业务状态并直接知道 `ConfigUtils` |
| `VideoCtrl` | FFmpeg、SDL、队列、时钟、线程、渲染、音频、命令、状态广播、音频提取 | 严重超载，应拆为会话/解码/音频/视频输出/命令层 |
| `datactrl.h` | FFmpeg/SDL 类型、播放状态结构、队列操作实现 | 底层实现细节通过公共头文件泄漏，编译耦合高 |
| `ConfigUtils` | 所有配置、列表、播放位置、历史读写 | 无抽象存储接口，路径和 schema 固化 |

### 3.3 依赖方向与耦合问题

理想方向应为：

```text
UI -> Application/Playback service -> Media engine interfaces -> FFmpeg/SDL adapters
                                      |
                                      +-> Repository interfaces -> QSettings/file/database
```

当前实际更接近：

```text
UI <-> MainWindow <-> VideoCtrl singleton <-> FFmpeg/SDL
 |        |                 ^
 +--------+-----------------+
```

主要问题：

- `Show` 直接依赖 `VideoCtrl`，视图不再是纯展示层；
- `MainWindow` 多次调用 `VideoCtrl::GetInstance()`，导致全局服务定位替代了依赖注入；
- `CtrlBar` 和 `Playlist` 直接使用 `ConfigUtils`，持久化策略侵入 UI；
- `VideoCtrl` 头文件直接包含 `datactrl.h` 和 `sonic.h`，导致任何使用播放控制的类都间接暴露 FFmpeg/SDL 类型；
- `MainWindow` 是 `Playlist`、`Title`、`Show`、`CtrlBar` 和 `VideoCtrl` 的连接枢纽，任何新功能都容易继续堆入该类；
- 播放结束行为由多个槽共同触发：更新控制栏、下一项、保存位置，顺序和状态边界需要靠连接语义维护；
- `DirectConnection` 将来自播放线程的信号直接执行到 UI 对象，存在跨线程 UI 访问风险。代码注释表达了意图，但 UI 槽函数必须始终运行在 GUI 线程。

未发现清晰的类级循环 `#include`，但存在明显的**逻辑循环依赖**：UI 触发核心、核心回调 UI、核心结束又驱动列表，且单例绕过了依赖边界。

## 4. 技术栈与关键实现评价

### 4.1 QWidget 与 `.ui`

**合理性：**

- 适合传统桌面播放器、复杂菜单、原生文件对话框和成熟的 Designer 工作流；
- QWidget 与 SDL `WId` 嵌入方式能够快速实现视频显示；
- QSS 和资源系统便于统一皮肤。

**局限：**

- UI 状态大量通过命令式槽函数修改，状态可观察性和可组合性弱；
- 无法自然复用到移动端、嵌入式或 WebView 场景；
- SDL 嵌入原生窗口把渲染后端和平台窗口句柄绑定到 UI；
- 自绘、全屏、DPI、窗口重建、渲染器失效需要大量平台特判。

短期无需迁移 QML。若目标仍是 Windows/Linux/macOS 桌面，先治理业务与渲染边界比改 UI 技术收益更高；只有在需要多终端、自适应 UI 或 GPU shader 管线时，才评估 Qt Quick。

### 4.2 FFmpeg + SDL，而不是 QtMultimedia

FFmpeg + SDL 适合需要：

- 自定义解码、队列、时钟和音视频同步；
- 精确 seek、逐帧、倍速和滤镜；
- 对编码格式、网络协议、硬件解码有控制权。

代价是：

- 需要自行承担线程、内存、时钟、设备、错误恢复和 ABI 维护；
- FFmpeg API 版本升级成本高；
- SDL 音频回调和渲染器生命周期需要严格的线程/设备规则；
- 当前代码混用了 FFmpeg C API、SDL 同步原语、`std::thread` 和 Qt 信号槽，模型复杂度较高。

`QtMultimedia` 可降低基础播放成本，但对高级滤镜、精确帧控制、跨版本行为一致性和自定义管线的控制较弱。更稳妥的路线是保留 FFmpeg 核心，同时把它包装在项目内部的 `IMediaBackend` 接口后；将来才可以选择 FFmpeg、QtMultimedia 或其他后端。

### 4.3 信号槽与线程模型

当前采用：

- GUI 线程创建和操作 Qt 控件；
- `VideoCtrl` 内部使用 `std::thread` 启动读取、音频解码、视频解码和播放循环；
- FFmpeg/SDL 队列使用 `SDL_mutex`/`SDL_cond`；
- 核心状态通过 Qt signals 广播；
- 部分信号指定 `DirectConnection`，部分指定 `QueuedConnection`。

问题在于：线程归属不是由类型或接口强制表达的，`VideoState` 中的字段被多个线程直接读取/写入，停止与切换依赖标志位和 `join()`。建议后续将所有播放命令串行化到一个 `PlaybackSession` 所属线程，解码线程只产生帧/事件，UI 只接收不可变状态快照。

## 5. 架构问题诊断

### 5.1 可维护性

| 风险 | 证据/表现 | 影响 |
|---|---|---|
| 上帝对象 | `VideoCtrl` 同时包含 1000+ 行级别的播放、渲染、音频和控制逻辑 | 修改任一播放功能都可能影响线程和资源生命周期 |
| UI 编排过重 | `MainWindow` 包含菜单、历史、续播、快捷键、窗口行为和大量连接 | 功能增长后难以定位状态来源 |
| 底层类型泄漏 | `videoctrl.h` 包含 `datactrl.h`，公共 API 使用 `WId`、`VideoState`、FFmpeg/SDL 类型 | 编译慢、替换后端困难 |
| 全局状态 | 单例、静态全局参数、全局互斥量、全局日志文件 | 多窗口、多实例、测试隔离和重入困难 |
| 数据模型缺失 | 播放列表以 `QListWidgetItem` 为数据载体 | 排序、持久化、元数据、队列测试成本高 |

### 5.2 可扩展性

字幕、滤镜、硬件解码、音轨/字幕轨选择、截图、网络重连等功能如果继续直接加入 `VideoCtrl`，会扩大核心类和共享状态。当前没有：

- `MediaItem`、`TrackInfo`、`PlaybackState` 等稳定领域模型；
- 播放后端能力查询（capability）；
- 音频/视频/字幕输出端口；
- 滤镜链或帧变换接口；
- 错误码/错误事件模型；
- 插件生命周期和版本协议。

### 5.3 可测试性

目前核心 API 直接依赖真实 FFmpeg、SDL、窗口句柄和全局单例，导致：

- 播放列表、播放模式、续播计算可以测，但尚未被隔离为纯逻辑；
- 播放停止竞态、失败打开、设备丢失、seek 边界难以做确定性单元测试；
- GUI 测试需要真实窗口和音视频资源；
- `ConfigUtils` 直接使用文件系统，测试会污染临时配置。

### 5.4 性能与稳定性

- 音视频队列和帧队列的大小、丢帧、缓存策略是宏和 `VideoState` 字段，缺少可观测指标；
- UI 频繁接收播放秒数信号，若刷新周期过高可能造成事件队列压力；
- `DirectConnection` 使播放线程可能直接触碰 UI；
- SDL renderer/window/texture 的销毁、重建和最后一帧保留逻辑复杂，设备重置时风险较高；
- 资源释放依赖手工 `stream_close`、线程 `join` 和多个标志位，异常路径需要重点压测；
- 日志文件固定为工作目录下的 `PVideo-player.log`，并且日志回调使用全局 `FILE*`，不适合多实例和正式日志轮转。

### 5.5 跨平台与交付

- qmake 配置明确包含 Windows Kits、Windows DLL 复制和 x86/x64 分支；
- Unix 分支只链接系统库，未提供依赖发现、版本约束和资源/运行库打包策略；
- `SDL_CreateWindowFrom(WId)`、`Ole32`/COM 初始化、DLL 命名和 FFmpeg ABI 都带来平台差异；
- 依赖目录同时出现不同命名/版本风格的 FFmpeg 和 SDL 二进制，需建立唯一依赖来源与可复现构建。

## 6. 重构建议

### 6.1 目标架构

```text
Presentation (QWidget/QML)
    |
Application (PlaybackService, PlaylistService, SettingsService)
    |
Domain (MediaItem, Track, PlaybackState, PlaybackCommand, Error)
    |
Ports (IMediaBackend, IAudioSink, IVideoSink, ISubtitleRenderer, ISettingsRepository)
    |
Adapters (FFmpegBackend, SDLAudioSink, SDLVideoSink, QSettingsRepository)
```

原则：

- UI 不直接包含 FFmpeg/SDL 头文件；
- UI 只发送命令、接收状态快照和错误事件；
- 播放后端通过接口返回媒体信息、轨道、状态和帧；
- 资源所有权由 RAII 类型表达；
- 线程边界由服务/会话对象负责，UI 不直接操作 `VideoState`；
- 持久化通过 repository 接口隔离。

### 6.2 分阶段改造

| 优先级 | 改造项 | 具体动作 | 成本 | 收益 |
|---|---|---|---|---|
| P0 | 线程安全与生命周期 | 禁止播放线程 `DirectConnection` 更新 UI；统一 UI 信号为 queued；增加停止超时/失败事件；梳理所有线程 join 和资源释放 | 中 | 降低崩溃、死锁、退出卡顿风险 |
| P0 | 播放核心切面 | 将 `VideoCtrl` 拆为 `PlaybackSession`、`DemuxDecodePipeline`、`ClockController`、`AudioOutput`、`VideoOutput` | 高 | 解决最大维护瓶颈 |
| P0 | 依赖收口 | 新增 `media/` 私有头文件；公共层只暴露 Qt/C++ 值类型和前置声明 | 中 | 降低编译耦合，支持后端替换 |
| P1 | 领域模型 | 引入 `MediaItem`、`PlaylistModel`、`PlaybackState`、`PlaybackCommand`、`PlaybackError` | 中 | 播放列表、状态和测试可独立演进 |
| P1 | 应用服务 | `PlaybackService` 负责命令、状态和事件；`MainWindow` 只做依赖注入和 UI 绑定 | 中 | 消除 MainWindow 连接网和单例依赖 |
| P1 | 配置抽象 | `ISettingsRepository` + `QSettingsRepository`；路径迁移到 `QStandardPaths::AppConfigLocation` | 低-中 | 可测试、可升级 schema、跨平台一致 |
| P1 | RAII 封装 | 为 `AVFormatContext`、`AVCodecContext`、`AVPacket`、`AVFrame`、SDL mutex/cond/texture/renderer 建立 deleter | 中 | 降低泄漏和异常路径风险 |
| P2 | 渲染端口 | `IVideoSink` 与 `IAudioSink`；SDL 只作为 adapter | 中-高 | 支持 Qt/SDL/OpenGL/Vulkan 或远程输出 |
| P2 | 后端能力 | `IMediaBackend::capabilities()`、编解码器/设备/协议能力报告 | 中 | 为硬件加速、网络流、插件做准备 |
| P2 | 插件边界 | 先定义插件 ABI/接口和版本，再做字幕、滤镜、输入源插件 | 高 | 避免过早拆分造成 ABI 灾难 |

推荐采用“绞杀者”迁移：保留现有 `VideoCtrl` 作为旧 adapter，先让新 `PlaybackService` 通过兼容接口调用它；每完成一个子模块再替换实现，不做一次性重写。

### 6.3 优先修正的具体问题

1. `VideoCtrl::GetInstance()` 改为由 `main` 创建并注入 `MainWindow`，至少先取消 UI 层的静态查找。
2. `SigStartPlay` 不应使用 `DirectConnection` 调用 UI 槽；跨线程状态统一通过 queued 信号和不可变数据传递。
3. `start_play`、`OnPause`、`OnStop`、`OnPlaySeek` 等命令应进入一个串行命令入口，避免多个线程同时修改 `m_cur_stream`、`m_play_loop` 和 `m_user_stop`。
4. 文件/URL 解析和校验统一为 `MediaLocator`/`MediaItem`，删除 `Playlist` 与 `OnAddFileAndPlay` 中的重复校验逻辑。
5. 用 `QAbstractListModel` 替代 `QListWidgetItem` 作为播放列表事实数据源，视图仅负责展示。
6. 对所有打开失败、解码失败、音频设备失败、渲染器创建失败提供结构化错误事件，避免只写日志后继续走成功路径。
7. `ConfigUtils` 的配置位置、版本和迁移策略显式化，避免使用临时目录存放长期用户数据。

## 7. 功能拓展建议与架构要求

| 功能 | 平滑程度 | 必要架构能力 |
|---|---:|---|
| 字幕加载/切换/样式 | 高 | 独立 `SubtitleService`、字幕轨模型、时间轴渲染端口；不要把字幕队列继续堆进 `VideoCtrl` |
| 音轨/字幕轨选择 | 高 | `TrackInfo`、轨道切换命令、后端能力查询 |
| 网络流/RTSP/HTTP 重连 | 中 | `InputSource`、超时/取消、缓冲状态、重连策略和网络错误事件 |
| 截图/录制 | 中 | 视频帧输出接口、像素格式转换、异步任务服务 |
| 滤镜/裁剪/旋转 | 中 | FFmpeg filter graph 或独立 `IVideoFilter` 链、帧生命周期和 GPU/CPU 能力标识 |
| 硬件解码 | 中-高 | `HwAccelContext`、设备选择、格式协商、软件回退、显存/系统内存帧边界 |
| 播放历史/收藏/标签 | 高 | repository、稳定的媒体 ID/URL 规范化、schema 版本和迁移 |
| 播放器插件生态 | 低（当前） | 稳定 C ABI 或 Qt 接口、版本协商、沙箱/权限、插件发现、错误隔离 |
| 多窗口/画中画 | 中 | 多个独立 `PlaybackSession`，禁止单例持有唯一 `m_cur_stream` |
| 多端 UI/QML | 中-高 | UI 与应用服务完全解耦、可序列化状态和命令模型 |

推荐顺序：字幕/轨道模型 → 网络健壮性 → 截图/滤镜 → 硬件加速 → 插件生态。插件应最后做，因为它要求最稳定的接口和生命周期。

## 8. 验证与质量门禁

重构过程中建议建立以下自动化门禁：

- **单元测试：** 播放模式、队列索引、URL/路径规范化、seek 百分比与时间换算、续播阈值、配置 schema 迁移；
- **组件测试：** fake media backend 驱动 `PlaybackService`，验证播放/暂停/停止/切换/失败状态；
- **集成测试：** 固定短视频、无音频视频、音频-only、损坏文件、网络超时、循环播放、快速切换；
- **线程测试：** 连续 start/stop/seek、窗口关闭时停止、播放结束自动下一项、渲染器创建失败；
- **性能指标：** 首帧时间、seek 响应时间、A/V drift、队列水位、丢帧数、CPU/内存、线程退出耗时；
- **平台矩阵：** Windows x64 至少覆盖 Debug/Release；若支持 Linux/macOS，必须覆盖依赖发现、窗口嵌入和音频设备；
- **静态检查：** clang-tidy、ASan/UBSan（可行平台）、线程竞态检测和 FFmpeg/SDL 资源审计。

## 9. 需要补充的信息与当前假设

### 需要补充

1. 目标平台：仅 Windows，还是需要 Linux/macOS？
2. 目标功能优先级：字幕、网络流、滤镜、硬件加速、多窗口、插件中哪些是近期目标？
3. 当前线上/实际使用问题：崩溃日志、卡顿、音画不同步、内存增长、设备兼容性。
4. 交付方式：是否需要安装包、便携版、自动更新、第三方许可证清单？
5. 团队规模和可接受重构周期：个人项目、短期迭代还是长期产品化？
6. 是否必须保持当前 FFmpeg/SDL 管线，或可接受 QtMultimedia/其他后端作为备选？

### 当前假设

- 假设播放器仍以桌面 Windows 为首要交付平台；
- 假设 FFmpeg/SDL 的自定义控制能力是保留要求；
- 假设现有构建目录不是源码依赖，只作为 Qt 版本和已构建组件的证据；
- 假设当前没有完整的自动化测试、CI、崩溃收集和性能基线，因为仓库中未发现相应配置/文档；
- 假设“网络流支持”目前主要是 URL 进入 FFmpeg，而不是已经完成重连、缓冲和网络状态管理。

## 10. 演进路线图与关键决策点

### 短期：稳定化与边界治理（1～2 个迭代）

- 修复跨线程 UI 访问风险，统一停止和错误状态；
- 为 `VideoCtrl` 增加结构化错误、状态快照和可观测日志；
- 引入 RAII 封装和 fake backend 测试；
- 抽取 `MediaItem`、`PlaylistModel`、`PlaybackState`；
- 将配置路径迁移到 Qt 标准应用配置目录并加入 schema 版本；
- 固化 Windows x64 Release 构建和 FFmpeg/SDL 依赖清单。

**决策点：** 是否承诺继续使用 FFmpeg + SDL；若是，立即定义内部 media port，避免后续继续扩大 C API 泄漏。

### 中期：服务化与能力扩展（2～4 个迭代）

- 引入 `PlaybackService`/`PlaybackSession`，消除单例；
- 将解码、音频输出、视频输出、字幕处理拆为可替换组件；
- 播放列表改为模型/服务，UI 改为绑定状态；
- 完成字幕、轨道选择、网络超时/重连和截图；
- 建立组件测试、固定媒体测试集和性能基线；
- 引入 CMake 作为候选构建系统，或至少把 qmake 依赖配置模块化。

**决策点：** 是否需要多窗口/多会话；若需要，必须禁止 `VideoCtrl` 的单一 `m_cur_stream` 设计继续扩张。

### 长期：平台与生态演进（4 个迭代以上）

- 硬件解码和 GPU/滤镜管线；
- 多渲染后端或 Qt Quick 前端；
- 插件 SDK、插件发现/版本协商和独立故障隔离；
- 跨平台打包、自动更新、许可证与 SBOM；
- 以稳定领域接口替换具体 FFmpeg/SDL 类型，支持后端替换。

**决策点：** 插件 ABI 是否需要跨编译器/跨版本稳定；若需要，应优先设计窄 C ABI 或 out-of-process 插件协议，而不是直接暴露 Qt/FFmpeg 类型。

## 11. 结论

当前项目已经具备一个功能完整度较高的传统桌面播放器原型，底层 FFmpeg/SDL 管线也提供了进一步实现高级播放能力的基础。短板不在“缺少一个新的 UI 框架”，而在于播放核心、线程、渲染和应用业务集中于少数全局对象，且缺乏稳定的领域模型和接口边界。

最优策略是：**先稳定线程和生命周期，再抽取领域/应用服务边界，随后隔离 FFmpeg/SDL，最后再做硬件加速和插件化。** 这样可以在保持当前可播放能力的同时，以增量方式降低技术债，避免一次性重写带来的功能回退和不可控迁移成本。

## 12. 已落地的第一阶段改造

截至当前版本，以下改造已经完成并通过 Qt 6.7.2 MinGW Release 构建：

- 新增 `PlaybackService`，作为 UI 与 `VideoCtrl` 之间的应用服务边界；
- 新增 `MediaItem`、`PlaybackState` 和 `PlaybackStatus` 值对象；
- 播放错误通过结构化信号向 UI 传播；
- 移除 UI 播放状态连接中的显式 `DirectConnection`；
- 新增 `PlaylistModel`，并将播放列表事实数据从 `QListWidgetItem` 迁移到模型；
- 新增 `ISettingsRepository`/`QSettingsRepository`；
- 配置文件从临时目录迁移至 Qt 标准应用配置目录；
- 播放列表添加、删除、清空、持久化和播放切换统一基于模型。

尚未完成的后续高风险改造仍包括 `VideoCtrl` 内部的解码/时钟/音频/视频输出拆分、FFmpeg/SDL 全量 RAII 化、命令串行化、多会话支持和自动化测试。后续应继续采用兼容适配器和小步构建验证，不建议直接替换整个播放核心。

## 13. 播放核心迁移边界（当前阶段）

为降低高风险核心重构的迁移风险，当前已先完成以下边界建设：

- `PlaybackEventSource` 统一播放事件信号，真实 `VideoCtrl` 与 fake backend 共用同一事件协议；
- `IPlaybackBackend` 统一播放控制命令，应用层不再依赖 FFmpeg/SDL 类型；
- `PlaybackService::enqueue()` 将跨线程播放命令投递到服务对象所属线程，保证命令在单一执行上下文中串行执行；
- `PlaybackService` 增加销毁期命令闸门，避免服务销毁后继续接受异步命令；
- 新增 `media_raii.h`，为 FFmpeg 格式上下文、编解码上下文、帧/包以及 SDL 窗口、渲染器、纹理提供 `std::unique_ptr` deleter 类型；
- 新增 fake backend 和 QtTest，覆盖命令转发、状态变化、空地址错误以及跨线程命令排队；
- 主工程和独立测试工程均已使用 Qt 6.7.2 MinGW 工具链验证通过。

这些改造目前属于“兼容外壳”和资源管理基础设施，`VideoCtrl` 内部的解封装、解码、时钟、音频输出、视频输出和音频提取仍未完成物理拆分。下一步应按音频提取、时钟、视频输出、音频输出、解码管线的顺序逐步抽取，并在每一步保留 `VideoCtrl` façade 和对应测试。

## 14. 当前增量：时钟控制器与纹理资源所有权

本阶段继续完成了两个低风险、可独立验证的物理迁移：

- 新增 `ClockController`，集中管理时钟初始化、设置、倍速变化和读取；
- `VideoCtrl` 保留兼容方法，但时钟算法已委托给 `ClockController`，后续音视频解码器不需要再依赖 `VideoCtrl` 的时钟实现；
- `m_vid_texture` 已改为 `SdlTexturePtr`，纹理释放、重建和上传路径统一通过 RAII 所有权管理；
- `m_last_frame` 已改为 `AvFramePtr`，结束播放、用户停止和纹理重建路径不再手工释放该帧；
- RAII 基础设施补充 SDL mutex/condition、SwsContext 和 SwrContext deleter；
- 主工程使用 Qt 6.7.2 MinGW Release 构建验证通过。

尚未完成的部分仍需按模块逐步迁移，尤其是 `VideoState` 内部的格式上下文、编解码上下文、帧队列和 SDL 音频设备。由于这些对象跨线程共享，不能仅替换指针类型，必须先明确线程停止顺序和队列所有权，再进行 RAII 化。

## 15. 当前增量：管线组件、音频提取和会话扩展

本阶段新增并接入了以下基础能力：

- `AudioExtractionService`：音频提取实现已从 `VideoCtrl` 移出，保留原有流拷贝、解码、重采样和编码输出路径；
- `DemuxReader`：独立封装 FFmpeg 输入打开、流信息探测、读包、定位和关闭；
- `DecoderComponent`：独立封装编解码器上下文的创建、打开、发送包、接收帧和关闭；
- `HardwareDecoderDevice`：提供 FFmpeg 硬件设备上下文的 RAII 初始化入口，具体硬件像素格式和解码器选择仍需按平台接入；
- `AudioOutputDevice`：统一 SDL 音频设备打开、暂停和关闭，已接入 `VideoCtrl` 音频输出路径；
- `VideoOutputResources`：独立管理视频纹理资源，作为现有视频输出拆分的资源边界；
- `FilterChain`：建立 FFmpeg filter graph 的独立边界；
- `SubtitlePluginRegistry`：建立字幕 provider 插件协议；
- `PlaybackSessionManager`：支持多个独立 `PlaybackService` 会话，并增加独立会话生命周期测试；
- `VideoState` 的格式上下文、解码器上下文和顶层对象生命周期已开始使用 RAII/正确 C++ 构造析构，修复了原先对含 `std::thread` 的对象使用 `av_mallocz` 的未定义行为；
- 解码线程退出时增加 `joinable()` 防护，资源释放顺序为停止读取线程、停止解码器、关闭音频输出、销毁队列和释放格式上下文。

当前仍需继续完成的接入工作：

- 将 `DemuxReader` 和 `DecoderComponent` 直接替换 `VideoCtrl::read_thread`、`audio_thread`、`video_thread` 的旧函数；
- 将 `VideoOutputResources` 完整接管窗口、渲染器、纹理和渲染循环；
- 将 `SwrContext`、`SwsContext`、SDL mutex/condition 逐个从 `VideoState` 裸字段迁移到所有权对象；
- 为硬件解码、滤镜链和字幕插件提供真实播放会话配置及平台实现，而不仅是接口；
- 将 `PlaybackSessionManager` 接入主窗口/播放列表，当前它已经支持多会话对象管理，但默认 UI 仍使用兼容的单实例后端。

## 16. 当前接入进展

本轮进一步完成了以下实际接入：

- `MainWindow` 不再直接构造 `PlaybackService`，而是通过 `PlaybackSessionManager` 创建默认会话；后续可在不改变 UI 命令接口的情况下增加第二个会话；
- `VideoOutputResources` 已在 renderer 创建/复用路径绑定，纹理资源边界与现有渲染循环保持一致；
- `VideoState::continue_read_thread` 增加 `SdlCondPtr` 所有者，线程停止后由 RAII 释放 condition；
- 现有读取线程、音频线程和视频线程保留兼容入口，已经可以逐步将 `DemuxReader`、`DecoderComponent` 迁入，而不需要再次改变 UI 或播放服务接口；
- 主工程和生命周期测试在本轮修改后均构建通过。

仍然不能安全“一步替换”的部分是 packet/frame queue。它们当前使用内嵌 `AVPacket`、SDL mutex/condition 和跨线程等待协议，必须先改造初始化/销毁函数，禁止对含 C++ RAII 成员的对象使用 `memset`，再把队列资源所有权迁移到 RAII 类型。硬件解码、滤镜、字幕也需要在真实帧管线中接入，而不是只创建对象，因此后续应以媒体文件集成测试作为验收标准。

## 17. 当前轮次：读取管线接入

- `VideoCtrl::read_thread()` 已使用 `VideoState::demux_reader` 完成输入打开、中断回调绑定、流信息探测和 packet 读取；
- `DemuxReader` 新增带中断回调的打开接口，读取线程停止时 FFmpeg I/O 可以通过 `abort_request` 中断；
- `VideoState` 继续保留 `ic` 非拥有观察指针，所有权由 `DemuxReader` 管理，避免格式上下文双重释放；
- 主工程和 QtTest 生命周期测试在读取管线迁移后通过。

音视频解码线程仍保留旧的队列协议，但已具备迁移到 `DecoderComponent` 的上下文边界。下一轮应先为 packet/frame queue 增加拥有者和停止协议测试，再迁移 decoder send/receive，最后接入硬件帧和滤镜，避免在后台线程仍持有资源时销毁 FFmpeg 上下文。

## 18. 当前轮次：队列所有权与 DecoderComponent 迁移

- `PacketQueue` 和 `FrameQueue` 分别拥有 SDL mutex/condition 的 RAII owner，裸指针仅作为兼容观察指针保留；
- 移除队列初始化阶段对含非平凡成员对象的整体 `memset`，并补充同步对象创建失败与帧槽位部分初始化的回滚；
- packet 数量和 serial 的读取统一通过加锁辅助函数完成，避免 decoder 线程无锁读取队列状态；
- abort 使用 condition broadcast，以同时唤醒多个等待中的生产者/消费者；
- `decoder_abort()` 保持“abort、唤醒、join、flush/释放”的顺序，并对线程状态做 `joinable()` 防护；
- `Decoder` 已持有 `DecoderComponent`，音视频 send/receive/flush 通过组件边界执行；现有 codec context 仍由兼容路径提供并以观察方式绑定，避免重复释放。

验证结果：使用 Qt 6.7.2 MinGW 11.2 工具链完成 Release 构建，QtTest 回归测试通过。真实媒体播放、seek、网络流和高并发队列场景仍需后续集成测试覆盖。

## 19. 当前轮次：VideoState 剩余资源所有权迁移

本阶段完成 `VideoState` 中最后一批手工管理资源的 RAII 化，覆盖文档第 15、18 节列出的待迁移项：

- `swr_ctx` 迁移为 `SwrContextPtr owned_swr_ctx`；`audio_decode_frame` 的重采样器创建、失败回滚与重初始化全部通过所有权对象完成；
- `img_convert_ctx`、`sub_convert_ctx` 迁移为 `SwsContextPtr owned_img_convert_ctx`、`owned_sub_convert_ctx`；
- `upload_texture()` 形参由 `SwsContext **` 改为 `SwsContextPtr &`，`sws_getCachedContext` 通过 `release()`/`reset()` 保持原有“缓存命中即复用、参数变化即重建”的语义；
- `VideoState::filename` 由 `char *`（`av_strdup`/`av_free`）迁移为 `std::string`，删除手工分配与释放；
- `stream_close()` 不再手工调用 `sws_freeContext`/`av_free`，`stream_component_close()` 不再手工 `swr_free`，资源统一随 `VideoState` 析构释放；
- 顺带消除了原先从未释放的 `sub_convert_ctx` 处理路径风险（改由 RAII 兜底）。

验证结果：主工程 Release 构建通过，QtTest 回归通过。

## 20. 当前轮次：组合根注入与 UI 层静态查找移除

对应文档第 6.3 节第 1 项“取消 UI 层的静态查找”和第 6.1 节“UI 不直接包含 FFmpeg/SDL 头文件”：

- 新增 `CreateDefaultPlaybackBackend()` 工厂，返回 `PlaybackBackendBundle{events, backend}`；声明位于只依赖 Qt 的 `playbackbackend.h`，实现位于后端实现文件；
- `main.cpp` 成为组合根：创建后端并注入 `MainWindow`，不再由窗口内部查找单例；
- `MainWindow` 构造函数改为接收 `PlaybackEventSource *` 与 `IPlaybackBackend *`，内部仅调用 `PlaybackSessionManager::createSession()`；
- `mainwindow.cpp` 移除 `#include "videoctrl.h"`，UI 层翻译单元不再间接包含 FFmpeg/SDL 头文件；
- `CreateDefaultPlaybackBackend()` 在 `videoctrl.cpp` 内实现，避免组合根为了拿后端而包含具体后端头（也规避了 `SDL.h` 对 `main` 的宏重定义）。

验证结果：主工程 Release 构建通过，启动冒烟测试通过。

## 21. 当前轮次：媒体地址校验统一与组件测试

对应文档第 6.3 节第 4 项“文件/URL 解析和校验统一”：

- 新增 `MediaLocator`（`medialocator.h/.cpp`）：统一网络流判定、受支持媒体扩展名判定，以及“空地址/不受支持格式/文件不存在/有效”的地址归一化与校验；
- `Playlist::SlotOnAddFile` 与 `Playlist::OnAddFileAndPlay` 改为调用 `MediaLocator::normalize()`，删除两处重复的扩展名、存在性和绝对路径处理逻辑；
- `GuiUtils::CheckNetworkStream` 移除，`Playlist`/`Title` 统一改用 `MediaLocator::isNetworkStream()`；
- `PlaylistModel` 新增 `indexOf()`，`addItem()` 与 `OnAddFileAndPlay()` 复用同一去重逻辑，删除手写重复查找；
- 测试工程重构为可运行多个测试类（新增 `tests/testmain.cpp`、`tests/tst_medialocator.*`），新增 `MediaLocatorTest` 10 项，覆盖网络流判定、扩展名判定、空地址、优先格式校验、缺失文件、绝对路径归一化。

验证结果：主工程 Release 构建通过；`MediaLocatorTest` 10/10、`PlaybackServiceTest` 6/6 全部通过。

## 22. 当前轮次：输出资源、包生命周期与媒体能力落地

本阶段完成文档第 22 节（上一轮遗留）中三项仍然只有“接口/裸成员”的改造。

### 22.1 `VideoOutputResources` 完整接管窗口、渲染器、纹理与渲染原语

- `VideoOutputResources` 现在持有 `SdlWindowPtr`、`SdlRendererPtr`、`SdlTexturePtr`，并提供 `ensureWindow`、`ensureRenderer`、`recreateRenderer`、`destroyRenderer`、`ensureTexture`、`beginFrame`、`presentTexture`、`endFrame`、`setFullScreen`、`reset`；
- `VideoCtrl` 移除 `m_window`、`m_renderer`、`m_vid_texture` 裸成员与 `realloc_texture()`，渲染路径（`video_open`、`video_display`、`video_image_display`、`do_exit`、空闲重绘、全屏切换）全部通过资源对象完成；
- 渲染器“真实清屏探测失效后重建”、纹理“尺寸/格式变化才重建”、纹理先于渲染器释放等既有语义保持不变，破坏性顺序由 `destroyRenderer()` 内部保证。

### 22.2 `AVPacket` 生命周期内聚

- `MyAVPacketList` 成为拥有型节点：析构统一 `av_packet_unref`，节点改用 `new (std::nothrow)` / `delete` 管理；
- `packet_queue_flush()` 不再手工 `unref` + `av_freep`，直接 `delete` 节点；
- `packet_queue_get()` 使用 `av_packet_move_ref()` 把引用移交给调用方后再 `delete` 节点，避免析构重复释放；生产端仍保留浅拷贝转移，因为 `av_packet_move_ref` 会重置 `flush_pkt` 哨兵而使刷新包判定失效；
- `Decoder` 增加析构统一 `unref` 待处理包，并删除从未在有效路径使用、仅服务于已废弃实现的 `pkt_temp` 字段；
- 顺带移除 `datactrl.h` 中整段 `#if 0` 的旧解码实现（约 92 行死代码，且引用了被删除的 `pkt_temp`）。

### 22.3 硬件解码、滤镜链、字幕插件的真实实现

- **硬件解码**：`HardwareDecoderDevice::initializeForDecoder()` 依据 `avcodec_get_hw_config()` 声明的能力协商设备类型与硬件像素格式，`toSoftwareFrame()` 负责把硬解帧转回系统内存；`stream_component_open()` 优先尝试硬件解码，并新增 `get_format` 协商回调与“打开失败即释放上下文重建为软件解码”的回退路径；视频解码线程统一转换为软件帧，渲染路径无需感知硬解。可通过 `PVP_HW_DECODE` 指定设备名或设为 `off/0/none` 禁用。
- **滤镜链**：`FilterChain` 实现完整的 `buffer → description → buffersink` 视频滤镜图（含 `avfilter_graph_parse_ptr` 端点所有权处理、`push`/`pull`、输出时间基查询）；`video_thread()` 在配置了 `PVP_VIDEO_FILTER` 时惰性构建滤镜图并把滤镜产出的所有帧依次入队，未配置时走与原来完全一致的路径。
- **字幕插件**：新增 `subtitleparser.h/.cpp`，实现 SRT/WebVTT 时间轴与文本解析；`FileSubtitleProvider` 查找媒体同目录同名 `.srt`/`.vtt` 并解析；`SubtitlePluginRegistry` 构造时自动注册内置 provider，并保留外部 provider 注册入口（不接管所有权）。

### 22.4 验证结果

- 主工程 Qt 6.7.2 MinGW Release 构建通过；
- 测试扩展到 3 个测试类共 24 项，全部通过：`MediaLocatorTest` 10、`SubtitlePluginTest` 8、`PlaybackServiceTest` 6；
- 应用启动冒烟测试通过（默认启用硬件解码，以及 `PVP_HW_DECODE=0` 两条路径）。

### 22.5 仍需真实环境验证

- 硬件解码与滤镜链需要真实媒体文件、显卡驱动和多平台设备才能确认输出正确性与性能；
- 字幕仅完成“提供者 + 解析”，**屏幕字幕渲染（含时间轴驱动与样式）尚未实现**，需要在 `Show` 侧增加渲染与时间同步；
- 解码队列竞态、快速切换与设备失效场景仍需压力测试。

## 23. 当前轮次：移除音频可视化遗留的 1 MB 死字段

播放实测内存为 130–180 MB，拆解后确认属正常区间（Qt6 + DLL 基线实测 40.2 MB 工作集，其余为解码线程帧缓冲、纹理/渲染器后备缓冲与包队列）。期间发现 `VideoState` 内音频可视化（RDFT/FFT，相关代码已注释）遗留的字段已无任何引用，属纯浪费：

- 删除 `int16_t sample_array[SAMPLE_ARRAY_SIZE]`（1 MiB）、其索引 `sample_array_index`（4 B）以及随之失效的 `SAMPLE_ARRAY_SIZE` 宏；
- `rdft_bits`、`xpos`、`last_vis_time` 等其余可视化残留（合计约 16 B，同属 `RDFTContext *rdft` / `FFTSample *rdft_data` 注释块）本次保留，未扩大改动范围。

编译期尺寸探针验证结果：`sizeof(VideoState)` 由 **1,055,272 字节降至 6,696 字节**，差值 1,048,576 字节 = 恰好 1 MiB。主工程与测试构建通过，24/24 测试通过。

## 24. 当前轮次：画中画接入（渲染目标切换）

对应文档第 7 节“多窗口/画中画”和第 10 节“决策点：是否需要多窗口/多会话”。真正的多会话要求多个独立 `PlaybackSession`，而 `VideoCtrl` 仍是单例并只持有一个 `m_cur_stream`，去单例化还牵连 `g_show_rect_mutex`、`g_audio_callback_time`、`log_file` 和 SDL 音频回调中的 `GetInstance()`。因此本轮先交付**单会话画中画**：播放会话保持不变，只切换视频渲染所绑定的原生窗口。这一步同时建立了多会话将来需要的渲染目标端口。

### 24.1 后端渲染目标端口

- `IPlaybackBackend` 新增纯虚 `OnSetRenderTarget(WId)`；`PlaybackService::setRenderTarget(WId)` 复用既有 `enqueue()` 在服务所属线程串行投递，UI 层依旧不接触后端实现；
- `tests/fakeplaybackbackend.h` 同步实现该接口并记录调用次数与目标句柄，新增 `PlaybackServiceTest::renderTargetCommandIsForwarded` 覆盖命令转发。

### 24.2 VideoCtrl 的跨线程切换

SDL 窗口由 `SDL_CreateWindowFrom(WId)` 包装 Qt 的原生窗口，渲染器有线程亲和性，只能在创建它的 SDL 线程上操作。实机探针验证（Qt 6.7.2 MinGW + Windows，见 24.5）表明：`QWidget` 重新设定父子关系时，Qt 通过 Win32 `SetParent` 把**同一个** HWND 迁移到新的顶层窗口，`winId()` 不变，SDL 仍能正常渲染并自动跟随新的窗口尺寸。因此切换分为两种情况：

- Qt 主线程调用 `OnSetRenderTarget()` 时只写入 `std::atomic` 的待处理句柄与标志（播放线程未启动时直接记录，供下次 `start_play` 使用），不触碰任何渲染资源；
- SDL 线程在 `refresh_loop_wait_event()` 的刷新循环与空闲事件循环中调用 `applyPendingRenderTarget()`；
- **句柄未变化**（Qt 迁移同一窗口，实测主路径）：只调用 `syncRenderTargetSize()` 读取新窗口尺寸、更新渲染区域并置 `force_refresh`，不重建任何资源，避免切换闪烁；
- **句柄变化**（防御性分支，例如平台或 Qt 版本重建了原生窗口）：释放 `VideoOutputResources` 并令 `m_video_open = false`，由既有 `video_open()` 在新窗口上重建窗口/渲染器/纹理；纹理随窗口释放后，`resetUploadedFlags()` 在 `pictq` 锁内清零 `uploaded` 并置 `force_refresh`，保证队列中的帧重新上传；
- 播放结束（无活动流、只剩最后一帧）时不存在 `video_open()` 路径，`redrawLastFrame()` 用 `m_last_frame` 在新目标上重建纹理并重绘。

### 24.3 UI 层画中画

- `Show::SetPipActive(bool)` 把视频容器 `label` 移入无边框置顶的 `Qt::Tool` 悬浮窗，`winId()` 随之变化后再通知后端切换，使播放不中断；退出时把容器归还 `Show` 并恢复 `ChangeShow()` 的几何计算；
- 画中画模式下 `ChangeShow()` 直接返回，视频尺寸交由悬浮窗布局管理；`Show` 析构时会先把容器归还，避免与悬浮窗重复释放；
- 画中画模式的鼠标语义：按下并拖动移动悬浮窗，未产生拖动则在松开时切换播放/暂停，从而与原有的“单击画面暂停”语义兼容；视频画面右键菜单可进出画中画；
- `MainWindow` 新增“画中画”菜单项与全局快捷键 `Ctrl+P`（`ApplicationShortcut`，焦点在悬浮窗上时仍生效），通过 `SigPipActiveChanged` 同步菜单勾选；进入全屏前自动退出画中画。

### 24.4 验证结果

- 主工程 Qt 6.7.2 MinGW Release 构建通过；
- 测试扩展到 4 个测试类共 25 项，全部通过（`MediaLocatorTest` 10、`SubtitlePluginTest` 8、`PlaybackServiceTest` 7，新增 `renderTargetCommandIsForwarded`）；
- 应用启动冒烟测试通过（持续运行，无早期退出）。

### 24.5 关键行为验证与仍需确认

为确认“视频容器迁移到另一个顶层窗口后 SDL 是否仍可用”，本轮用临时探针程序（不属于仓库，验证后已删除）在真实 Windows 平台实测：

- `label` 迁移到悬浮窗后 `winId()` **不变**（同为 4328492），而其 Win32 父窗口由主窗口 HWND 变为悬浮窗 HWND，迁回后恢复；
- `SDL_CreateWindowFrom` 绑定该 HWND 后，迁移前后 `SDL_RenderClear` 均返回 0，`SDL_GetError()` 为空，说明 SDL 在迁移后的窗口上仍能正常渲染；
- `SDL_GetWindowSize` 由 960x540 变为 447x237，与视频容器的实际尺寸同步变化，说明 SDL 能跟随迁移后的窗口尺寸。

仍需真实播放确认：

- 画中画切换瞬间的画面连续性，以及播放结束后切换时 `redrawLastFrame()` 的表现；
- D3D 交换链在窗口迁移后是否需要额外的 `ResizeBuffers`（探针中 clear/present 无错误，但未验证实际帧内容）；
- 真正的多会话（同时播放两个独立视频）仍要求 `VideoCtrl` 去单例化与全局状态隔离，本轮的 `OnSetRenderTarget` 端口可作为其中一环继续复用。
