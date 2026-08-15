# 建置與測試紀錄（v0.3）

建置日期：2026-08-15（GMT+8）。

| 檢查項目 | 結果 | 證據／備註 |
|---|---|---|
| Windows x64 建置 | 通過 | MinGW-w64 成功產出 `dist/CantoCandidate.exe`。PE 格式檢查結果為 `PE32+ executable (GUI) x86-64`。 |
| 自足式相依 | 通過 | 以 `-static -static-libgcc -static-libstdc++` 建置；不依賴額外的 `libgcc` 或 `libstdc++` DLL。 |
| Windows 內建 DLL 相依 | 通過 | 僅匯入 `GDI32.dll`、`KERNEL32.dll`、`msvcrt.dll`、`SHELL32.dll`、`USER32.dll` 和 `WINHTTP.dll`。 |
| 45 個候選字取得 | 通過 | 對 `neihou` 的實際 HTTPS 請求在 `num=50` 時回傳超過 20 項候選，足以供程式以每頁 9 個方式分頁顯示。 |
| 模糊配對 | 通過 | `leihou`、`nihou`、`neihow` 和 `neiho` 實測仍可取得「你好」；詳見 `research/FUZZY_MATCH_TESTS.md`。 |
| 候選翻頁與焦點 | 靜態建置通過，待 Windows 11 實機確認 | 程式實作 `PageUp/PageDown`、方向鍵、`,/.`、`-/=` 翻頁及 `←/→` 焦點選擇；候選視窗顯示頁碼與焦點。 |
| Shift 中英切換 | 靜態建置通過，待 Windows 11 實機確認 | 單按 Shift 切換模式；按住 Shift 配合其他鍵時不切換。 |
| 本機歷史與自訂詞庫 | 靜態建置通過，待 Windows 11 實機確認 | 使用同資料夾的 `history.tsv` 和 `custom_dictionary.tsv`；程式碼僅以 Win32 本機檔案 API 讀寫，候選 HTTP 請求只包含拼音組合。 |
| 壓縮包完整性 | 待最終封裝後重新驗證 | 將以 `unzip -t` 檢查。 |

> 本工具不是 Windows 原生 TSF/IME，也不是 Google 官方 Windows 產品。它使用可攜式背景程式模式，並受 Windows 權限與目標程式保護機制限制。由於建置環境為 Linux，鍵盤掛鈎、候選視窗和貼上行為必須由使用者在 Windows 11 一般權限程式（例如記事本）中作最後實機確認。
