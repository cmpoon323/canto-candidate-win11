# 建置與測試紀錄（v0.9 Offline Core）

建置日期：2026-08-15（GMT+8）。

| 檢查項目 | 結果 | 證據／備註 |
|---|---|---|
| Rime-Cantonese 詞庫編譯 | 通過 | 使用隨專案附帶的字表、詞表及 `essay-cantonese.txt`，產生 CANTOLEX v2 索引。 |
| 直接映射詞條 | 通過 | 輸入資料 137,439 條；索引共 168,252 條，另有 30,813 條高頻詞本機碼推導。 |
| 索引體積與完整性 | 通過 | `offline_lexicon.bin` 為 6,302,529 bytes，建置報告記錄來源與索引 SHA-256。 |
| 常用候選驗收 | 通過 | `neihou→你好`、`m4goi→唔該`、`gwongdung→廣東`、`hoenggong→香港`、`ngo→我`、`sikfaan→食飯`。 |
| Python 查詢效能基準 | 通過 | 100 次本機查詢平均約 5.31 ms、P95 約 6.32 ms；此為驗證工具的 Python 基準，不等於 Windows EXE 實機延遲。 |
| Windows x64 建置 | 通過 | 成功產出 `dist/CantoCandidate.exe`；格式為 `PE32+ executable (GUI) x86-64`。 |
| 網絡相依 | 通過 | 程式碼已移除 `inputtools`／`WinHttp`；PE 進口表不含 `WINHTTP.dll`。 |
| Windows 內建 DLL 相依 | 通過 | 僅匯入 `GDI32`、`KERNEL32`、`MSVCRT`、`OLE32`、`SHELL32`、`USER32`。 |
| PE 安全旗標 | 通過 | 產物具 `HIGH_ENTROPY_VA`、`DYNAMIC_BASE`（ASLR）及 `NX_COMPAT`（DEP/NX）。 |
| 離線詞庫安全讀取 | 靜態建置通過，待 Windows 11 實機確認 | 啟動時限制索引為 32 MiB／500,000 條，拒絕重新解析點、格式錯誤、截斷或未排序資料。 |
| Windows 11 端到端操作 | 待實機確認 | 須驗證 EXE 能載入同資料夾索引、候選窗顯示 `OFFLINE`、快捷鍵、貼上、詞庫缺失錯誤及 Defender 行為。 |
| 發行 ZIP 完整性 | 待最終封裝後驗證 | 將使用 `unzip -t` 與 SHA-256 驗證。 |

> Linux 的交叉編譯及 Python 詞庫驗收無法替代 Windows 11 對低階鍵盤掛鈎、系統匣、OLE 剪貼簿、候選窗、不同 App 和 Defender 的實機驗收。請依 `WINDOWS11_TEST_CHECKLIST.md` 完成最後測試。
