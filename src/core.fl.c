// Automatically generated file. Do not edit.
static const char* coreSource __attribute__((unused)) =
"class Bool {}\n"
"class Function {}\n"
"class `None` {}\n"
"class Number {}\n"
"\n"
"class Sequence\n"
"  all(function)\n"
"    var result = True\n"
"    each (element in this)\n"
"      result = function(element)\n"
"      if (not result) return False\n"
"    return True\n"
"  \n"
"  any(function)\n"
"    var result = False\n"
"    each (element in this)\n"
"      result = function(element)\n"
"      if (result) return True\n"
"    return False\n"
"  \n"
"  contains(item)\n"
"    each (element in this)\n"
"      if (element == item) return True\n"
"    return False\n"
"  \n"
"  count(function)\n"
"    var result = 0\n"
"    each (element in this)\n"
"      if (function(element)) result = result + 1\n"
"    return result\n"
"  \n"
"  forEach(function)\n"
"    each (element in this)\n"
"      function(element)\n"
"  \n"
"  attribute isEmpty = if (this.iterate(None)) False else True\n"
"  \n"
"  map(transformation) = MapSequence(this, transformation)\n"
"  \n"
"  skip(count)\n"
"    if ((not count is Number) or (not count.isInteger) or (count < 0))\n"
"      return\n"
"    \n"
"    return SkipSequence(this, count)\n"
"  \n"
"  take(count)\n"
"    if ((not count is Number) or (not count.isInteger) or (count < 0))\n"
"      return\n"
"    \n"
"    return TakeSequence(this, count)\n"
"  \n"
"  where(predicate) = WhereSequence(this, predicate)\n"
"  \n"
"  reduce(acc, function)\n"
"    each (element in this)\n"
"      acc = function(acc, element)\n"
"    return acc\n"
"  \n"
"  reduce(function)\n"
"    var iter = this.iterate(None)\n"
"    if (not iter) return\n"
"\n"
"    var result = this.iteratorValue(iter)\n"
"    while (iter = this.iterate(iter))\n"
"      result = function(result, this.iteratorValue(iter))\n"
"    \n"
"    return result\n"
"  \n"
"  join() = this.join(\", \")\n"
"  \n"
"  join(sep)\n"
"    var result = \"\"\n"
"\n"
"    for (var i = 0; i < this.size; i = i + 1)\n"
"      if (i != 0) result = result + sep\n"
"      result = result + this[i].toString()\n"
"    \n"
"    return result\n"
"  \n"
"  toList()\n"
"    var result = List()\n"
"    each (element in this)\n"
"      result.add(element)\n"
"    return result\n"
"\n"
"class MapSequence < Sequence\n"
"  init(+sequence, +function)\n"
"    pass\n"
"\n"
"  iterate(iterator) = this.sequence.iterate(iterator)\n"
"  \n"
"  iteratorValue(iterator)\n"
"    return this.function(this.sequence.iteratorValue(iterator))\n"
"\n"
"class SkipSequence < Sequence\n"
"  init(+sequence, +count)\n"
"    pass\n"
"  \n"
"  iterate(iterator)\n"
"    if (iterator)\n"
"      return this.sequence.iterate(iterator)\n"
"    else\n"
"      iterator = this.sequence.iterate(iterator)\n"
"      for (var count = this.count; count > 0 and iterator; count = count - 1)\n"
"        iterator = this.sequence.iterate(iterator)\n"
"      return iterator\n"
"\n"
"  iteratorValue(iterator) = this.sequence.iteratorValue(iterator)\n"
"\n"
"class TakeSequence < Sequence\n"
"  init(+sequence, +count)\n"
"    pass\n"
"  \n"
"  iterate(iterator)\n"
"    if (not iterator) this.taken = 1 else this.taken = this.taken + 1\n"
"    return if (this.taken > this.count) None else this.sequence.iterate(iterator)\n"
"  \n"
"  iteratorValue(iterator) = this.sequence.iteratorValue(iterator)\n"
"\n"
"class WhereSequence < Sequence\n"
"  init(+sequence, +function)\n"
"    pass\n"
"  \n"
"  iterate(iterator)\n"
"    while (iterator = this.sequence.iterate(iterator))\n"
"      if (this.function(this.sequence.iteratorValue(iterator))) break\n"
"    return iterator\n"
"  \n"
"  iteratorValue(iterator)\n"
"    return this.sequence.iteratorValue(iterator)\n"
"\n"
"class String < Sequence\n"
"  attribute bytes = StringByteSequence(this)\n"
"  attribute codePoints = StringCodePointSequence(this)\n"
"  \n"
"  +(other) = this.concatenate(other.toString())\n"
"  \n"
"  *(count)\n"
"    if ((not count is Num) or (not count.isInteger) or count < 0)\n"
"      return\n"
"    \n"
"    var result = \"\"\n"
"    each (i in 0:count)\n"
"      result = result + this\n"
"    return result\n"
"  \n"
"  split(delimiter)\n"
"    if ((not delimiter is String) or delimiter.isEmpty)\n"
"      return\n"
"    \n"
"    var result = []\n"
"\n"
"    var last = 0\n"
"    var index = 0\n"
"    \n"
"    var delimiterSize = delimiter.byteCount\n"
"    var size = this.byteCount\n"
"\n"
"    while (last < size and (index = indexOf(delimiter, last)) != -1)\n"
"      result.add(this[last:index])\n"
"      last = index + delimiterSize\n"
"    \n"
"    if (last < size)\n"
"      result.add(this[last..-1])\n"
"    else\n"
"      result.add(\"\")\n"
"    \n"
"    return result\n"
"  \n"
"  replace(from, to)\n"
"    if ((not from is String) or from.isEmpty or (not to is String))\n"
"      return    \n"
"\n"
"    var result = \"\"\n"
"\n"
"    var last = 0\n"
"    var index = 0\n"
"\n"
"    var fromSize = from.byteCount\n"
"    var size = this.byteCount\n"
"\n"
"    while (last < size and (index = indexOf(from, last)) != -1)\n"
"      result = result + this[last:index] + to\n"
"      last = index + fromSize\n"
"    \n"
"    if (last < size) result = result + this[last..-1]\n"
"\n"
"    return result\n"
"  \n"
"  trim() = this.trim_(\"\\t\\r\\n \", True, True)\n"
"  trim(chars) = this.trim_(chars, True, True)\n"
"  trimEnd() = this.trim_(\"\\t\\r\\n \", False, True)\n"
"  trimEnd(chars) = this.trim_(chars, False, True)\n"
"  trimStart() = this.trim_(\"\\t\\r\\n \", True, False)\n"
"  trimStart(chars) = this.trim_(chars, True, False)\n"
"  \n"
"  trim_(chars, trimStart, trimEnd)\n"
"    if (not chars is String)\n"
"      return\n"
"    \n"
"    var codePoints = chars.codePoints.toList()\n"
"\n"
"    var start\n"
"    if (trimStart)\n"
"      while (start = iterate(start))\n"
"        if (not codePoints.contains(this.codePointAt(start))) break\n"
"      \n"
"      if (start == False) return \"\"\n"
"    else start = 0\n"
"    \n"
"    var end\n"
"    if (trimEnd)\n"
"      end = this.byteCount - 1\n"
"      while (end >= start)\n"
"        var codePoint = this.codePointAt(end)\n"
"        if (codePoint == -1 and not codePoints.contains(codePoint)) break\n"
"        end = end - 1\n"
"      \n"
"      if (end < start) return \"\"\n"
"    else end = -1\n"
"\n"
"    return this[start..end]\n"
"\n"
"class StringByteSequence < Sequence\n"
"  init(+string)\n"
"    pass\n"
"  \n"
"  attribute count = this.string.byteCount\n"
"\n"
"  get(index) = this.string.byteAt(index)\n"
"  iterate(iterator) = this.string.iterateByte(iterator)\n"
"  iteratorValue(iterator) = this.string.byteAt(iterator)\n"
"\n"
"class StringCodePointSequence < Sequence\n"
"  init(+string)\n"
"    pass\n"
"\n"
"  attribute count = this.string.count\n"
"  \n"
"  get(index) = this.string.codePointAt(index)\n"
"  iterate(iterator) = this.string.iterate(iterator)\n"
"  iteratorValue(iterator) = this.string.codePointAt(iterator)\n"
"\n"
"class List < Sequence\n"
"  +(other)\n"
"    var result = this[0..-1]\n"
"    each (element in other)\n"
"      result.add(element)\n"
"    return result\n"
"  \n"
"  *(count)\n"
"    if ((not count is Number) or (not count.isInteger) or count < 0)\n"
"      return\n"
"    \n"
"    var result = []\n"
"    each (i in 0:count)\n"
"      result.addAll(this)\n"
"    return result\n"
"  \n"
"  addAll(other)\n"
"    each (element in other)\n"
"      this.add(element)\n"
"\n"
"  sort() = this.sort { |low, high| low < high }\n"
"\n"
"  sort(comparer)\n"
"    if (not comparer is Function)\n"
"      return\n"
"    this.quicksort(0, this.size - 1, comparer)\n"
"\n"
"  quicksort(low, high, comparer)\n"
"    if (low < high)\n"
"      var p = this.partition(low, high, comparer)\n"
"      quicksort(low, p - 1, comparer)\n"
"      quicksort(p + 1, high, comparer)\n"
"  \n"
"  partition(low, high, comparer)\n"
"    var p = this[high]\n"
"    var i = low - 1\n"
"    each (j in low..(high - 1))\n"
"      if (comparer(this[j], p))\n"
"        i = i + 1\n"
"        var t = this[i]\n"
"        this[i] = this[j]\n"
"        this[j] = t\n"
"    var t = this[i + 1]\n"
"    this[i + 1] = this[high]\n"
"    this[high] = t\n"
"    return i + 1\n"
"  \n"
"  toString() = \"[=(this.join())]\"\n"
"\n"
"class Range < Sequence {}\n"
"\n"
"class Sys\n"
"  static `print`()\n"
"    Sys.writeString(\"\\n\")\n"
"  \n"
"  static `print`(obj)\n"
"    Sys.writeObject(obj)\n"
"    Sys.writeString(\"\\n\")\n"
"    return obj\n"
"  \n"
"  static printAll(sequence)\n"
"    each (object in sequence) Sys.writeObject(object)\n"
"    Sys.writeString(\"\\n\")\n"
"  \n"
"  static write(obj)\n"
"    Sys.writeObject(obj)\n"
"    return obj\n"
"\n"
"  static writeAll(sequence)\n"
"    each(object in sequence) Sys.writeObject(object)\n"
"\n"
"  static writeObject(obj)\n"
"    var string = obj.toString()\n"
"    if (string is String)\n"
"      Sys.writeString(string)\n"
"    else\n"
"      Sys.writeString(\"[invalid toString()]\")\n";
