#!/usr/bin/env python3
# Regenerates one shaders/Compiled/*.c from its .spv after a glslangValidator/glslc recompile. Not part of the build.
import sys

def convert(spv_path, name):
    with open(spv_path, "rb") as f:
        data = f.read()
    lines = []
    lines.append("// Precompiled SPIR-V for shaders/%s." % name.replace("_vert", ".vert").replace("_frag", ".frag"))
    lines.append("unsigned char %s_spv[] = {" % name)
    for i in range(0, len(data), 10):
        chunk = data[i:i+10]
        lines.append("".join("0x%02X, " % b for b in chunk))
    lines.append("};")
    lines.append("int %s_spv_size = %d;" % (name, len(data)))
    return "\n".join(lines) + "\n"

if __name__ == "__main__":
    spv_path, name, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(out_path, "w", newline="\n") as f:
        f.write(convert(spv_path, name))
