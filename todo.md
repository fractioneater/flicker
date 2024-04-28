### TODO IMPORTANT: Fix garbage collector bugs

In `initializeCore()`, with `DEBUG_STRESS_GC` enabled, there's an unwanted "free type 11" that causes a segfault. This is probably one of many issues related to the GC.

At least I can use valgrind to help with this.

### TODO: Be nicer to the user

NEXT: Allow linebreaks after equals signs for function returns.

```kotlin
fun a(arg) =
  "something that should definitely not be a one-liner (okay, yeah, long strings should always be avoided I guess): =(3 + 24 / 3)"
```

There are some things that I really want the user to be able to decide about, but with the way indentation works it's just really hard.

Here's an example of something users should be able to do:
```scala
var letters = [
  "a",
  "b",
  "c",
  "d"
]

var moreLetters = [
  "e", "f",
  "g", "h",
  "i", "j"
]
```

And the there's the problem with long lines.  
This...
```cs
var answerAcceptable = "True" if selected == response and mode == answering and timeRemaining >= 0 else "False"
```
Should be written as this (or something like it):
```cs
var answerAcceptable = "True" if
  selected == response and mode == answering and
    timeRemaining >= 0
  else "False"
```

But so many people do so many different things in cases like that, so with Flicker's indentation it'll be a struggle to let people have what they want.

### TODOs

`-` Not done<br>
`*` Changed my mind (or still haven't decided)<br>
`>` Started, but couldn't finish<br>
`$` Started, will finish later<br>
`%` Done, I think<br>

```
* Add hash functions for things other than strings
  * Support those other things as map keys

- Implement threads/fibers/coroutines (fibers look like the best one)
  - Add something along the lines of time.sleep()

- Some kind of image editing module maybe
- Undefined variables cause compiler error

% Use null tokens to make lex errors more like compile errors.
- Better line number storage (https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter14_chunks/1.md).

> Add else clause to while loop. It turns out to be quite hard, because the only way to evaluate the condition multiple times is jumps.
  I might have to manually go through the code to replicate the process of getting the condition.

    INSTRUCTIONS        ║     IF CONDITION IS TRUTHY                                 ║     IF CONDITION IS FALSY
                        ║                                                            ║
    [ condition ]       ║     [ Truthy ]                                             ║     [ Falsy ]
┌─╼ OP_JUMP_FALSY       ║                                                            ║     [ Falsy ] ╾────┐
│   OP_DUP ───────┐     ║     [ Truthy ] [ Truthy ] ──┐                              ║                    │
│┌╼ OP_JUMP_FALSY │     ║                             │ This will continue           ║                    │
││  OP_POP        │     ║     [ Truthy ]              │ forever because condition    ║                    │
││  [ body ]      │     ║     EXECUTE BODY            │ isn't re-evaluated.          ║                    │
││  OP_LOOP ╾─────┘     ║     [ Truthy ] ╾────────────┘                              ║                    │
│└──OP_POP              ║                                                            ║                    │
│┌╼ OP_JUMP             ║                                                            ║                    │
└┼──OP_POP              ║                                                            ║     [] ────────────┘
 │  [ else ]            ║                                                            ║     EXECUTE ELSE BODY
 ├╼ OP_JUMP             ║                                                            ║     [] ╾─┐
 │  OP_POP              ║                                                            ║          │
 └──[ after ]           ║                                                            ║     [] ──┘
```