Import("env")
import subprocess
import datetime

def git(cmd):
    return subprocess.check_output(cmd, shell=True).strip().decode()

try:
    git_hash = git("git describe --always --dirty")
except:
    git_hash = "nogit"

# PHT = datetime.timezone(datetime.timedelta(hours=8))
build_ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S UTC")

env.Append(
    CPPDEFINES=[
        ("GIT_HASH", '\\"%s\\"' % git_hash),
        ("BUILD_TIMESTAMP", '\\"%s\\"' % build_ts),
    ]
)
