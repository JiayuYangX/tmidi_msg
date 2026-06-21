# TMIDI Messages 插件

> [English README](README.en.md)

将 TMIDI Player 连接到 SSP 桌面角色的 PLUGIN/2.0 插件，通过 DDE 实时传递播放信息。

当 TMIDI Player 开始播放 MIDI 文件时，插件自动检测并注入 Sakura Script，让角色说出当前曲目信息。

## 前情提要

TMIDI Player 自 **ver.3.8.1**（2001 年）起搭载了 SSTP 客户端功能，可在演奏开始时向伪春菜发送曲名。但该功能使用的是**同步阻塞**模式的 Winsock，一旦连接超时就会卡住演奏；加上 SSP / 伪春菜协议历经多年更新，TMIDI 的内置 SSTP 已在现代环境下失效。

本插件以 **DDE + PLUGIN/2.0** 方案替代原有的 TCP/SSTP 通路：
- **DDE** 与 TMIDI 通信，获取播放状态和曲目信息（TMIDI 至今仍保留完整的 DDE 服务）
- **PLUGIN/2.0** 将脚本直接注入 SSP，无需网络连接、无超时阻塞

## 功能

- **零配置 DDE 检测** — 自动发现所有正在运行的 TMIDI Player 实例
- **多实例支持** — 同时跟踪所有运行中的 TMIDI Player 实例
- **暂停/恢复不误报** — 仅在首次播放或停止后重新播放时触发
- **自动重连** — TMIDI 重启后静默恢复连接
- **模板可自定义** — 编辑 `sstp_sample.txt` 修改播报内容

## 运行环境

- **TMIDI Player**（DDE 服务名：`TMIDI`）
- **SSP**（Sakura Script Player），需加载人格
- **Windows**（DDE 使用系统 ANSI 代码页）

## 安装方法

从 Release 中下载最新版本提供的 NAR 文件。

- **安装方案 1**：打开 SSP，将 NAR 文件拖入，点击确定覆盖

- **安装方案 2**：将 NAR 文件的默认打开方式设为 SSP，双击运行，点击确定覆盖

- **安装方案 3**：将 NAR 文件的后缀名改为 `.zip`，解压覆盖到 `SSP/plugin/tmidi_msg` 目录下

```
SSP/
  plugin/
    tmidi_msg/
      descript.txt
      install.txt
      message.chinese.txt
      message.english.txt
      message.japanese.txt
      sstp_sample.txt
      tmidi_msg.dll
```

安装完成后，在 TMIDI Player 中播放 MIDI 文件，角色便会说出曲目信息。

## 模板配置

编辑 `sstp_sample.txt` 自定义播报内容。格式与 TMIDI Player 原生 `sstp_sample.txt` 类似，可直接沿用原有配置（**注意：`$target` 和 `$module` 不被支持**）。

`sstp_sample.txt` 支持检测 WRD 类型，插件根据播放文件自动选择：

| 节区         | 触发条件                        |
|--------------|---------------------------------|
| `#MIMPIWRD`  | 存在同名的 `.wrd` 或 `.dv` 文件 |
| `#SherryWRD` | 存在同名的 `.sry` 文件          |
| `#NeoWRD`    | 播放文件自身扩展名为 `.neo`     |
| `#NoWRD`     | 未检测到以上任何 WRD（默认）    |

### 模板变量

| 变量      | 替换值     |
|-----------|------------|
| `$title`  | 曲目标题   |
| `$format` | 文件格式   |

## 工作原理

```
TMIDI Player ──DDE──▶ tmidi_msg.dll ──PLUGIN/2.0──▶ SSP ──Sakura Script──▶ Ghost
```

- 使用 **DDEML**（`APPCLASS_STANDARD`）建立持久会话
- 通过 `OnSecondChange` 每秒轮询 `getstatus`
- 检测到状态为 `"play"` 且文件名与上次不同时，通过 `gettitle` 获取标题并注入脚本
- DDE 字符串使用 `CP_WINUNICODE`，数据交换使用 `CF_TEXT` + 系统 ANSI 代码页

已知问题：如果在 SSP 插件的定时器周期内（最短 1 秒）快速停止再重新播放同一个文件，或不停止直接重新打开同一个文件，插件不会检测到文件名变更，因此无法触发对话（不过我想应该没人会这么做吧）。

## 编译

```bash
cl /LD /O2 /MT /Fe:tmidi_msg.dll tmidi_msg.cpp user32.lib shell32.lib
```

需要 MSVC（`cl.exe`）。SSP 是 32 位程序，请使用 x86 工具链编译（如 `vcvars32.bat`）。生成单个 `tmidi_msg.dll`。

## 许可证

原始 TMIDI Player 许可证详见 `readme.txt`。插件代码可自由使用和修改。
