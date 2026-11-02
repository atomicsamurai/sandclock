# PlatformIO pre-build script: embed `git describe` as GIT_REV so /info shows
# exactly which commit (and whether uncommitted changes) is running.
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO)


def git_rev():
    try:
        return subprocess.check_output(
            ["git", "describe", "--always", "--dirty", "--abbrev=7"],
            cwd=env["PROJECT_DIR"],  # noqa: F821
            stderr=subprocess.DEVNULL,
        ).decode().strip()
    except Exception:
        return "unknown"


env.Append(CPPDEFINES=[("GIT_REV", env.StringifyMacro(git_rev()))])  # noqa: F821
