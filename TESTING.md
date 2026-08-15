# 建置與測試紀錄（v0.8）

建置日期：2026-08-15（GMT+8）。

| 檢查項目 | 結果 | 證據／備註 |
|---|---|---|
| Windows x64 建置 | 通過 | MinGW-w64 成功產出 `dist/CantoCandidate.exe`；格式為 `PE32+ executable (GUI) x86-64`。 |
| 自足式相依與 PE 硬化 | 通過 | 靜態 C++ 執行庫；檢測到 `HIGH_ENTROPY_VA`、`DYNAMIC_BASE`、`NX_COMPAT`。 |
| Windows 內建 DLL 相依 | 通過 | 僅匯入 `GDI32`、`KERNEL32`、`MSVCRT`、`OLE32`、`SHELL32`、`USER32`、`WINHTTP`。 |
| 線上候選與容錯 | 通過 | 候選服務可回傳最多 45 項；既有容錯樣本見 `research/FUZZY_MATCH_TESTS.md`。 |
| 本機近音變體 | 靜態建置通過 | 實作 `n/l`、`ng`、`h/f`、末尾 `ng/n`、單字元刪除與相鄰交換；上限 8 個，僅查本機快取。 |
| 本機近音後備 | 靜態建置通過，待 Windows 11 實機確認 | 線上失敗／無候選時先用精確快取，再用近音快取；候選窗應顯示來源。 |
| 語境排序 | 靜態建置通過，待 Windows 11 實機確認 | 僅在選字後記錄相鄰已確認詞；最多保存 5,000 條並可清除。 |
| 短語、快取與資料管理 | 靜態建置通過，待 Windows 11 實機確認 | 既有 v0.7 功能與 v0.8 語境歷史管理已整合至系統匣。 |
| 設定控制 | 靜態建置通過 | `local_fuzzy_suggestions` 與 `context_ranking` 為 `0` 時分別停用相關功能。 |
| GitHub Actions | 已更新，待 GitHub Actions 實際執行確認 | 工作流程將 v0.8 規格與最新版資產一併打包。 |
| 發行 ZIP 完整性 | 待最終封裝後驗證 | 將以 `unzip -t` 與 SHA-256 確認。 |

> Linux 建置不能取代 Windows 11 的低階鍵盤掛鈎、OLE 剪貼簿、系統匣、不同 App caret、候選顯示和實際網絡／離線切換測試。請依 `WINDOWS11_TEST_CHECKLIST.md` 完成最終驗收。
