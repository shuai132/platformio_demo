from pathlib import Path
Import("env")
env.Replace(BUILD_SCRIPT=str(Path(env.subst("$PROJECT_DIR")) / "baremetal/esp32c3/builder.py"))
