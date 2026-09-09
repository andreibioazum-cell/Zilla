"""Functions used to generate source files during build time"""

import subprocess
import sys


def generate_android_binaries(target, source, env):
    gradle_process = []

    if sys.platform.startswith("win"):
        gradle_process = [
            "cmd",
            "/c",
            "gradlew.bat",
        ]
    else:
        gradle_process = ["./gradlew"]

    if env["target"] == "editor":
        # Zilla: only android distribution is enabled (horizonos/picoos removed from build.gradle)
        gradle_process += ["generateGodotEditor"]
    else:
        gradle_process += ["generateGodotTemplates"]
    gradle_process += ["--quiet"]

    if env["debug_symbols"] and not env["separate_debug_symbols"]:
        gradle_process += ["-PdoNotStrip=true"]

    result = subprocess.run(
        gradle_process,
        cwd="platform/android/java",
    )
    if result.returncode != 0:
        # Fail SCons build if Gradle failed (previously silent).
        sys.exit(result.returncode)
