# 固件内嵌资源（EMBED）

放随 **app 固件** 一起烧录 / OTA 的静态资源，**不要**放进 `xingzhi-assets`（resources）或语言 `assets` 分区 bin。

| 文件 | 用途 |
|------|------|
| `ic_s_assistant_hint_en.a2i1` | 百问空会话英文全屏提示（480×800） |

CMake：`main/CMakeLists.txt` 的 `EMBED_FILES`。符号名按文件名：`_binary_<name>_{start,end}`。
