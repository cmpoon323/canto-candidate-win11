# CantoCandidate v0.8.4 Online — 端點防護安全相容模式

v0.8.4 根據實機 Windows Application Error Event ID 1000 的證據而設計。該事件將 `C:\Windows\System32\hmpalert.dll`（Sophos／HitmanPro.Alert 元件）列為 faulting module，例外碼為 `0xc0000005`。這表示崩潰發生在已注入程序的端點防護模組內；它不證明應用程式不需要修正，也不應以關閉防護或加入排除來掩蓋問題。

| 新行為 | 說明 |
|---|---|
| **啟動時模組偵測** | 若目前程序已載入 `hmpalert.dll`，程式會自動進入安全相容模式。 |
| **沒有全域鍵盤掛鈎** | 安全相容模式不會呼叫 `SetWindowsHookEx(WH_KEYBOARD_LL, …)`，避免背景攔截全系統按鍵。 |
| **沒有注入式自動貼上** | 安全相容模式不使用 `SendInput` 自動貼到另一個 App；候選只會複製到剪貼簿。 |
| **原生前景輸入面板** | 程式會開啟「CantoCandidate Online — 安全相容輸入」視窗。輸入粵拼、按搜尋、選候選、複製，然後在目標 App 手動按 `Ctrl + V`。 |
| **修復中心保護** | 在安全相容模式按「立即修復」只會保留／重新開啟前景面板，不會重新安裝全域鍵盤掛鈎。 |
| **一般電腦維持原有體驗** | 未偵測到 `hmpalert.dll` 時，保留背景候選、NumPad 選字、Shift 切換、候選窗及修復中心。 |

## 對使用者的正確做法

請先完整結束舊版，再將 v0.8.4 解壓到新的資料夾。若自動出現「安全相容輸入」面板，表示偵測到端點防護相容性風險，請使用面板的搜尋與複製流程；這是預期行為，不是故障。

這台電腦為機構網域管理裝置。請把以下資料提交給機構 IT／端點安全團隊：Application Error Event ID 1000、`hmpalert.dll` 3.9.7.1381、例外碼 `0xc0000005`、CantoCandidate v0.8.4 的 SHA-256 和精確重現步驟。Sophos 官方把 hmpalert.dll 導致第三方程式崩潰列為需要管理端收集 dump、SDU 及重現資料的支援情境。[1]

> 不要自行停用 Tamper Protection、重新命名 `hmpalert.dll`／`hmpalert.sys`、加入防護排除，或為了繞過問題而以系統管理員身分執行程式。這些做法需要機構安全管理員決定，並可能降低端點保護。

## Reference

[1] [Sophos Windows Endpoint — HitmanPro Alert including ransomware/exploit detections](https://support.sophos.com/support/s/article/KBA-000006827)
