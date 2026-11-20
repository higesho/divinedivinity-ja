# ディヴァイン・ディヴィニティ 日本語化 version.dll のソース

Steam 版 Divine Divinity（div.exe V1.0062a）の文字表示を日本語にする `version.dll` と、その組み立てに使う道具です。

- `src/` … version.dll 本体（C）。Visual Studio 2022 の x86 ツールで `src\build.bat` を実行すると `build\version.dll` ができます。
- `tools/` … Python の道具。`extract_all.py` で原作の文章を集め、`sh tools/rebuild.sh`（merge → glossfix → make_data → make_font → build.bat）でまとめて組み立てます。`make_zip.py` は配布用の zip を作ります。`win.py` は試験用の起動・操作の道具です。

必要なもの: Python 3（Pillow、fontTools）、Noto Sans JP と Noto Serif JP、訳の表（このリポジトリには入っていません）。
原作のファイル（div.exe、localizations、dat など）は各自で用意してください。
