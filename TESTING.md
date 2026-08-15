# 建置與測試紀錄（v0.6）

建置日期：2026-08-15（GMT+8）。

| 檢查項目 | 結果 | 證據／備註 |
|---|---|---|
| Windows x64 建置 | 通過 | MinGW-w64 成功產出 `dist/CantoCandidate.exe`；格式為 `PE32+ executable (GUI) x86-64`。 |
| 自足式相依 | 通過 | 以靜態 C++ 執行庫建置，不依賴額外的 `libgcc` 或 `libstdc++` DLL。 |
| Windows 內建 DLL 相依 | 通過 | 僅匯入 `GDI32`、`KERNEL32`、`MSVCRT`、`OLE32`、`SHELL32`、`USER32`、`WINHTTP`。 |
| PE 安全旗標 | 通過 | 檢測到 `HIGH_ENTROPY_VA`、`DYNAMIC_BASE`（ASLR）及 `NX_COMPAT`（DEP/NX）。 |
| Google 候選服務 | 通過 | 實際 HTTPS 請求可取得最多 45 個粵語候選；容錯樣本見 `research/FUZZY_MATCH_TESTS.md`。 |
| 候選翻頁、焦點、標點與 Shift | 靜態建置通過，待 Windows 11 實機確認 | 控制流程已納入來源碼；需以不同目標 App 確認按鍵事件。 |
| caret 定位與滑鼠後備 | 靜態建置通過，待 Windows 11 實機確認 | 使用 `GetGUIThreadInfo`／`rcCaret`；無法讀取時退回滑鼠位置。 |
| 剪貼簿保留 | 靜態建置通過，待 Windows 11 實機確認 | 使用 OLE 取得資料物件並以剪貼簿序號避免覆蓋使用者在 150 ms 內的新複製內容。 |
| 設定檔 | 靜態建置通過 | `settings.ini` 只接受 `0` 或 `1`，無效內容回退安全預設值。 |
| 自動建置流程 | 已加入，待 GitHub Actions 實際執行確認 | `.github/workflows/build.yml` 會在推送、PR、標籤及手動觸發時建置 Windows 產物。 |
| 發行 ZIP 完整性 | 通過 | `unzip -t` 顯示 v0.6 壓縮檔無錯誤，並附有 SHA-256。 |

> 建置環境為 Linux，無法替代 Windows 11 的低階鍵盤掛鈎、OLE 剪貼簿、不同 App caret 和 Windows Defender 實機驗證。請依 `WINDOWS11_TEST_CHECKLIST.md` 完成最終驗收；所有失敗項目應記錄目標 App、Windows 版本和重現步驟。
