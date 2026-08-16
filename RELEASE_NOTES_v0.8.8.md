# CantoCandidate v0.8.8 Online — 剪貼簿時序修正版 RC

v0.8.8 修正 v0.8.7 在 Microsoft Word、Chrome 等非標準文字控制項上出現「候選可選、log 顯示 Ctrl+V 已送出，但沒有文字」的確定性錯誤。

## 根因與修正

v0.8.7 的 log 已證實：`Ctrl+V fallback used delivered=1`。這只表示 Windows 已把 Ctrl+V 放進目標程式的訊息佇列，並不表示 Word 或 Chrome 已立即讀取剪貼簿。v0.8.7 在同一函式結束前就還原剪貼簿，令目標 App 處理 Ctrl+V 時讀到的是還原後內容，因而沒有候選字。

v0.8.8 會區分兩種情況：

| 提交方式 | 剪貼簿還原行為 |
|---|---|
| 標準控制項的直接 `WM_PASTE` | 此訊息為同步處理，完成後立即依既有規則還原。 |
| 非標準控制項的 Ctrl+V 後備 | 保留候選字 180 ms，再使用既有剪貼簿序號檢查還原；其他程式在這段期間改變剪貼簿時不會覆蓋該改動。 |

## 測試

請完全結束舊版 CantoCandidate，解壓本 RC 到新資料夾後，在 Microsoft Word 空白文件輸入粵拼、選擇候選；重複測試至少 10 次。再在 Chrome 文字輸入框測試相同行為。預期候選字應自動顯示於目前插入點，且剪貼簿在約 180 ms 後還原。

成功時，`diagnostics.log` 會依序出現類似：

```text
WM_PASTE target=_WwG standard=0 delivered=1
Ctrl+V fallback used delivered=1
clipboard restore deferred after injected Ctrl+V
```

若仍不上屏，請將這三行與 Word/Chrome 版本貼回。若發生 Event ID 1000 且 `hmpalert.dll` 為 faulting module，請同時提供事件紀錄；不要自行停用或排除端點防護。
