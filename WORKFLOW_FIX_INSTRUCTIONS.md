# Fix для последнего билда — инструкция из-за ограничения GitHub App

## Причина падения

Последний билд `34326760604` (`5b8e363`, job `102385694301`) падает на шаге:

```
Generate APK via Gradle (explicit) — failure (exit 1, 3с)
```

**Корень:** рассинхрон между обрезанным `platform/android/java/build.gradle`
(`supportedAndroidDistributions = ["android"]`, только `generateGodotEditor`)
и кодом который требует несуществующие задачи:

- `platform/android/platform_android_builders.py:20`
  ```python
  gradle_process += ["generateGodotEditor", "generateGodotHorizonOSEditor", "generateGodotPicoOSEditor"]
  ```
- `.github/workflows/android_builds.yml:94`
  ```bash
  ./gradlew generateGodotEditor generateGodotHorizonOSEditor generateGodotPicoOSEditor --info
  ```

Gradle отвечает: `Task 'generateGodotHorizonOSEditor' not found` → `exit 1`.

Дополнительно:
- `build.gradle:60` `onlyIf { ... || generateGodotMonoTemplates.state.executed }` ссыдается на несуществующий таск `generateGodotMonoTemplates` → `MissingPropertyException`
- `android_builds.yml` `Upload APK` ищет `app/build/...` вместо `editor/build/...` и `bin/android_editor_builds` → даже если Gradle успешен, `Upload` с `if-no-files-found: error` упадёт с `No files were found`

До `ce3703c` это маскировалось: `Upload: if-no-files-found: warn + continue-on-error: true` и `subprocess.run` без `check=True` (тихая ошибка).

## Что уже исправлено и запушено в `arena/01a08533-zilla`

Коммит `3898b64` (уже на GitHub):

1. `platform/android/platform_android_builders.py` — только `generateGodotEditor`, теперь `sys.exit` при ошибке Gradle
2. `platform/android/java/build.gradle` — `onlyIf { generateGodotTemplates.state.executed }` (убрана ссылка на моно)

## Что нужно исправить вручную (workflows permission)

Файл `.github/workflows/android_builds.yml` нужно исправить вручную. Полный исправленный файл лежит в `android_builds_fixed.yml` в корне ветки.

### Быстрый способ — скопировать

```bash
cp android_builds_fixed.yml .github/workflows/android_builds.yml
git add .github/workflows/android_builds.yml
git commit -m "fix(workflow): android-only editor"
git push origin arena/01a08533-zilla
```

### Или патчем

```bash
patch -p1 < workflow_fix.patch
```

### Или через GitHub UI

Скопируйте содержимое `android_builds_fixed.yml` в `.github/workflows/android_builds.yml` через Edit на GitHub.

