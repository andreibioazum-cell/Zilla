# Фикс билда `🤖 Android / Editor (SCU, arm64, ultra-fast)` — run #50

## Что на самом деле происходит

Билд **не падает на Gradle**. По логу job `102392920007` (run `34329012541`, коммит `5438e83`):

```
> Task :generateGodotEditor UP-TO-DATE
BUILD SUCCESSFUL in 2s
69 actionable tasks: 69 up-to-date
./editor/build/outputs/apk/android/debug/android_editor-android-debug.apk
```

APK собран и лежит в
`platform/android/java/editor/build/outputs/apk/android/debug/android_editor-android-debug.apk`
(плюс копия в `bin/android_editor_builds/`).

Шаг `Generate APK via Gradle (explicit)` (длится 3 с) падает на **диагностической** строке:

```bash
ls -R editor/build 2>&1 | head -n 500
```

`run:`-блоки в GitHub Actions выполняются через `bash -eo pipefail`.
`head -n 500` закрывает пайп после 500 строк, `ls` получает broken pipe и завершается
с кодом 2 (`ubuntu-24.04`, coreutils 9.4), а `pipefail` протаскивает этот код как код шага.
В логе видно ровно это: вывод `ls` обрывается на
`editor/build/intermediates/global_synthetics_dex/androidDebug/mergeAndroidDebugGlobalSynthetics:`
и сразу идёт `##[error]Process completed with exit code 2.`

Из-за этого пропускаются `List outputs`, `Upload APK`, `Save ccache`, `Save Godot build cache` —
то есть **артефакт с APK не загружается, хотя сам APK есть**.

## Что изменено

`.github/workflows/android_builds.yml`, шаги `Generate APK via Gradle (explicit)` и `List outputs`:
вывод `ls`/`find` сначала пишется в файл в `$RUNNER_TEMP`, и уже файл обрезается `head`
(без пайпа), плюс `|| true` на каждой диагностике. Упасть теперь может только реальный Gradle.

Воспроизведение локально (старый вариант → код 141/2 и обрыв шага, новый → 0):

```
OLD: ls -R tree | head -n 500   -> exit 141 (SIGPIPE) / 2 (write error), шаг прерван
NEW: ls -R tree > f; head -n 500 f  -> exit 0
```

## Почему это нельзя запушить автоматически

GitHub App, которым работает агент, не имеет права `workflows`:

```
! [remote rejected] (refusing to allow a GitHub App to create or update workflow
  `.github/workflows/android_builds.yml` without `workflows` permission)
```

## Как применить (нужен человек с доступом к репозиторию)

Исправленный файл целиком лежит в корне ветки: `android_builds_fixed.yml`.

### Вариант 1 — через GitHub UI

1. Открыть `.github/workflows/android_builds.yml` → **Edit**.
2. Заменить содержимое на содержимое `android_builds_fixed.yml`.
3. **Commit changes** в `master`.

### Вариант 2 — одной командой локально

```bash
git checkout master && git pull
cp android_builds_fixed.yml .github/workflows/android_builds.yml
git add .github/workflows/android_builds.yml
git commit -m "fix(ci): stop 'ls | head' from failing the Gradle step"
git push origin master
```

### Минимальная правка (если не хочется менять весь файл)

В шаге `Generate APK via Gradle (explicit)` заменить

```bash
find . -name "*.apk" | head -n 100
ls -R editor/build 2>&1 | head -n 500
ls -R app/build 2>&1 | head -n 200 || true
```

на

```bash
find . -name "*.apk" > "$RUNNER_TEMP/apks.txt" 2>&1 || true
head -n 100 "$RUNNER_TEMP/apks.txt" || true
ls -R editor/build > "$RUNNER_TEMP/editor_build.txt" 2>&1 || true
head -n 500 "$RUNNER_TEMP/editor_build.txt" || true
ls -R app/build > "$RUNNER_TEMP/app_build.txt" 2>&1 || true
head -n 200 "$RUNNER_TEMP/app_build.txt" || true
```

И в шаге `List outputs` заменить

```bash
find platform/android -type f \( -name "*.apk" -o -name "*.aab" -o -name "*.so" \) | head -n 100
...
ls -R platform/android/java/editor/build 2>&1 | head -n 300 || true
```

на

```bash
find platform/android -type f \( -name "*.apk" -o -name "*.aab" -o -name "*.so" \) > "$RUNNER_TEMP/android_outputs.txt" 2>&1 || true
head -n 100 "$RUNNER_TEMP/android_outputs.txt" || true
...
ls -R platform/android/java/editor/build > "$RUNNER_TEMP/editor_build_full.txt" 2>&1 || true
head -n 300 "$RUNNER_TEMP/editor_build_full.txt" || true
```

## Что должно получиться после применения

Шаг `Generate APK via Gradle (explicit)` → success, дальше выполнятся `List outputs`,
`Upload APK` (артефакт `android-editor-apk` с `android_editor-android-debug.apk`) и шаги сохранения кэша.
