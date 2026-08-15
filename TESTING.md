# 建置與測試紀錄（v0.7）

建置日期：2026-08-15（GMT+8）。

| 檢查項目 | 結果 | 證據／備註 |
|---|---|---|
| Windows x64 建置 | 通過 | MinGW-w64 成功產出 `dist/CantoCandidate.exe`；格式為 `PE32+ executable (GUI) x86-64`。 |
| 自足式相依 | 通過 | 靜態連結 C++ 執行庫，未依賴額外的 `libgcc` 或 `libstdc++` DLL。 |
| Windows 內建 DLL 相依 | 通過 | 僅匯入 `GDI32`、`KERNEL32`、`MSVCRT`、`OLE32`、`SHELL32`、`USER32`、`WINHTTP`。 |
| PE 安全旗標 | 通過 | 產物具 `HIGH_ENTROPY_VA`、`DYNAMIC_BASE`（ASLR）及 `NX_COMPAT`（DEP/NX）。 |
| 線上候選與容錯 | 通過 | 候選服務可回傳最多 45 項；容錯樣本見 `research/FUZZY_MATCH_TESTS.md`。 |
| 離線候選快取 | 靜態建置通過，待 Windows 11 實機確認 | 成功查詢後把原始候選保存至本機快取；服務失敗後才讀取快取並標示離線狀態。 |
| 常用短語 | 靜態建置通過，待 Windows 11 實機確認 | `;shortcut` + `Space/Enter` 從 `snippets.tsv` 展開，不觸發網絡候選請求。 |
| 資料管理 | 靜態建置通過，待 Windows 11 實機確認 | 系統匣可開啟／清除短語、詞庫、歷史及快取；清除全部資料會再次確認。 |
| 候選外觀設定 | 靜態建置通過，待 Windows 11 實機確認 | `settings.ini` 可調整主題、字體大小和每頁候選數；無效值回退至安全預設。 |
| 候選窗、Shift、標點與剪貼簿 | 靜態建置通過，待 Windows 11 實機確認 | 須以 `WINDOWS11_TEST_CHECKLIST.md` 在不同 App、DPI 和權限邊界完成驗收。 |
| GitHub Actions | 已更新，待 GitHub Actions 實際執行確認 | 工作流程會打包 v0.7 的短語、設定及規格文件。 |
| 發行 ZIP 完整性 | 待最終封裝後驗證 | 將使用 `unzip -t` 和 SHA-256 驗證。 |

> Linux 建置成功不等同 Windows 11 的低階鍵盤掛鈎、OLE 剪貼簿、caret、系統匣和 Defender 實機驗證。請完成 `WINDOWS11_TEST_CHECKLIST.md`，並記錄目標程式、Windows 版本、DPI、步驟和截圖。
