# Driveshaft

A smart console for [Flywheel](https://flywheel.polymorphia.dev) in the terminal. Type
Flywheel line by line; each input is compiled by the real compiler and run at once.

```sh
pulley install polymorphia.driveshaft
driveshaft
```

```text
◖◗ Driveshaft 1.0.0 · Flywheel console · run mode: return ends the session
   :help for commands, exit to leave
fw> var x = 4
fw> function sq(int n) returns int
...      return n * n
...  end
fw> sq(x)
16
fw> import os
fw> os.ar▯gs          # a grey suggestion; Right accepts it, Tab completes or lists
```

- **Syntax highlighting** as you type: keywords, types, strings and their `{interpolations}`,
  numbers, comments, calls, and markup tags.
- **Suggestions**: names you declared, keywords and types, module names after `import`, a
  module's functions after `module.` (also through an alias), and earlier inputs. The best one
  is shown in grey after the cursor.
- **Blocks** (`if`, `for`, `function`, `class`, `match`, markup, open brackets) continue on
  `...` lines with the indentation filled in, and run when they are complete. An empty line
  runs an unfinished input as it is, to see the compiler's message.
- An **expression** on its own is printed. Errors are reported with line numbers counted from
  the input's first line.
- `input()` works: what you type is remembered and replayed, so you are asked once.
- **Ctrl-C** cancels the line, or stops the running program and keeps the session.
- History is kept in `~/.driveshaft_history`.

## Modes and `return`

| Command | A top-level `return` |
|---|---|
| `driveshaft` | ends the session, as it ends a program: `return 2` exits with status 2 |
| `driveshaft serve` | `return <markup>` is served at `http://127.0.0.1:8000/` (`--port`, `--host`); the open page reloads on every later `return`. Other values are printed (`=> [1,2]`) |
| `driveshaft desktop` | `return <markup>` opens it in a window, with working event handlers. Other values are printed |

In `serve` and `desktop` mode `return` never ends the session; `exit` (or `quit`, Ctrl-D)
does, in every mode. `driveshaft FILE.fw` runs a file first and keeps what it declares.

```text
$ driveshaft serve
fw> import html.*
fw> var name = "Ann"
fw> return <html>
...      <body><h1>Hello, {name}</h1></body>
...  </html>
serving at http://127.0.0.1:8000/ (the page reloads on each return)
fw> name = "Bo"
fw> return <h1>Hello, {name}</h1>
page updated: http://127.0.0.1:8000/
```

## Commands

| | |
|---|---|
| `exit`, `quit` | leave |
| `:help` | the commands and keys |
| `:show` | the session so far |
| `:undo` | forget the last statement (or declaration) |
| `:reset` | forget everything |
| `:load FILE`, `:save FILE` | run a file into the session; write the session to a file |
| `:mode [run\|serve\|desktop]` | show or change the mode |

Keys: Tab, Right/End (accept the suggestion), Up/Down, Home/End, Ctrl-A/E/K/U/W, Ctrl-Left and
Ctrl-Right, Ctrl-L (clear), Ctrl-C, Ctrl-D.

## How it runs

Flywheel is compiled, so Driveshaft keeps the session as a program: declarations first, then
the statements that ran without error. Each input is appended, the program is compiled and run
again (a few tens of milliseconds), and only the output of the new input is shown. Inputs that
only read values (`x + 1`, `print total`) are not kept. Because earlier statements run again,
their side effects repeat: a statement that appends to a file appends again on every input.
`:undo` and `:reset` take statements back out.

## As a library

`pulley add polymorphia.driveshaft`, then:

```flywheel
import driveshaft

var session = driveshaft.Session()            # Session("serve") for the page modes
session.eval("var x = 4")
var outcome = session.eval("x * 2")
print outcome.status, outcome.output          # 0 8
print session.eval("print nope").errors       # line 1:7: error: undeclared identifier ...
session.close()

print driveshaft.highlight("print x * 2")              # ANSI colors
print driveshaft.complete("os.ar", "import os")        # ["args"]
print driveshaft.incomplete("if x > 1 do")             # true
```

| | |
|---|---|
| `Session(mode = "run", streaming = false)` | a session; `eval(code)` returns an `Outcome` |
| `Outcome` | `status`, `output`, `errors`, `kept`, `unfinished`, `returned` (`""`, `"exit"`, `"page"`, `"value"`), `value`, `seconds` |
| `session.check(code)`, `load(text)`, `source()`, `undo()`, `reset()`, `close()` | |
| `session.stdin`, `timeout`, `compiler` | lines for `input()`, a time limit, and the compiler to use |
| `tokenize`, `highlight`, `palette`, `colors` | tokens and ANSI colors (`palette["keyword"] = "1;35"`) |
| `analyze`, `incomplete`, `symbols`, `complete`, `typed` | structure and names of a piece of code |

The compiler is `DRIVESHAFT_FLYWHEEL` or `FLYWHEEL_COMPILER` when set, otherwise the one
running the program, otherwise `flywheel` on `PATH`.

## Requirements

A Flywheel with `os.start` (releases after 2.17). The console reads keys through a small C file
(`term.c`, a [native import](https://flywheel.polymorphia.dev)), so it needs the C++ compiler
Flywheel uses for native imports; the library (`import driveshaft`) does not. Live output
while a program runs uses `stdbuf` when it is installed. Linux and macOS; on Windows the
console runs without live output and Ctrl-C handling (untested).

## Development

```sh
flywheel test tests/
```

MIT license.
