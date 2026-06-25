# OJfix

OJfix 是一个面向在线评测（Online Judge）场景的 Windows 后台辅助工具。它可以监听剪切板纯文本，把剪切板内容或 GDI 截屏发送给 OpenAI 兼容的 Chat Completions 接口，并把模型回复写回剪切板。

Release 版本没有可见窗口，也不会弹出命令行窗口。程序以单进程后台方式运行，通过全局热键控制。

## 功能特性

- 监听剪切板纯文本变化。
- 将当前剪切板文本发送给大模型。
- 使用 GDI/WinAPI 直接截取完整虚拟桌面，并以图片形式发送给大模型。
- 将模型输出自动写回剪切板。
- 内置多组系统提示词：Java、MSVC C++、GNU G++ C++17、Python 3。
- 使用流式响应解析。
- 支持主接口/备用接口回退，并在主接口失败后临时锁定备用接口，避免反复等待主接口超时。
- Release 构建使用 Windows 子系统、静态运行库、优化选项、图标资源，并支持 UPX 压缩。
- 使用命名互斥量保证只能运行一个实例。
- 运行错误会写入日志，尽量保持程序继续运行。

## 热键

| 热键 | 功能 |
| --- | --- |
| `Ctrl + Alt + 1` | 将当前剪切板文本发送给模型 |
| `Ctrl + Alt + 2` | 使用 GDI 直接截屏并发送给模型 |
| `Ctrl + Alt + 3` | 切换系统提示词 |
| `Ctrl + Alt + 4` | 开启/关闭提示词切换通知 |
| `Ctrl + Alt + 5` | 退出后台程序 |

提示词切换通知默认关闭。

## 配置方式

OJfix 会按以下顺序读取 API 配置：

1. 环境变量
2. `OJfix.exe` 同目录下的 `ojfix.ini`
3. 程序内置的安全默认值

环境变量：

| 名称 | 说明 |
| --- | --- |
| `OJFIX_API_KEY` | OpenAI 兼容接口的 API Key |
| `OJFIX_PRIMARY_API_BASE_URL` | 主接口地址，例如 `https://api.openai.com/v1` |
| `OJFIX_FALLBACK_API_BASE_URL` | 备用接口地址，可留空 |
| `OJFIX_API_MODEL` | 模型名称 |

`ojfix.ini` 示例：

```ini
[api]
key=sk-your-api-key
primary_base_url=https://api.openai.com/v1
fallback_base_url=
model=gpt-4.1-mini
```

仓库不会包含真实 API Key。请把自己的 `ojfix.ini` 放在 `OJfix.exe` 同目录下，本地配置文件已被 `.gitignore` 忽略。

## 编译

环境要求：

- Windows 10 或更新版本
- Visual Studio 2022
- MSVC v143 工具集
- 可选：UPX，用于压缩 Release 可执行文件

编译 Debug：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\OJfix.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

编译 Release：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\OJfix.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

使用 UPX 打包 Release：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-release.ps1
```

打包后的程序会输出到：

```text
dist\OJfix.exe
```

## 目录结构

```text
include/ojfix/        项目头文件
src/                  C++ 源文件
resources/            RC 文件和图标资源
scripts/              构建与打包脚本
config/               示例运行配置
```

## 日志

日志文件位置：

```text
%LOCALAPPDATA%\OJfix\OJfix.log
```

## 注意事项

- 截图提问会截取完整虚拟桌面，多显示器环境下会包含所有屏幕。
- 如果未配置 API Key，程序不会退出，但模型请求会失败并写入日志。
- 模型提示词要求只输出纯代码，不输出 Markdown 代码围栏或语言标签。
- 由于程序没有 UI，退出请使用 `Ctrl + Alt + 5`。

## 许可证

MIT
