import os.path
import sys

OUTPUT = """// Automatically generated file. Do not edit.
static const char* {0}Source __attribute__((unused)) =
{1};
"""

def to_c_string(source_lines, module):
  source = ""
  for line in source_lines:
    line = line.replace("\\", "\\\\")
    line = line.replace('"', "\\\"")
    line = line.replace("\n", "\\n\"")
    if source: source += "\n"
    source += '"' + line
  
  return OUTPUT.format(module, source)

def main():
  with open(sys.argv[1], "r") as file:
    source_lines = file.readlines()

  module = os.path.splitext(os.path.basename(sys.argv[1]))[0]

  source = to_c_string(source_lines, module)

  with open(sys.argv[2], "w") as out:
    out.write(source)

main()