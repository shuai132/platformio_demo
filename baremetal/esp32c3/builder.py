from pathlib import Path
import runpy
from SCons.Script import Builder, Default, DefaultEnvironment

env = DefaultEnvironment()
platform = env.PioPlatform()
board = env.BoardConfig()
root = Path(env.subst('$PROJECT_DIR'))
toolchain = platform.get_package_dir('toolchain-riscv32-esp')
esptool = platform.get_package_dir('tool-esptoolpy')
if not toolchain or not esptool:
    raise RuntimeError('Missing ESP32-C3 toolchain or esptool package')
_, esptool_cli = platform.setup_python_env(env)
env.PrependENVPath('PATH', str(Path(toolchain) / 'bin'))
board.update('build.extra_flags', '')
board.update('upload.maximum_ram_size', 262144)
env.Replace(
    AR='riscv32-esp-elf-gcc-ar', AS='riscv32-esp-elf-gcc',
    CC='riscv32-esp-elf-gcc', CXX='riscv32-esp-elf-g++',
    RANLIB='riscv32-esp-elf-gcc-ranlib', SIZETOOL='riscv32-esp-elf-size',
    PROGNAME='firmware', PROGSUFFIX='.elf',
    ASFLAGS=['-march=rv32imc_zicsr_zifencei', '-mabi=ilp32'],
    ASPPFLAGS=['-x', 'assembler-with-cpp'],
    CCFLAGS=['-Os', '-Wall', '-Wextra', '-march=rv32imc_zicsr_zifencei',
             '-mabi=ilp32', '-ffunction-sections', '-fdata-sections',
             '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-msmall-data-limit=0'],
    CXXFLAGS=['-fno-exceptions', '-fno-rtti', '-fno-threadsafe-statics',
              '-fno-use-cxa-atexit', '-include', str(root / 'baremetal/esp32c3/single_thread.hpp')],
    LINKFLAGS=['-march=rv32imc_zicsr_zifencei', '-mabi=ilp32', '-fno-rtti',
               '-nostdlib', '-static', '-Wl,--gc-sections', '-Wl,--no-relax',
               '-Wl,--build-id=none', '-Wl,-Map,$BUILD_DIR/firmware.map'],
    LDSCRIPT_PATH=str(root / 'baremetal/esp32c3/link.ld'),
    LIBS=['stdc++', 'm', 'c', 'gcc'],
    SIZEPROGREGEXP=r'^(?:\.iram0\.text|\.dram0\.data)\s+([0-9]+).*',
    SIZEDATAREGEXP=r'^(?:\.dram0\.(?:data|bss)|\.heap|\.stack)\s+([0-9]+).*',
    SIZECHECKCMD='$SIZETOOL -A -d $SOURCES',
    SIZEPRINTCMD='$SIZETOOL -B -d $SOURCES',
    ELF2BINCMD='"$ESPTOOL" --chip esp32c3 elf2image --flash-mode dio --flash-freq 40m --flash-size 4MB -o "$TARGET" "$SOURCE"',
    ESPTOOL=esptool_cli,
)
env.Append(CPPPATH=[str(root / 'include'), str(root / 'lib/coro/include')])
env.Append(BUILDERS={'ElfToBin': Builder(
    action=env.VerboseAction('$ELF2BINCMD', 'Building $TARGET'), suffix='.bin')})
env.BuildSources('$BUILD_DIR/baremetal', str(root / 'baremetal/esp32c3'),
                 src_filter=['+<runtime.cpp>', '+<startup.S>'])
program = env.BuildProgram()
firmware = env.ElfToBin('$BUILD_DIR/${PROGNAME}', program)
env.Depends(firmware, 'checkprogsize')
checks = runpy.run_path(str(root / 'scripts/verify_baremetal.py'))

def check_elf(source, target, env):
    checks['verify_elf'](str(target[0]))

def check_image(source, target, env):
    checks['verify_image'](str(target[0]))

env.Depends(program, [str(root / 'scripts/verify_baremetal.py'),
                      str(root / 'baremetal/esp32c3/link.ld')])
env.AddPostAction(program, env.VerboseAction(check_elf, 'Verifying bare-metal ELF'))
env.AddPostAction(firmware, env.VerboseAction(check_image, 'Verifying ROM boot image'))

def before_upload(source, target, env):
    if not env.subst('$UPLOAD_PORT'):
        env.AutodetectUploadPort()

env.Replace(
    UPLOAD_SPEED=env.GetProjectOption('upload_speed', board.get('upload.speed', 460800)),
    UPLOADCMD='"$ESPTOOL" --chip esp32c3 --port "$UPLOAD_PORT" --baud $UPLOAD_SPEED --before default-reset --after hard-reset write-flash --flash-mode dio --flash-freq 40m --flash-size 4MB 0x0 "$SOURCE"',
)
env.AddPlatformTarget('upload', firmware, [before_upload,
    env.VerboseAction('$UPLOADCMD', 'Uploading $SOURCE')], 'Upload bare-metal firmware')
env.AddPlatformTarget('size', program,
    env.VerboseAction('$SIZEPRINTCMD', 'Calculating size $SOURCE'), 'Program size')
Default(firmware)
