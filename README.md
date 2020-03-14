<div style="display:flex;justify-content:center;align-items:center">
<img src="./logo.svg" width="200px">

# WT\* framework
</div>

This is the public source repository of the WT\* framework.
The project page is [https://beda.dcs.fmph.uniba.sk/wtstar](https://beda.dcs.fmph.uniba.sk/wtstar). The Doxygen-generated source
documentation is [https://beda.dcs.fmph.uniba.sk/wtstar-doc](https://beda.dcs.fmph.uniba.sk/wtstar-doc). Current version is RC 1.1.

## Building from the source

To build the cli tools, a C compiler, `flex`, and `bison` are needed. Edit the first 
few lines in `src/Makefile` to suit your paths, and just `make`. 

The code should build without problems on Linux, FreeBSD, and similar. There are many
unaligned memory read/writes in the code, so probably compiling on, e.g. ARM would be 
tricky. 

To build the documentation, you need Doxygen installed, and run `make documentation` 
from `src`.

The web-based IDE is located in `web/ide`. It needs [nodejs](https://nodejs.org) and [emscripten](https://emscripten.org).
First run `npm install` from `web/ide`
to install dependencies. Edit `Makefile` to suit your paths
and `make`. Make creates directory `_build/web` with files `backend.js`, `backend.wasm`,
`backend.wasm.map`, `ide.css`, and `ide.js`. Copy the content of `web/static` there, too.

In a web page, import the `ide.css` and `ide.js`. The IDE uses [w2ui](http://w2ui.com)
and [ace editor](http://ace.c9.io), so you probably want something like this in
your markup

```
<link rel="stylesheet" href="https://fonts.googleapis.com/css?family=Montserrat:300,400">
<link href="https://fonts.googleapis.com/css?family=Roboto+Mono" rel="stylesheet">

<link rel="stylesheet" type="text/css" href="https://cdnjs.cloudflare.com/ajax/libs/w2ui/1.4.3/w2ui.min.css" />
<script src="https://ajax.googleapis.com/ajax/libs/jquery/2.1.1/jquery.min.js"></script>
<script type="text/javascript" src="https://cdnjs.cloudflare.com/ajax/libs/w2ui/1.4.3/w2ui.min.js"></script>
<script type="text/javascript" src="https://cdnjs.cloudflare.com/ajax/libs/ace/1.4.5/ace.js"></script>
```

The scripr on load expects the following markup:
```
<div id="app-layout"><xmp id="editor-data">initial stuff here</xmp></div>
```
and places the whole IDE into `app-layout`.

To make the file `web/static/mode-wtstar.js` which contains the highlighting rules for the
ace editor, use the files in `web/resources`, and follow the instructions in the source
repository of ace. 

## TODO

- more portable build system
- cleanup code
- finish documentation
- add documentation to `web/ide`
- make `#include` directive work in web IDE
- improve the debugger (bot the cli, and the web-based)

## CHANGELOG

### master branch
- bug fix in treating windows cr/lf endlines

### RC 1.1

- bug fix in reading multiple input arrays
- added implementation of operators '|', '&', '~'
- bug fix in operator '~|'
