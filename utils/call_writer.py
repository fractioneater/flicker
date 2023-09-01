import sys

TYPES = {"CALL": "byte", "INVOKE": "invoke", "SUPER": "invoke"}

OUTPUT ="""case OP_{0}_{2}:
  return {1}Instruction("OP_{0}_{2}", chunk, offset);
"""

def to_c_string(name, instruction):
  source = ""
  for i in range(17):
    source += OUTPUT.format(name, instruction, i)
  
  return source

source = ""
for i in TYPES:
  source += to_c_string(i, TYPES[i])

with open(sys.argv[1], "w") as out:
  out.write(source)
