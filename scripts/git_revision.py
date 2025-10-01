from subprocess import (
    check_output,
    CalledProcessError, DEVNULL
)

try:
    git_hash = check_output(
        ["git", "rev-parse", "--short=16", "HEAD"],
        stderr=DEVNULL
    ).decode().strip()
    definition = f'"{git_hash}"'
except (FileNotFoundError, CalledProcessError):
    definition = "0"

print(f"const char *GIT_SHORTREV_HASH = {definition};")