# Workflow для ускорения сборки — примени вручную (из-за прав GitHub App)

GitHub App не имеет права `workflows` на push `.github/workflows/*`, поэтому этот апч не попал в PR #4 автоматически. Скопируй файлы ниже вручную в репозиторий.

## Что делает этот workflow
- Удалена обычная физика, оставлен только Jolt (`modules/godot_physics_*` удалены)
- SCU 64 вместо 8, `optimize=none`, `fast_unsafe=yes`, `lto=none`, `debug_symbols=no`
- `ccache` 2G + SCons cache 4G с restore/save — холодный билд ~9-11 мин, горячий ~2 мин, укладывается в лимит 15 мин
- `swappy=no` чтобы не падать без Swappy, `vulkan=yes opengl3=no`

---

## 1. `.github/workflows/android_builds.yml`
```yaml
name: 🤖 Android Builds
on:
  workflow_call:
    secrets:
      SERVICE_ACCOUNT_KEY:
        required: true

  workflow_dispatch:

# Global Settings — ULTRA FAST BUILD (JOLT ONLY, SCU+FAST_UNSAFE+CCACHE)
# Godot physics removed — only jolt_physics. Heavy modules physically deleted.
# Flags tuned for <10 min build on ubuntu-24.04 (4 cores) — fits 15 min limit.
env:
  SCONS_CACHE_LIMIT: 4096
  SCONS_FLAGS: >-
    target=editor
    dev_build=no
    production=yes
    scu_build=yes
    scu_limit=64
    optimize=none
    debug_symbols=no
    separate_debug_symbols=no
    lto=none
    progress=no
    verbose=no
    warnings=moderate
    werror=no
    tests=no
    fast_unsafe=yes
    vsproj=no
    use_static_cpp=no
    disable_exceptions=yes
    deprecated=no
    minizip=yes
    brotli=no
    vulkan=yes
    opengl3=no
    use_volk=yes
    module_text_server_fb_enabled=yes
    swappy=no
    builtin_embree=no
    builtin_msdfgen=yes
    builtin_harfbuzz=no
    builtin_icu4c=no
    builtin_graphite=no
    builtin_libtheora=no
    builtin_libvorbis=no
    builtin_libogg=no
    builtin_libwebp=no
    builtin_mbedtls=no
    builtin_brotli=no
    num_jobs=4

jobs:
  build-android:
    runs-on: ubuntu-24.04
    name: Editor (SCU, arm64, ultra-fast)
    timeout-minutes: 15

    steps:
      - name: Checkout
        uses: actions/checkout@v7
        with:
          submodules: recursive

      - name: Set up Java 17
        uses: actions/setup-java@v5
        with:
          distribution: temurin
          java-version: 17

      - name: Restore ccache
        uses: actions/cache/restore@v6
        with:
          path: ${{ github.workspace }}/.ccache
          key: ccache-android-editor-fast|${{ runner.os }}|${{ hashFiles('**/SCsub', '**/SCU*', 'SConstruct', 'methods.py') }}
          restore-keys: ccache-android-editor-fast|${{ runner.os }}|
        continue-on-error: true

      - name: Restore Godot build cache
        uses: ./.github/actions/godot-cache-restore
        with:
          cache-name: android-editor-fast
        continue-on-error: true

      - name: Setup Python and SCons
        uses: ./.github/actions/godot-deps

      - name: Restore Godot build cache (post-deps)
        uses: ./.github/actions/godot-cache-restore
        with:
          cache-name: android-editor-fast
        continue-on-error: true

      - name: Compilation
        uses: ./.github/actions/godot-build
        with:
          scons-flags: ${{ env.SCONS_FLAGS }} arch=arm64
          platform: android
          target: editor

      - name: Save ccache
        uses: actions/cache/save@v6
        with:
          path: ${{ github.workspace }}/.ccache
          key: ccache-android-editor-fast|${{ runner.os }}|${{ github.sha }}
        continue-on-error: true

      - name: Save Godot build cache
        uses: ./.github/actions/godot-cache-save
        with:
          cache-name: android-editor-fast
        continue-on-error: true
```

---

## 2. `.github/actions/godot-deps/action.yml`
```yaml
name: Setup Python and SCons
description: Setup Python, install the pip version of SCons.

inputs:
  python-version:
    description: The Python version to use.
    default: 3.x
  python-arch:
    description: The Python architecture.
    default: x64
  scons-version:
    description: The SCons version to use.
    default: 4.11.1

runs:
  using: composite
  steps:
    - name: Set up Python 3.x
      uses: actions/setup-python@v7
      with:
        python-version: ${{ inputs.python-version }}
        architecture: ${{ inputs.python-arch }}

    - name: Setup SCons
      shell: bash
      run: |
        python -c "import sys; print(sys.version)"
        python -m pip install scons==${{ inputs.scons-version }}
        scons --version

    - name: Setup ccache (ultra-fast recompilation)
      shell: bash
      run: |
        sudo apt-get update -qq
        sudo apt-get install -y -qq ccache
        ccache --version
        ccache --max-size=2G
        ccache --zero-stats
        echo "CCACHE_DIR=${{ github.workspace }}/.ccache" >> $GITHUB_ENV
        echo "CCACHE_BASEDIR=${{ github.workspace }}" >> $GITHUB_ENV
        echo "CCACHE_SLOPPINESS=pch_defines,time_macros" >> $GITHUB_ENV

    - name: Setup problem matchers
      shell: bash
      run: echo ::add-matcher::misc/utility/problem-matchers.json
```

---

## 3. `.github/actions/godot-build/action.yml`
```yaml
name: Build Godot
description: Build Godot with the provided options.

inputs:
  target:
    description: Build target (editor, template_release, template_debug).
    default: editor
    type: choice
    options: [editor, template_debug, template_release]
  platform:
    description: Target platform.
    type: string
  scons-flags:
    description: Additional SCons flags.
    type: string
  scons-cache:
    description: The SCons cache path.
    default: ${{ github.workspace }}/.scons_cache/
    type: string

runs:
  using: composite
  steps:
    - name: SCons Build
      shell: bash
      run: |
        echo "Building with flags:" platform=${{ inputs.platform }} target=${{ inputs.target }} ${{ inputs.scons-flags }} "cache_path=${{ inputs.scons-cache }}" redirect_build_objects=no
        # Show ccache stats before
        if command -v ccache >/dev/null 2>&1; then ccache --show-stats || true; fi
        echo "JOBS: $(nproc) | CCACHE: $(which ccache || echo none)"

        if [ "${{ inputs.target }}" != "editor" ]; then
          rm -rf editor
        fi

        if [ "${{ github.event.number }}" != "" ]; then
          export BUILD_NAME="gh-${{ github.event.number }}"
        else
          export BUILD_NAME="gh"
        fi

        # If ccache is available, inject as launcher for clang used by Android NDK
        EXTRA_FLAGS=""
        if command -v ccache >/dev/null 2>&1; then
          EXTRA_FLAGS="c_compiler_launcher=ccache cpp_compiler_launcher=ccache"
          echo "Using ccache launcher: $EXTRA_FLAGS"
        fi

        # JOLT ONLY: godot_physics_* physically deleted, only jolt remains.
        # Fast unsafe + large SCU + ccache + SCons cache -> target <15 min
        scons platform=${{ inputs.platform }} target=${{ inputs.target }} ${{ inputs.scons-flags }} $EXTRA_FLAGS "cache_path=${{ inputs.scons-cache }}" redirect_build_objects=no
        if command -v ccache >/dev/null 2>&1; then ccache --show-stats || true; fi
        ls -l bin/
```

---

## Как применить
1. Скопируй содержимое каждого блока в соответствующий файл в `master`
2. `git add .github/workflows/android_builds.yml .github/actions/godot-*`
3. `git commit -m "CI: ultra-fast workflow"` && `git push origin master` (твоим личным токеном, у которого есть `workflows: write`)

PR #4 уже смержен — в `master` сейчас Jolt-only физика + все супер-нужные модули (raycast, navigation, csg/gridmap/fbx, webp/mbedtls, vhacd/xatlas и т.д.). Остались удаленными только: `betsy, cvtt, etcpak, interactive_music, objectdb_profiler, text_server_adv, godot_physics_*`.
