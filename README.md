# Drak- — Kurdish Sorani (کوردیی سۆرانی) Unity APK Translator

ئامرازێکی Pythonـە بۆ دەرهێنان، تەرجەمەکردن بۆ **کوردیی سۆرانی (`ckb`)** (لەگەڵ چارەسەری RTL و پێکەوەلکاندنی پیتەکان)، و دووبارە پێچانەوەی فایلەکانی یاری Unity Android APK لەسەر بنەمای **[UnityPy](https://github.com/K0lb3/UnityPy)**.

---

## دامەزراندن (Installation)

```bash
python3 -m venv .venv
source .venv/bin/activate   # لەسەر Windows: .venv\Scripts\activate
pip install -r requirements.txt
```

## بەکارهێنان (Usage)

### ١. دەرهێنانی دەقەکان لە APK یان لینکی Dropbox
```bash
python -m kurdish_game_translator.cli extract "https://www.dropbox.com/scl/fi/bby2q4npuakdxgsfvdd8x/Fallen-Elf-Freya-vFinal-Mod.apk?rlkey=r66fxm4hzsxytkccii39yturw&st=57ngxf5p&dl=0" -o work
```
یان ئەگەر فایلەکە لەسەر کۆمپیوتەر/مۆبایلەکەتە:
```bash
python -m kurdish_game_translator.cli extract Fallen-Elf-Freya-vFinal-Mod.apk -o work
```

### ٢. تەرجەمەکردن بۆ کوردیی سۆرانی (`ckb`)
- ئەگەر یارییەکە بە TextMeshPro / RTLTMPro یان سیستەمی ڕاست-بۆ-چەپ کار دەکات (`logical`):
```bash
python -m kurdish_game_translator.cli translate work/strings.json -o work/strings_ckb.json --rtl-mode logical
```
- ئەگەر دەقە کوردییەکان بە پچڕپچڕ و پێچەوانە دەرکەوتن (`visual`):
```bash
python -m kurdish_game_translator.cli translate work/strings.json -o work/strings_ckb.json --rtl-mode visual
```

### ٣. خستنەوەی دەقە تەرجەمەکراوەکان ناو APK (و گۆڕینی فۆنت ئەگەر پێویست بوو)
```bash
python -m kurdish_game_translator.cli pack Fallen-Elf-Freya-vFinal-Mod.apk work/strings_ckb.json -o Fallen-Elf-Freya-ckb.apk
```
*(ئەگەر پیتە کوردییەکان وەک چوارگۆشە دەرکەوتن، فۆنتێکی وەک `NotoSansArabic-Regular.ttf` بە `--font path/to/font.ttf` زیاد بکە.)*

### ٤. بۆ یارییە گەورەکانی UnityFS (`android.ab` / AvgCreator وەک Fallen Elf Freya)
کاتێک فایلی `android.ab` زۆر گەورەیە (~٥ گێگابایت کاتێک دەکرێتەوە)، `avg-extract` و `avg-pack` تەنها بەشی `CAB` و `game.json` دەردەهێنن و پاشان بە کەمترین RAM دووبارە دەیپێچنەوە:
```bash
# ١. دەرهێنانی game.json و unique_en.json لە android.ab
python -m kurdish_game_translator.cli avg-extract android.ab -o work_avg

# ٢. تەرجەمەکردنی unique_en.json بۆ کوردیی سۆرانی
python -m kurdish_game_translator.cli avg-translate work_avg/unique_en.json -o work_avg/unique_ckb.json

# ٣. خستنەوەی دەقە کوردییەکان بە پێچەوانەکردنەوەی دروست (visual RTL + line-wrapping)
python -m kurdish_game_translator.cli avg-pack android.ab work_avg/unique_ckb.json -w work_avg -o work_avg/android_ckb.ab --rtl-mode visual

# ٤. گۆڕینی فۆنتەکان لە bin/Data/ بۆ فۆنتی کوردی-پشتیوان (بۆ نەمانی چوارگۆشەی □□□)
python -m kurdish_game_translator.cli patch-font bin/Data/20c529ce41df043bba11419a7bb4ec8d DejaVuSans.ttf -o bin/Data/20c529ce41df043bba11419a7bb4ec8d
```

### ٥. ڕەنکردنی ڕاستەوخۆ لەسەر GitHub Actions (بەبێ کۆمپیوتەر)
لەناو ڕیپۆکەدا لە GitHub بڕۆ بەشی **Actions** -> **Kurdish Sorani APK Translator** -> **Run workflow**:
1. لینکی Dropboxـی APKـەکە بنووسە لە `apk_url`.
2. `extract_only` یان `full_pipeline` هەڵبژێرە.
3. دوای تەواوبوون لە بەشی **Artifacts** فایلی `strings.json` یان `translated_ckb.apk` داببەزێنە.

---

## تێبینی بۆ یارییەکانی IL2CPP
ئەگەر یارییەکە دەقەکانی لەناو `assets/bin/Data/Managed` یان `TextAsset`/`JSON` دانەنابێت و لەناو `libil2cpp.so` + `global-metadata.dat` شاردبێتەوە، پێویستە هاوشانی ئەم ئامرازە **[Perfare/Il2CppDumper](https://github.com/Perfare/Il2CppDumper)** یان **[nesrak1/UABEA](https://github.com/nesrak1/UABEA)** بەکاربهێنیت.
