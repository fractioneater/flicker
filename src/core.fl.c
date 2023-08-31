// Automatically generated file. Do not edit.
static const char* coreSource __attribute__((unused)) =
"class Bool {}\n"
"class Function {}\n"
"class `None` {}\n"
"class Number {}\n"
"class String {}\n"
"class List {}\n"
"class Range {}\n"
"class Sys\n"
"  static writeObject(obj)\n"
"    var string = obj.toString \n"
"    if (string is String)\n"
"      writeString(string)\n"
"    else\n"
"      writeString(\"[invalid toString method]\")\n";
