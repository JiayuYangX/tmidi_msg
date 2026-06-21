------------------------------
TMIDI Messages - SSP 插件
将 TMIDI Player 的播放信息通过 Sakura Script 发送给角色
项目地址：https://github.com/JiayuYangX/tmidi_msg
------------------------------

■概要
TMIDI Player 演奏时，插件通过 DDE 自动检测播放状态，读取曲目信息，
以 Sakura Script 形式注入 SSP，让角色说出当前曲目。

支持同时运行多个 TMIDI Player 实例，各自独立检测。
仅在"停止→播放"或切换曲目时触发，暂停后恢复不重复播报。

■使用方法
安装 NAR 文件后即自动生效，无需任何设置。
角色会在 TMIDI 开始播放时自动播报曲目。

如需自定义播报内容，编辑 sstp_sample.txt：

- 支持 #MIMPIWRD / #SherryWRD / #NeoWRD / #NoWRD 四个段落，
  根据 MIDI 文件目录下的 .wrd / .dv / .sry / .neo 文件自动切换。
- 模板变量：
  $title  曲目标题
  $format 文件格式
- 注意：$target、$module、%me 等原有变量不被支持。

■必要环境
- TMIDI Player（DDE 服务名：TMIDI）
- SSP（Sakura Script Player），需加载人格
- Windows（DDE 使用系统 ANSI 代码页）

■更新历史
- 2026/06/21 1.1.0
  改用 C++ 构建
  支持动态监测多进程窗口，自动发现新启动的 TMIDI Player 实例，断开时自动清理，无实例数上限
  修复模板解析，正确处理 sstp_sample.txt 中 4 个段落的分段逻辑
  优化多重边界情况，不再误触发或漏触发
- 2026/06/17 1.0.0
  首次发布
