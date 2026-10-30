# QtScrcpy 本機自編自跑與隨時修改指引 (Local Build, Run & Development Guide)

> 本文件針對將 [QtScrcpy](file:///d:/Projects/qtscrcpy) 配置為類似 [DroidMultiSync](file:///d:/Projects/DroidMultiSync/start_droid_multi_sync.bat)「本機一鍵執行、隨時修改代碼即時生效」之開發架構進行完整評估與實作規劃。

---

## 一、 架構本質差異對比 (Python vs. C++ / Qt)

[DroidMultiSync](file:///d:/Projects/DroidMultiSync) 與 [QtScrcpy](file:///d:/Projects/qtscrcpy) 採用不同的技術堆疊，開發與執行機制如下：

| 維度 | DroidMultiSync | QtScrcpy |
| :--- | :--- | :--- |
| **程式語言** | Python 3 (動態直譯型) | C++11 (靜態編譯型) |
| **圖形與多媒體框架** | PySide6 / PyQt | Qt 5.12+ / Qt 6 + OpenGL + FFmpeg |
| **代碼修改後生效方式** | 存檔後重新執行腳本即刻載入最新 `.py` 檔案 | 存檔後必須調用編譯器 (MSVC/CMake) 進行**增量編譯 (Incremental Build)** 重新生成二進位 `QtScrcpy.exe` |
| **啟動腳本核心職責** | 優先尋找綠色打包版，無則調用 `pythonw -m` 直譯原始碼 | 優先檢測已編譯產物，若原始碼有異動則觸發增量編譯與資源同步，隨後啟動 |

---

## 二、 本機環境盤點診斷 (實機檢測結果)

目前本機工具鏈已 100% 就緒：

1. **已具備之編譯工具**：
   - **MSVC 編譯器環境**：`C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools`
   - **CMake**：`C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` (版本 3.19+)
   - **Ninja 建置器**：`1.11.1`
   - **專案內嵌依賴**：FFmpeg、ADB、scrcpy-server 均已完整就緒於 `QtScrcpy/QtScrcpyCore/src/third_party`。
2. **已安裝之 Qt 開發套件**：
   - **路徑**：`D:\Qt\5.15.2\msvc2019_64`
   - **核心工具**：`qmake.exe` (v3.1)、`windeployqt.exe`、CMake 模組 `D:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5` 均已測試驗證正常。

---

## 三、 已交付之一鍵啟動與開發腳本

專案根目錄已建立並驗證以下腳本：

1. [`start_qtscrcpy.bat`](file:///d:/Projects/qtscrcpy/start_qtscrcpy.bat)：
   - **快速啟動**：若 `output\x64\RelWithDebInfo\QtScrcpy.exe` 已存在，直接以獨立免安裝模式秒開啟動。
   - **自動補建**：若初次執行或可執行檔缺失，自動進行 CMake 配置、MSVC 增量編譯、資源部署（`adb`、`scrcpy-server`、`keymap`、`config`）與 `windeployqt`，隨後啟動。
2. [`build_and_run.bat`](file:///d:/Projects/qtscrcpy/build_and_run.bat)：
   - **隨改隨編專用**：自行修改 C++ / UI 代碼後，雙擊此腳本自動呼叫增量編譯（僅重編異動檔案，通常 3~8 秒），完成後自動重啟新版程式。

---

## 四、 雙軌日常開發工作流建議

| 模式 | 適用情境 | 操作方式 |
| :--- | :--- | :--- |
| **日常使用 / 秒開** | 純粹使用投屏與控制功能 | 直接雙擊 [`start_qtscrcpy.bat`](file:///d:/Projects/qtscrcpy/start_qtscrcpy.bat)。 |
| **代碼修改 / 即時驗證** | 修改 `.cpp`、`.h` 或介面後查看效果 | 儲存代碼後雙擊 [`build_and_run.bat`](file:///d:/Projects/qtscrcpy/build_and_run.bat)。 |
| **IDE 視覺化開發** | 需要繪製 `.ui` 介面、除錯打斷點 (Breakpoint)、符號跳轉 | 使用 **Qt Creator** 或 **VS Code + CMake Tools** 開啟專案根目錄 `CMakeLists.txt`，CMake 模組指定 `D:\Qt\5.15.2\msvc2019_64` 即可。 |

