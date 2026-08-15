# 第三方資料與授權通知

## Rime-Cantonese 粵拼詞庫

CantoCandidate v0.9 的 `offline_lexicon.bin` 由 [Rime-Cantonese](https://github.com/rime/rime-cantonese) 的公開資料編譯而成，資料來源為 Cantonese Computational Linguistics Infrastructure Development Workgroup（CanCLID）及其貢獻者。Rime-Cantonese README 表示其主要部分採用 **Creative Commons Attribution 4.0 International（CC BY 4.0）**。

本發行檔只處理下列資料：

| 原始檔 | 用途 | 處理方式 |
|---|---|---|
| `jyut6ping3.chars.dict.yaml` | 字與粵拼映射 | 直接編譯為離線索引。 |
| `jyut6ping3.words.dict.yaml` | 詞語與粵拼映射 | 直接編譯為離線索引。 |
| `essay-cantonese.txt` | 本機候選頻率及安全推導常用詞 | 用於排序；僅在可取得字的粵拼讀音時產生有限離線碼。 |

本發行檔**不使用** `jyut6ping3.maps.dict.yaml`，因此不把該檔的 ODbL 資料納入索引；但為完整保存來源專案的授權資料，發行包仍附上 `third_party/rime-cantonese/LICENSE-CC-BY` 與 `LICENSE-ODbL`。

CantoCandidate 對原始資料作了技術性編譯、聲調移除正規化、頻率排序和有限短語碼推導。此修改不是 Rime-Cantonese 官方資料，亦不代表 CanCLID、Rime 或其貢獻者認可本專案。原始來源、著作權及本通知應與任何再發佈的離線詞庫一併保留。

- 原始資料來源：[rime/rime-cantonese](https://github.com/rime/rime-cantonese)
- 主要資料授權：[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- Rime-Cantonese README：[授權與粵拼方案說明](https://github.com/rime/rime-cantonese/blob/main/README-en.md)
