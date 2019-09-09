(function () {
  'use strict';

  var Module = (function() {
    var _scriptDir = typeof document !== 'undefined' && document.currentScript ? document.currentScript.src : undefined;
    return (
  function(Module) {
    Module = Module || {};

  // Copyright 2010 The Emscripten Authors.  All rights reserved.
  // Emscripten is available under two separate licenses, the MIT license and the
  // University of Illinois/NCSA Open Source License.  Both these licenses can be
  // found in the LICENSE file.

  // The Module object: Our interface to the outside world. We import
  // and export values on it. There are various ways Module can be used:
  // 1. Not defined. We create it here
  // 2. A function parameter, function(Module) { ..generated code.. }
  // 3. pre-run appended it, var Module = {}; ..generated code..
  // 4. External script tag defines var Module.
  // We need to check if Module already exists (e.g. case 3 above).
  // Substitution will be replaced with actual code on later stage of the build,
  // this way Closure Compiler will not mangle it (e.g. case 4. above).
  // Note that if you want to run closure, and also to use Module
  // after the generated code, you will need to define   var Module = {};
  // before the code. Then that object will be used in the code, and you
  // can continue to use Module afterwards as well.
  var Module = typeof Module !== 'undefined' ? Module : {};

  // --pre-jses are emitted after the Module integration code, so that they can
  // refer to Module (if they choose; they can also define Module)
  // {{PRE_JSES}}

  // Sometimes an existing Module object exists with properties
  // meant to overwrite the default module functionality. Here
  // we collect those properties and reapply _after_ we configure
  // the current environment's defaults to avoid having to be so
  // defensive during initialization.
  var moduleOverrides = {};
  var key;
  for (key in Module) {
    if (Module.hasOwnProperty(key)) {
      moduleOverrides[key] = Module[key];
    }
  }

  Module['arguments'] = [];
  Module['thisProgram'] = './this.program';
  Module['quit'] = function(status, toThrow) {
    throw toThrow;
  };
  Module['preRun'] = [];
  Module['postRun'] = [];

  // Determine the runtime environment we are in. You can customize this by
  // setting the ENVIRONMENT setting at compile time (see settings.js).

  var ENVIRONMENT_IS_WEB = false;
  var ENVIRONMENT_IS_WORKER = false;
  var ENVIRONMENT_IS_NODE = false;
  var ENVIRONMENT_IS_SHELL = false;
  ENVIRONMENT_IS_WEB = typeof window === 'object';
  ENVIRONMENT_IS_WORKER = typeof importScripts === 'function';
  ENVIRONMENT_IS_NODE = typeof process === 'object' && typeof require === 'function' && !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_WORKER;
  ENVIRONMENT_IS_SHELL = !ENVIRONMENT_IS_WEB && !ENVIRONMENT_IS_NODE && !ENVIRONMENT_IS_WORKER;

  if (Module['ENVIRONMENT']) {
    throw new Error('Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -s ENVIRONMENT=web or -s ENVIRONMENT=node)');
  }


  // Three configurations we can be running in:
  // 1) We could be the application main() thread running in the main JS UI thread. (ENVIRONMENT_IS_WORKER == false and ENVIRONMENT_IS_PTHREAD == false)
  // 2) We could be the application main() thread proxied to worker. (with Emscripten -s PROXY_TO_WORKER=1) (ENVIRONMENT_IS_WORKER == true, ENVIRONMENT_IS_PTHREAD == false)
  // 3) We could be an application pthread running in a worker. (ENVIRONMENT_IS_WORKER == true and ENVIRONMENT_IS_PTHREAD == true)




  // `/` should be present at the end if `scriptDirectory` is not empty
  var scriptDirectory = '';
  function locateFile(path) {
    if (Module['locateFile']) {
      return Module['locateFile'](path, scriptDirectory);
    } else {
      return scriptDirectory + path;
    }
  }

  if (ENVIRONMENT_IS_NODE) {
    scriptDirectory = __dirname + '/';

    // Expose functionality in the same simple way that the shells work
    // Note that we pollute the global namespace here, otherwise we break in node
    var nodeFS;
    var nodePath;

    Module['read'] = function shell_read(filename, binary) {
      var ret;
        if (!nodeFS) nodeFS = require('fs');
        if (!nodePath) nodePath = require('path');
        filename = nodePath['normalize'](filename);
        ret = nodeFS['readFileSync'](filename);
      return binary ? ret : ret.toString();
    };

    Module['readBinary'] = function readBinary(filename) {
      var ret = Module['read'](filename, true);
      if (!ret.buffer) {
        ret = new Uint8Array(ret);
      }
      assert(ret.buffer);
      return ret;
    };

    if (process['argv'].length > 1) {
      Module['thisProgram'] = process['argv'][1].replace(/\\/g, '/');
    }

    Module['arguments'] = process['argv'].slice(2);

    // MODULARIZE will export the module in the proper place outside, we don't need to export here

    process['on']('uncaughtException', function(ex) {
      // suppress ExitStatus exceptions from showing an error
      if (!(ex instanceof ExitStatus)) {
        throw ex;
      }
    });
    // Currently node will swallow unhandled rejections, but this behavior is
    // deprecated, and in the future it will exit with error status.
    process['on']('unhandledRejection', abort);

    Module['quit'] = function(status) {
      process['exit'](status);
    };

    Module['inspect'] = function () { return '[Emscripten Module object]'; };
  } else
  if (ENVIRONMENT_IS_SHELL) {


    if (typeof read != 'undefined') {
      Module['read'] = function shell_read(f) {
        return read(f);
      };
    }

    Module['readBinary'] = function readBinary(f) {
      var data;
      if (typeof readbuffer === 'function') {
        return new Uint8Array(readbuffer(f));
      }
      data = read(f, 'binary');
      assert(typeof data === 'object');
      return data;
    };

    if (typeof scriptArgs != 'undefined') {
      Module['arguments'] = scriptArgs;
    } else if (typeof arguments != 'undefined') {
      Module['arguments'] = arguments;
    }

    if (typeof quit === 'function') {
      Module['quit'] = function(status) {
        quit(status);
      };
    }
  } else
  if (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) {
    if (ENVIRONMENT_IS_WORKER) { // Check worker, not web, since window could be polyfilled
      scriptDirectory = self.location.href;
    } else if (document.currentScript) { // web
      scriptDirectory = document.currentScript.src;
    }
    // When MODULARIZE (and not _INSTANCE), this JS may be executed later, after document.currentScript
    // is gone, so we saved it, and we use it here instead of any other info.
    if (_scriptDir) {
      scriptDirectory = _scriptDir;
    }
    // blob urls look like blob:http://site.com/etc/etc and we cannot infer anything from them.
    // otherwise, slice off the final part of the url to find the script directory.
    // if scriptDirectory does not contain a slash, lastIndexOf will return -1,
    // and scriptDirectory will correctly be replaced with an empty string.
    if (scriptDirectory.indexOf('blob:') !== 0) {
      scriptDirectory = scriptDirectory.substr(0, scriptDirectory.lastIndexOf('/')+1);
    } else {
      scriptDirectory = '';
    }


    Module['read'] = function shell_read(url) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', url, false);
        xhr.send(null);
        return xhr.responseText;
    };

    if (ENVIRONMENT_IS_WORKER) {
      Module['readBinary'] = function readBinary(url) {
          var xhr = new XMLHttpRequest();
          xhr.open('GET', url, false);
          xhr.responseType = 'arraybuffer';
          xhr.send(null);
          return new Uint8Array(xhr.response);
      };
    }

    Module['readAsync'] = function readAsync(url, onload, onerror) {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', url, true);
      xhr.responseType = 'arraybuffer';
      xhr.onload = function xhr_onload() {
        if (xhr.status == 200 || (xhr.status == 0 && xhr.response)) { // file URLs can return 0
          onload(xhr.response);
          return;
        }
        onerror();
      };
      xhr.onerror = onerror;
      xhr.send(null);
    };

    Module['setWindowTitle'] = function(title) { document.title = title; };
  } else
  {
    throw new Error('environment detection error');
  }

  // Set up the out() and err() hooks, which are how we can print to stdout or
  // stderr, respectively.
  // If the user provided Module.print or printErr, use that. Otherwise,
  // console.log is checked first, as 'print' on the web will open a print dialogue
  // printErr is preferable to console.warn (works better in shells)
  // bind(console) is necessary to fix IE/Edge closed dev tools panel behavior.
  var out = Module['print'] || (typeof console !== 'undefined' ? console.log.bind(console) : (typeof print !== 'undefined' ? print : null));
  var err = Module['printErr'] || (typeof printErr !== 'undefined' ? printErr : ((typeof console !== 'undefined' && console.warn.bind(console)) || out));

  // Merge back in the overrides
  for (key in moduleOverrides) {
    if (moduleOverrides.hasOwnProperty(key)) {
      Module[key] = moduleOverrides[key];
    }
  }
  // Free the object hierarchy contained in the overrides, this lets the GC
  // reclaim data used e.g. in memoryInitializerRequest, which is a large typed array.
  moduleOverrides = undefined;

  // perform assertions in shell.js after we set up out() and err(), as otherwise if an assertion fails it cannot print the message
  assert(typeof Module['memoryInitializerPrefixURL'] === 'undefined', 'Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead');
  assert(typeof Module['pthreadMainPrefixURL'] === 'undefined', 'Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead');
  assert(typeof Module['cdInitializerPrefixURL'] === 'undefined', 'Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead');
  assert(typeof Module['filePackagePrefixURL'] === 'undefined', 'Module.filePackagePrefixURL option was removed, use Module.locateFile instead');

  // stack management, and other functionality that is provided by the compiled code,
  // should not be used before it is ready
  stackSave = stackRestore = stackAlloc = function() {
    abort('cannot use the stack before compiled code is ready to run, and has provided stack access');
  };

  function warnOnce(text) {
    if (!warnOnce.shown) warnOnce.shown = {};
    if (!warnOnce.shown[text]) {
      warnOnce.shown[text] = 1;
      err(text);
    }
  }

  var asm2wasmImports = { // special asm2wasm imports
      "f64-rem": function(x, y) {
          return x % y;
      },
      "debugger": function() {
          debugger;
      }
  };

  var tempRet0 = 0;

  var setTempRet0 = function(value) {
    tempRet0 = value;
  };

  var getTempRet0 = function() {
    return tempRet0;
  };




  // === Preamble library stuff ===

  // Documentation for the public APIs defined in this file must be updated in:
  //    site/source/docs/api_reference/preamble.js.rst
  // A prebuilt local version of the documentation is available at:
  //    site/build/text/docs/api_reference/preamble.js.txt
  // You can also build docs locally as HTML or other formats in site/
  // An online HTML version (which may be of a different version of Emscripten)
  //    is up at http://kripken.github.io/emscripten-site/docs/api_reference/preamble.js.html


  if (typeof WebAssembly !== 'object') {
    abort('No WebAssembly support found. Build with -s WASM=0 to target JavaScript instead.');
  }




  // Wasm globals

  var wasmMemory;

  // Potentially used for direct table calls.
  var wasmTable;


  //========================================
  // Runtime essentials
  //========================================

  // whether we are quitting the application. no code should run after this.
  // set in exit() and abort()
  var ABORT = false;

  /** @type {function(*, string=)} */
  function assert(condition, text) {
    if (!condition) {
      abort('Assertion failed: ' + text);
    }
  }

  // Returns the C function with a specified identifier (for C++, you need to do manual name mangling)
  function getCFunc(ident) {
    var func = Module['_' + ident]; // closure exported function
    assert(func, 'Cannot call unknown function ' + ident + ', make sure it is exported');
    return func;
  }

  // C calling interface.
  function ccall(ident, returnType, argTypes, args, opts) {
    // For fast lookup of conversion functions
    var toC = {
      'string': function(str) {
        var ret = 0;
        if (str !== null && str !== undefined && str !== 0) { // null string
          // at most 4 bytes per UTF-8 code point, +1 for the trailing '\0'
          var len = (str.length << 2) + 1;
          ret = stackAlloc(len);
          stringToUTF8(str, ret, len);
        }
        return ret;
      },
      'array': function(arr) {
        var ret = stackAlloc(arr.length);
        writeArrayToMemory(arr, ret);
        return ret;
      }
    };

    function convertReturnValue(ret) {
      if (returnType === 'string') return UTF8ToString(ret);
      if (returnType === 'boolean') return Boolean(ret);
      return ret;
    }

    var func = getCFunc(ident);
    var cArgs = [];
    var stack = 0;
    assert(returnType !== 'array', 'Return type should not be "array".');
    if (args) {
      for (var i = 0; i < args.length; i++) {
        var converter = toC[argTypes[i]];
        if (converter) {
          if (stack === 0) stack = stackSave();
          cArgs[i] = converter(args[i]);
        } else {
          cArgs[i] = args[i];
        }
      }
    }
    var ret = func.apply(null, cArgs);
    ret = convertReturnValue(ret);
    if (stack !== 0) stackRestore(stack);
    return ret;
  }

  function cwrap(ident, returnType, argTypes, opts) {
    return function() {
      return ccall(ident, returnType, argTypes, arguments);
    }
  }


  // Given a pointer 'ptr' to a null-terminated UTF8-encoded string in the given array that contains uint8 values, returns
  // a copy of that string as a Javascript String object.

  var UTF8Decoder = typeof TextDecoder !== 'undefined' ? new TextDecoder('utf8') : undefined;

  /**
   * @param {number} idx
   * @param {number=} maxBytesToRead
   * @return {string}
   */
  function UTF8ArrayToString(u8Array, idx, maxBytesToRead) {
    var endIdx = idx + maxBytesToRead;
    var endPtr = idx;
    // TextDecoder needs to know the byte length in advance, it doesn't stop on null terminator by itself.
    // Also, use the length info to avoid running tiny strings through TextDecoder, since .subarray() allocates garbage.
    // (As a tiny code save trick, compare endPtr against endIdx using a negation, so that undefined means Infinity)
    while (u8Array[endPtr] && !(endPtr >= endIdx)) ++endPtr;

    if (endPtr - idx > 16 && u8Array.subarray && UTF8Decoder) {
      return UTF8Decoder.decode(u8Array.subarray(idx, endPtr));
    } else {
      var str = '';
      // If building with TextDecoder, we have already computed the string length above, so test loop end condition against that
      while (idx < endPtr) {
        // For UTF8 byte structure, see:
        // http://en.wikipedia.org/wiki/UTF-8#Description
        // https://www.ietf.org/rfc/rfc2279.txt
        // https://tools.ietf.org/html/rfc3629
        var u0 = u8Array[idx++];
        if (!(u0 & 0x80)) { str += String.fromCharCode(u0); continue; }
        var u1 = u8Array[idx++] & 63;
        if ((u0 & 0xE0) == 0xC0) { str += String.fromCharCode(((u0 & 31) << 6) | u1); continue; }
        var u2 = u8Array[idx++] & 63;
        if ((u0 & 0xF0) == 0xE0) {
          u0 = ((u0 & 15) << 12) | (u1 << 6) | u2;
        } else {
          if ((u0 & 0xF8) != 0xF0) warnOnce('Invalid UTF-8 leading byte 0x' + u0.toString(16) + ' encountered when deserializing a UTF-8 string on the asm.js/wasm heap to a JS string!');
          u0 = ((u0 & 7) << 18) | (u1 << 12) | (u2 << 6) | (u8Array[idx++] & 63);
        }

        if (u0 < 0x10000) {
          str += String.fromCharCode(u0);
        } else {
          var ch = u0 - 0x10000;
          str += String.fromCharCode(0xD800 | (ch >> 10), 0xDC00 | (ch & 0x3FF));
        }
      }
    }
    return str;
  }

  // Given a pointer 'ptr' to a null-terminated UTF8-encoded string in the emscripten HEAP, returns a
  // copy of that string as a Javascript String object.
  // maxBytesToRead: an optional length that specifies the maximum number of bytes to read. You can omit
  //                 this parameter to scan the string until the first \0 byte. If maxBytesToRead is
  //                 passed, and the string at [ptr, ptr+maxBytesToReadr[ contains a null byte in the
  //                 middle, then the string will cut short at that byte index (i.e. maxBytesToRead will
  //                 not produce a string of exact length [ptr, ptr+maxBytesToRead[)
  //                 N.B. mixing frequent uses of UTF8ToString() with and without maxBytesToRead may
  //                 throw JS JIT optimizations off, so it is worth to consider consistently using one
  //                 style or the other.
  /**
   * @param {number} ptr
   * @param {number=} maxBytesToRead
   * @return {string}
   */
  function UTF8ToString(ptr, maxBytesToRead) {
    return ptr ? UTF8ArrayToString(HEAPU8, ptr, maxBytesToRead) : '';
  }

  // Copies the given Javascript String object 'str' to the given byte array at address 'outIdx',
  // encoded in UTF8 form and null-terminated. The copy will require at most str.length*4+1 bytes of space in the HEAP.
  // Use the function lengthBytesUTF8 to compute the exact number of bytes (excluding null terminator) that this function will write.
  // Parameters:
  //   str: the Javascript string to copy.
  //   outU8Array: the array to copy to. Each index in this array is assumed to be one 8-byte element.
  //   outIdx: The starting offset in the array to begin the copying.
  //   maxBytesToWrite: The maximum number of bytes this function can write to the array.
  //                    This count should include the null terminator,
  //                    i.e. if maxBytesToWrite=1, only the null terminator will be written and nothing else.
  //                    maxBytesToWrite=0 does not write any bytes to the output, not even the null terminator.
  // Returns the number of bytes written, EXCLUDING the null terminator.

  function stringToUTF8Array(str, outU8Array, outIdx, maxBytesToWrite) {
    if (!(maxBytesToWrite > 0)) // Parameter maxBytesToWrite is not optional. Negative values, 0, null, undefined and false each don't write out any bytes.
      return 0;

    var startIdx = outIdx;
    var endIdx = outIdx + maxBytesToWrite - 1; // -1 for string null terminator.
    for (var i = 0; i < str.length; ++i) {
      // Gotcha: charCodeAt returns a 16-bit word that is a UTF-16 encoded code unit, not a Unicode code point of the character! So decode UTF16->UTF32->UTF8.
      // See http://unicode.org/faq/utf_bom.html#utf16-3
      // For UTF8 byte structure, see http://en.wikipedia.org/wiki/UTF-8#Description and https://www.ietf.org/rfc/rfc2279.txt and https://tools.ietf.org/html/rfc3629
      var u = str.charCodeAt(i); // possibly a lead surrogate
      if (u >= 0xD800 && u <= 0xDFFF) {
        var u1 = str.charCodeAt(++i);
        u = 0x10000 + ((u & 0x3FF) << 10) | (u1 & 0x3FF);
      }
      if (u <= 0x7F) {
        if (outIdx >= endIdx) break;
        outU8Array[outIdx++] = u;
      } else if (u <= 0x7FF) {
        if (outIdx + 1 >= endIdx) break;
        outU8Array[outIdx++] = 0xC0 | (u >> 6);
        outU8Array[outIdx++] = 0x80 | (u & 63);
      } else if (u <= 0xFFFF) {
        if (outIdx + 2 >= endIdx) break;
        outU8Array[outIdx++] = 0xE0 | (u >> 12);
        outU8Array[outIdx++] = 0x80 | ((u >> 6) & 63);
        outU8Array[outIdx++] = 0x80 | (u & 63);
      } else {
        if (outIdx + 3 >= endIdx) break;
        if (u >= 0x200000) warnOnce('Invalid Unicode code point 0x' + u.toString(16) + ' encountered when serializing a JS string to an UTF-8 string on the asm.js/wasm heap! (Valid unicode code points should be in range 0-0x1FFFFF).');
        outU8Array[outIdx++] = 0xF0 | (u >> 18);
        outU8Array[outIdx++] = 0x80 | ((u >> 12) & 63);
        outU8Array[outIdx++] = 0x80 | ((u >> 6) & 63);
        outU8Array[outIdx++] = 0x80 | (u & 63);
      }
    }
    // Null-terminate the pointer to the buffer.
    outU8Array[outIdx] = 0;
    return outIdx - startIdx;
  }

  // Copies the given Javascript String object 'str' to the emscripten HEAP at address 'outPtr',
  // null-terminated and encoded in UTF8 form. The copy will require at most str.length*4+1 bytes of space in the HEAP.
  // Use the function lengthBytesUTF8 to compute the exact number of bytes (excluding null terminator) that this function will write.
  // Returns the number of bytes written, EXCLUDING the null terminator.

  function stringToUTF8(str, outPtr, maxBytesToWrite) {
    assert(typeof maxBytesToWrite == 'number', 'stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!');
    return stringToUTF8Array(str, HEAPU8,outPtr, maxBytesToWrite);
  }


  // Given a pointer 'ptr' to a null-terminated UTF16LE-encoded string in the emscripten HEAP, returns
  // a copy of that string as a Javascript String object.

  var UTF16Decoder = typeof TextDecoder !== 'undefined' ? new TextDecoder('utf-16le') : undefined;

  function writeArrayToMemory(array, buffer) {
    assert(array.length >= 0, 'writeArrayToMemory array must have a length (should be an array or typed array)');
    HEAP8.set(array, buffer);
  }





  function demangle(func) {
    warnOnce('warning: build with  -s DEMANGLE_SUPPORT=1  to link in libcxxabi demangling');
    return func;
  }

  function demangleAll(text) {
    var regex =
      /__Z[\w\d_]+/g;
    return text.replace(regex,
      function(x) {
        var y = demangle(x);
        return x === y ? x : (y + ' [' + x + ']');
      });
  }

  function jsStackTrace() {
    var err = new Error();
    if (!err.stack) {
      // IE10+ special cases: It does have callstack info, but it is only populated if an Error object is thrown,
      // so try that as a special-case.
      try {
        throw new Error(0);
      } catch(e) {
        err = e;
      }
      if (!err.stack) {
        return '(no stack trace available)';
      }
    }
    return err.stack.toString();
  }

  function stackTrace() {
    var js = jsStackTrace();
    if (Module['extraStackTrace']) js += '\n' + Module['extraStackTrace']();
    return demangleAll(js);
  }
  var WASM_PAGE_SIZE = 65536;

  function alignUp(x, multiple) {
    if (x % multiple > 0) {
      x += multiple - (x % multiple);
    }
    return x;
  }

  var /** @type {ArrayBuffer} */
    buffer,
  /** @type {Int8Array} */
    HEAP8,
  /** @type {Uint8Array} */
    HEAPU8,
  /** @type {Int16Array} */
    HEAP16,
  /** @type {Uint16Array} */
    HEAPU16,
  /** @type {Int32Array} */
    HEAP32,
  /** @type {Uint32Array} */
    HEAPU32,
  /** @type {Float32Array} */
    HEAPF32,
  /** @type {Float64Array} */
    HEAPF64;

  function updateGlobalBufferViews() {
    Module['HEAP8'] = HEAP8 = new Int8Array(buffer);
    Module['HEAP16'] = HEAP16 = new Int16Array(buffer);
    Module['HEAP32'] = HEAP32 = new Int32Array(buffer);
    Module['HEAPU8'] = HEAPU8 = new Uint8Array(buffer);
    Module['HEAPU16'] = HEAPU16 = new Uint16Array(buffer);
    Module['HEAPU32'] = HEAPU32 = new Uint32Array(buffer);
    Module['HEAPF32'] = HEAPF32 = new Float32Array(buffer);
    Module['HEAPF64'] = HEAPF64 = new Float64Array(buffer);
  }


  var STACK_BASE = 24496,
      STACK_MAX = 5267376,
      DYNAMIC_BASE = 5267376,
      DYNAMICTOP_PTR = 24240;

  assert(STACK_BASE % 16 === 0, 'stack must start aligned');
  assert(DYNAMIC_BASE % 16 === 0, 'heap must start aligned');



  var TOTAL_STACK = 5242880;
  if (Module['TOTAL_STACK']) assert(TOTAL_STACK === Module['TOTAL_STACK'], 'the stack size can no longer be determined at runtime');

  var INITIAL_TOTAL_MEMORY = Module['TOTAL_MEMORY'] || 16777216;
  if (INITIAL_TOTAL_MEMORY < TOTAL_STACK) err('TOTAL_MEMORY should be larger than TOTAL_STACK, was ' + INITIAL_TOTAL_MEMORY + '! (TOTAL_STACK=' + TOTAL_STACK + ')');

  // Initialize the runtime's memory
  // check for full engine support (use string 'subarray' to avoid closure compiler confusion)
  assert(typeof Int32Array !== 'undefined' && typeof Float64Array !== 'undefined' && Int32Array.prototype.subarray !== undefined && Int32Array.prototype.set !== undefined,
         'JS engine does not provide full typed array support');







  // Use a provided buffer, if there is one, or else allocate a new one
  if (Module['buffer']) {
    buffer = Module['buffer'];
    assert(buffer.byteLength === INITIAL_TOTAL_MEMORY, 'provided buffer should be ' + INITIAL_TOTAL_MEMORY + ' bytes, but it is ' + buffer.byteLength);
  } else {
    // Use a WebAssembly memory where available
    if (typeof WebAssembly === 'object' && typeof WebAssembly.Memory === 'function') {
      assert(INITIAL_TOTAL_MEMORY % WASM_PAGE_SIZE === 0);
      wasmMemory = new WebAssembly.Memory({ 'initial': INITIAL_TOTAL_MEMORY / WASM_PAGE_SIZE });
      buffer = wasmMemory.buffer;
    } else
    {
      buffer = new ArrayBuffer(INITIAL_TOTAL_MEMORY);
    }
    assert(buffer.byteLength === INITIAL_TOTAL_MEMORY);
  }
  updateGlobalBufferViews();


  HEAP32[DYNAMICTOP_PTR>>2] = DYNAMIC_BASE;


  // Initializes the stack cookie. Called at the startup of main and at the startup of each thread in pthreads mode.
  function writeStackCookie() {
    assert((STACK_MAX & 3) == 0);
    HEAPU32[(STACK_MAX >> 2)-1] = 0x02135467;
    HEAPU32[(STACK_MAX >> 2)-2] = 0x89BACDFE;
  }

  function checkStackCookie() {
    if (HEAPU32[(STACK_MAX >> 2)-1] != 0x02135467 || HEAPU32[(STACK_MAX >> 2)-2] != 0x89BACDFE) {
      abort('Stack overflow! Stack cookie has been overwritten, expected hex dwords 0x89BACDFE and 0x02135467, but received 0x' + HEAPU32[(STACK_MAX >> 2)-2].toString(16) + ' ' + HEAPU32[(STACK_MAX >> 2)-1].toString(16));
    }
    // Also test the global address 0 for integrity.
    if (HEAP32[0] !== 0x63736d65 /* 'emsc' */) throw 'Runtime error: The application has corrupted its heap memory area (address zero)!';
  }

  function abortStackOverflow(allocSize) {
    abort('Stack overflow! Attempted to allocate ' + allocSize + ' bytes on the stack, but stack has only ' + (STACK_MAX - stackSave() + allocSize) + ' bytes available!');
  }


    HEAP32[0] = 0x63736d65; /* 'emsc' */



  // Endianness check (note: assumes compiler arch was little-endian)
  HEAP16[1] = 0x6373;
  if (HEAPU8[2] !== 0x73 || HEAPU8[3] !== 0x63) throw 'Runtime error: expected the system to be little-endian!';

  function callRuntimeCallbacks(callbacks) {
    while(callbacks.length > 0) {
      var callback = callbacks.shift();
      if (typeof callback == 'function') {
        callback();
        continue;
      }
      var func = callback.func;
      if (typeof func === 'number') {
        if (callback.arg === undefined) {
          Module['dynCall_v'](func);
        } else {
          Module['dynCall_vi'](func, callback.arg);
        }
      } else {
        func(callback.arg === undefined ? null : callback.arg);
      }
    }
  }

  var __ATPRERUN__  = []; // functions called before the runtime is initialized
  var __ATINIT__    = []; // functions called during startup
  var __ATMAIN__    = []; // functions called when main() is to be run
  var __ATPOSTRUN__ = []; // functions called after the main() is called

  var runtimeInitialized = false;
  var runtimeExited = false;


  function preRun() {
    // compatibility - merge in anything from Module['preRun'] at this time
    if (Module['preRun']) {
      if (typeof Module['preRun'] == 'function') Module['preRun'] = [Module['preRun']];
      while (Module['preRun'].length) {
        addOnPreRun(Module['preRun'].shift());
      }
    }
    callRuntimeCallbacks(__ATPRERUN__);
  }

  function ensureInitRuntime() {
    checkStackCookie();
    if (runtimeInitialized) return;
    runtimeInitialized = true;
    
    callRuntimeCallbacks(__ATINIT__);
  }

  function preMain() {
    checkStackCookie();
    
    callRuntimeCallbacks(__ATMAIN__);
  }

  function exitRuntime() {
    checkStackCookie();
    runtimeExited = true;
  }

  function postRun() {
    checkStackCookie();
    // compatibility - merge in anything from Module['postRun'] at this time
    if (Module['postRun']) {
      if (typeof Module['postRun'] == 'function') Module['postRun'] = [Module['postRun']];
      while (Module['postRun'].length) {
        addOnPostRun(Module['postRun'].shift());
      }
    }
    callRuntimeCallbacks(__ATPOSTRUN__);
  }

  function addOnPreRun(cb) {
    __ATPRERUN__.unshift(cb);
  }

  function addOnPostRun(cb) {
    __ATPOSTRUN__.unshift(cb);
  }


  assert(Math.imul, 'This browser does not support Math.imul(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill');
  assert(Math.fround, 'This browser does not support Math.fround(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill');
  assert(Math.clz32, 'This browser does not support Math.clz32(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill');
  assert(Math.trunc, 'This browser does not support Math.trunc(), build with LEGACY_VM_SUPPORT or POLYFILL_OLD_MATH_FUNCTIONS to add in a polyfill');



  // A counter of dependencies for calling run(). If we need to
  // do asynchronous work before running, increment this and
  // decrement it. Incrementing must happen in a place like
  // Module.preRun (used by emcc to add file preloading).
  // Note that you can add dependencies in preRun, even though
  // it happens right before run - run will be postponed until
  // the dependencies are met.
  var runDependencies = 0;
  var runDependencyWatcher = null;
  var dependenciesFulfilled = null; // overridden to take different actions when all run dependencies are fulfilled
  var runDependencyTracking = {};

  function addRunDependency(id) {
    runDependencies++;
    if (Module['monitorRunDependencies']) {
      Module['monitorRunDependencies'](runDependencies);
    }
    if (id) {
      assert(!runDependencyTracking[id]);
      runDependencyTracking[id] = 1;
      if (runDependencyWatcher === null && typeof setInterval !== 'undefined') {
        // Check for missing dependencies every few seconds
        runDependencyWatcher = setInterval(function() {
          if (ABORT) {
            clearInterval(runDependencyWatcher);
            runDependencyWatcher = null;
            return;
          }
          var shown = false;
          for (var dep in runDependencyTracking) {
            if (!shown) {
              shown = true;
              err('still waiting on run dependencies:');
            }
            err('dependency: ' + dep);
          }
          if (shown) {
            err('(end of list)');
          }
        }, 10000);
      }
    } else {
      err('warning: run dependency added without ID');
    }
  }

  function removeRunDependency(id) {
    runDependencies--;
    if (Module['monitorRunDependencies']) {
      Module['monitorRunDependencies'](runDependencies);
    }
    if (id) {
      assert(runDependencyTracking[id]);
      delete runDependencyTracking[id];
    } else {
      err('warning: run dependency removed without ID');
    }
    if (runDependencies == 0) {
      if (runDependencyWatcher !== null) {
        clearInterval(runDependencyWatcher);
        runDependencyWatcher = null;
      }
      if (dependenciesFulfilled) {
        var callback = dependenciesFulfilled;
        dependenciesFulfilled = null;
        callback(); // can add another dependenciesFulfilled
      }
    }
  }

  Module["preloadedImages"] = {}; // maps url to image data
  Module["preloadedAudios"] = {}; // maps url to audio data



  var /* show errors on likely calls to FS when it was not included */ FS = {
    error: function() {
      abort('Filesystem support (FS) was not included. The problem is that you are using files from JS, but files were not used from C/C++, so filesystem support was not auto-included. You can force-include filesystem support with  -s FORCE_FILESYSTEM=1');
    },
    init: function() { FS.error(); },
    createDataFile: function() { FS.error(); },
    createPreloadedFile: function() { FS.error(); },
    createLazyFile: function() { FS.error(); },
    open: function() { FS.error(); },
    mkdev: function() { FS.error(); },
    registerDevice: function() { FS.error(); },
    analyzePath: function() { FS.error(); },
    loadFilesFromDB: function() { FS.error(); },

    ErrnoError: function ErrnoError() { FS.error(); },
  };
  Module['FS_createDataFile'] = FS.createDataFile;
  Module['FS_createPreloadedFile'] = FS.createPreloadedFile;



  // Copyright 2017 The Emscripten Authors.  All rights reserved.
  // Emscripten is available under two separate licenses, the MIT license and the
  // University of Illinois/NCSA Open Source License.  Both these licenses can be
  // found in the LICENSE file.

  // Prefix of data URIs emitted by SINGLE_FILE and related options.
  var dataURIPrefix = 'data:application/octet-stream;base64,';

  // Indicates whether filename is a base64 data URI.
  function isDataURI(filename) {
    return String.prototype.startsWith ?
        filename.startsWith(dataURIPrefix) :
        filename.indexOf(dataURIPrefix) === 0;
  }




  var wasmBinaryFile = 'backend.wasm';
  if (!isDataURI(wasmBinaryFile)) {
    wasmBinaryFile = locateFile(wasmBinaryFile);
  }

  function getBinary() {
    try {
      if (Module['wasmBinary']) {
        return new Uint8Array(Module['wasmBinary']);
      }
      if (Module['readBinary']) {
        return Module['readBinary'](wasmBinaryFile);
      } else {
        throw "both async and sync fetching of the wasm failed";
      }
    }
    catch (err) {
      abort(err);
    }
  }

  function getBinaryPromise() {
    // if we don't have the binary yet, and have the Fetch api, use that
    // in some environments, like Electron's render process, Fetch api may be present, but have a different context than expected, let's only use it on the Web
    if (!Module['wasmBinary'] && (ENVIRONMENT_IS_WEB || ENVIRONMENT_IS_WORKER) && typeof fetch === 'function') {
      return fetch(wasmBinaryFile, { credentials: 'same-origin' }).then(function(response) {
        if (!response['ok']) {
          throw "failed to load wasm binary file at '" + wasmBinaryFile + "'";
        }
        return response['arrayBuffer']();
      }).catch(function () {
        return getBinary();
      });
    }
    // Otherwise, getBinary should be able to get it synchronously
    return new Promise(function(resolve, reject) {
      resolve(getBinary());
    });
  }

  // Create the wasm instance.
  // Receives the wasm imports, returns the exports.
  function createWasm(env) {
    // prepare imports
    var info = {
      'env': env
      ,
      'global': {
        'NaN': NaN,
        'Infinity': Infinity
      },
      'global.Math': Math,
      'asm2wasm': asm2wasmImports
    };
    // Load the wasm module and create an instance of using native support in the JS engine.
    // handle a generated wasm instance, receiving its exports and
    // performing other necessary setup
    function receiveInstance(instance, module) {
      var exports = instance.exports;
      Module['asm'] = exports;
      removeRunDependency('wasm-instantiate');
    }
    addRunDependency('wasm-instantiate');

    // User shell pages can write their own Module.instantiateWasm = function(imports, successCallback) callback
    // to manually instantiate the Wasm module themselves. This allows pages to run the instantiation parallel
    // to any other async startup actions they are performing.
    if (Module['instantiateWasm']) {
      try {
        return Module['instantiateWasm'](info, receiveInstance);
      } catch(e) {
        err('Module.instantiateWasm callback failed with error: ' + e);
        return false;
      }
    }

    // Async compilation can be confusing when an error on the page overwrites Module
    // (for example, if the order of elements is wrong, and the one defining Module is
    // later), so we save Module and check it later.
    var trueModule = Module;
    function receiveInstantiatedSource(output) {
      // 'output' is a WebAssemblyInstantiatedSource object which has both the module and instance.
      // receiveInstance() will swap in the exports (to Module.asm) so they can be called
      assert(Module === trueModule, 'the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?');
      trueModule = null;
        // TODO: Due to Closure regression https://github.com/google/closure-compiler/issues/3193, the above line no longer optimizes out down to the following line.
        // When the regression is fixed, can restore the above USE_PTHREADS-enabled path.
      receiveInstance(output['instance']);
    }
    function instantiateArrayBuffer(receiver) {
      getBinaryPromise().then(function(binary) {
        return WebAssembly.instantiate(binary, info);
      }).then(receiver, function(reason) {
        err('failed to asynchronously prepare wasm: ' + reason);
        abort(reason);
      });
    }
    // Prefer streaming instantiation if available.
    if (!Module['wasmBinary'] &&
        typeof WebAssembly.instantiateStreaming === 'function' &&
        !isDataURI(wasmBinaryFile) &&
        typeof fetch === 'function') {
      WebAssembly.instantiateStreaming(fetch(wasmBinaryFile, { credentials: 'same-origin' }), info)
        .then(receiveInstantiatedSource, function(reason) {
          // We expect the most common failure cause to be a bad MIME type for the binary,
          // in which case falling back to ArrayBuffer instantiation should work.
          err('wasm streaming compile failed: ' + reason);
          err('falling back to ArrayBuffer instantiation');
          instantiateArrayBuffer(receiveInstantiatedSource);
        });
    } else {
      instantiateArrayBuffer(receiveInstantiatedSource);
    }
    return {}; // no exports yet; we'll fill them in later
  }

  // Provide an "asm.js function" for the application, called to "link" the asm.js module. We instantiate
  // the wasm module at that time, and it receives imports and provides exports and so forth, the app
  // doesn't need to care that it is wasm or asm.js.

  Module['asm'] = function(global, env, providedBuffer) {
    // memory was already allocated (so js could use the buffer)
    env['memory'] = wasmMemory
    ;
    // import table
    env['table'] = wasmTable = new WebAssembly.Table({
      'initial': 10,
      'maximum': 10,
      'element': 'anyfunc'
    });
    env['__memory_base'] = 1024; // tell the memory segments where to place themselves
    env['__table_base'] = 0; // table starts at 0 by default (even in dynamic linking, for the main module)

    var exports = createWasm(env);
    assert(exports, 'binaryen setup failed (no wasm support?)');
    return exports;
  };





  // STATICTOP = STATIC_BASE + 23472;
  /* global initializers */ /*__ATINIT__.push();*/








  /* no memory initializer */
  var tempDoublePtr = 24480;
  assert(tempDoublePtr % 8 == 0);

  // {{PRE_LIBRARY}}


    function ___assert_fail(condition, filename, line, func) {
        abort('Assertion failed: ' + UTF8ToString(condition) + ', at: ' + [filename ? UTF8ToString(filename) : 'unknown filename', line, func ? UTF8ToString(func) : 'unknown function']);
      }

    function ___lock() {}

    
    var SYSCALLS={buffers:[null,[],[]],printChar:function (stream, curr) {
          var buffer = SYSCALLS.buffers[stream];
          assert(buffer);
          if (curr === 0 || curr === 10) {
            (stream === 1 ? out : err)(UTF8ArrayToString(buffer, 0));
            buffer.length = 0;
          } else {
            buffer.push(curr);
          }
        },varargs:0,get:function (varargs) {
          SYSCALLS.varargs += 4;
          var ret = HEAP32[(((SYSCALLS.varargs)-(4))>>2)];
          return ret;
        },getStr:function () {
          var ret = UTF8ToString(SYSCALLS.get());
          return ret;
        },get64:function () {
          var low = SYSCALLS.get(), high = SYSCALLS.get();
          if (low >= 0) assert(high === 0);
          else assert(high === -1);
          return low;
        },getZero:function () {
          assert(SYSCALLS.get() === 0);
        }};function ___syscall140(which, varargs) {SYSCALLS.varargs = varargs;
    try {
     // llseek
        var stream = SYSCALLS.getStreamFromFD(), offset_high = SYSCALLS.get(), offset_low = SYSCALLS.get(), result = SYSCALLS.get(), whence = SYSCALLS.get();
        // NOTE: offset_high is unused - Emscripten's off_t is 32-bit
        var offset = offset_low;
        FS.llseek(stream, offset, whence);
        HEAP32[((result)>>2)]=stream.position;
        if (stream.getdents && offset === 0 && whence === 0) stream.getdents = null; // reset readdir state
        return 0;
      } catch (e) {
      if (typeof FS === 'undefined' || !(e instanceof FS.ErrnoError)) abort(e);
      return -e.errno;
    }
    }

    function ___syscall145(which, varargs) {SYSCALLS.varargs = varargs;
    try {
     // readv
        var stream = SYSCALLS.getStreamFromFD(), iov = SYSCALLS.get(), iovcnt = SYSCALLS.get();
        return SYSCALLS.doReadv(stream, iov, iovcnt);
      } catch (e) {
      if (typeof FS === 'undefined' || !(e instanceof FS.ErrnoError)) abort(e);
      return -e.errno;
    }
    }

    
    function flush_NO_FILESYSTEM() {
        // flush anything remaining in the buffers during shutdown
        var fflush = Module["_fflush"];
        if (fflush) fflush(0);
        var buffers = SYSCALLS.buffers;
        if (buffers[1].length) SYSCALLS.printChar(1, 10);
        if (buffers[2].length) SYSCALLS.printChar(2, 10);
      }function ___syscall146(which, varargs) {SYSCALLS.varargs = varargs;
    try {
     // writev
        // hack to support printf in FILESYSTEM=0
        var stream = SYSCALLS.get(), iov = SYSCALLS.get(), iovcnt = SYSCALLS.get();
        var ret = 0;
        for (var i = 0; i < iovcnt; i++) {
          var ptr = HEAP32[(((iov)+(i*8))>>2)];
          var len = HEAP32[(((iov)+(i*8 + 4))>>2)];
          for (var j = 0; j < len; j++) {
            SYSCALLS.printChar(stream, HEAPU8[ptr+j]);
          }
          ret += len;
        }
        return ret;
      } catch (e) {
      if (typeof FS === 'undefined' || !(e instanceof FS.ErrnoError)) abort(e);
      return -e.errno;
    }
    }

    
    function ___setErrNo(value) {
        if (Module['___errno_location']) HEAP32[((Module['___errno_location']())>>2)]=value;
        else err('failed to set errno from JS');
        return value;
      }function ___syscall221(which, varargs) {SYSCALLS.varargs = varargs;
    try {
     // fcntl64
        return 0;
      } catch (e) {
      if (typeof FS === 'undefined' || !(e instanceof FS.ErrnoError)) abort(e);
      return -e.errno;
    }
    }

    function ___syscall5(which, varargs) {SYSCALLS.varargs = varargs;
    try {
     // open
        var pathname = SYSCALLS.getStr(), flags = SYSCALLS.get(), mode = SYSCALLS.get(); // optional TODO
        var stream = FS.open(pathname, flags, mode);
        return stream.fd;
      } catch (e) {
      if (typeof FS === 'undefined' || !(e instanceof FS.ErrnoError)) abort(e);
      return -e.errno;
    }
    }

    function ___syscall54(which, varargs) {SYSCALLS.varargs = varargs;
    try {
     // ioctl
        return 0;
      } catch (e) {
      if (typeof FS === 'undefined' || !(e instanceof FS.ErrnoError)) abort(e);
      return -e.errno;
    }
    }

    function ___syscall6(which, varargs) {SYSCALLS.varargs = varargs;
    try {
     // close
        var stream = SYSCALLS.getStreamFromFD();
        FS.close(stream);
        return 0;
      } catch (e) {
      if (typeof FS === 'undefined' || !(e instanceof FS.ErrnoError)) abort(e);
      return -e.errno;
    }
    }

    function ___unlock() {}

    function _abort() {
        Module['abort']();
      }

    function _emscripten_get_heap_size() {
        return HEAP8.length;
      }

    
    function abortOnCannotGrowMemory(requestedSize) {
        abort('Cannot enlarge memory arrays to size ' + requestedSize + ' bytes (OOM). Either (1) compile with  -s TOTAL_MEMORY=X  with X higher than the current value ' + HEAP8.length + ', (2) compile with  -s ALLOW_MEMORY_GROWTH=1  which allows increasing the size at runtime, or (3) if you want malloc to return NULL (0) instead of this abort, compile with  -s ABORTING_MALLOC=0 ');
      }
    
    function emscripten_realloc_buffer(size) {
        var PAGE_MULTIPLE = 65536;
        size = alignUp(size, PAGE_MULTIPLE); // round up to wasm page size
        var oldSize = buffer.byteLength;
        // native wasm support
        try {
          var result = wasmMemory.grow((size - oldSize) / 65536); // .grow() takes a delta compared to the previous size
          if (result !== (-1 | 0)) {
            // success in native wasm memory growth, get the buffer from the memory
            return buffer = wasmMemory.buffer;
          } else {
            return null;
          }
        } catch(e) {
          console.error('emscripten_realloc_buffer: Attempted to grow from ' + oldSize  + ' bytes to ' + size + ' bytes, but got error: ' + e);
          return null;
        }
      }function _emscripten_resize_heap(requestedSize) {
        var oldSize = _emscripten_get_heap_size();
        assert(requestedSize > oldSize); // This function should only ever be called after the ceiling of the dynamic heap has already been bumped to exceed the current total size of the asm.js heap.
    
    
        var PAGE_MULTIPLE = 65536;
        var LIMIT = 2147483648 - PAGE_MULTIPLE; // We can do one page short of 2GB as theoretical maximum.
    
        if (requestedSize > LIMIT) {
          err('Cannot enlarge memory, asked to go up to ' + requestedSize + ' bytes, but the limit is ' + LIMIT + ' bytes!');
          return false;
        }
    
        var MIN_TOTAL_MEMORY = 16777216;
        var newSize = Math.max(oldSize, MIN_TOTAL_MEMORY); // So the loop below will not be infinite, and minimum asm.js memory size is 16MB.
    
        while (newSize < requestedSize) { // Keep incrementing the heap size as long as it's less than what is requested.
          if (newSize <= 536870912) {
            newSize = alignUp(2 * newSize, PAGE_MULTIPLE); // Simple heuristic: double until 1GB...
          } else {
            // ..., but after that, add smaller increments towards 2GB, which we cannot reach
            newSize = Math.min(alignUp((3 * newSize + 2147483648) / 4, PAGE_MULTIPLE), LIMIT);
            if (newSize === oldSize) {
              warnOnce('Cannot ask for more memory since we reached the practical limit in browsers (which is just below 2GB), so the request would have failed. Requesting only ' + HEAP8.length);
            }
          }
        }
    
        var replacement = emscripten_realloc_buffer(newSize);
        if (!replacement || replacement.byteLength != newSize) {
          err('Failed to grow the heap from ' + oldSize + ' bytes to ' + newSize + ' bytes, not enough memory!');
          if (replacement) {
            err('Expected to get back a buffer of size ' + newSize + ' bytes, but instead got back a buffer of size ' + replacement.byteLength);
          }
          return false;
        }
    
        // everything worked
        updateGlobalBufferViews();
    
    
    
        return true;
      }

    function _exit(status) {
        // void _exit(int status);
        // http://pubs.opengroup.org/onlinepubs/000095399/functions/exit.html
        exit(status);
      }

    
    function _emscripten_memcpy_big(dest, src, num) {
        HEAPU8.set(HEAPU8.subarray(src, src+num), dest);
      }


  // ASM_LIBRARY EXTERN PRIMITIVES: Int8Array,Int32Array


  function nullFunc_ii(x) { err("Invalid function pointer called with signature 'ii'. Perhaps this is an invalid value (e.g. caused by calling a virtual method on a NULL pointer)? Or calling a function with an incorrect type, which will fail? (it is worth building your source files with -Werror (warnings are errors), as warnings can indicate undefined behavior which can cause this)");  err("Build with ASSERTIONS=2 for more info.");abort(x); }

  function nullFunc_iiii(x) { err("Invalid function pointer called with signature 'iiii'. Perhaps this is an invalid value (e.g. caused by calling a virtual method on a NULL pointer)? Or calling a function with an incorrect type, which will fail? (it is worth building your source files with -Werror (warnings are errors), as warnings can indicate undefined behavior which can cause this)");  err("Build with ASSERTIONS=2 for more info.");abort(x); }

  var asmGlobalArg = {};

  var asmLibraryArg = {
    "abort": abort,
    "setTempRet0": setTempRet0,
    "getTempRet0": getTempRet0,
    "abortStackOverflow": abortStackOverflow,
    "nullFunc_ii": nullFunc_ii,
    "nullFunc_iiii": nullFunc_iiii,
    "___assert_fail": ___assert_fail,
    "___lock": ___lock,
    "___setErrNo": ___setErrNo,
    "___syscall140": ___syscall140,
    "___syscall145": ___syscall145,
    "___syscall146": ___syscall146,
    "___syscall221": ___syscall221,
    "___syscall5": ___syscall5,
    "___syscall54": ___syscall54,
    "___syscall6": ___syscall6,
    "___unlock": ___unlock,
    "_abort": _abort,
    "_emscripten_get_heap_size": _emscripten_get_heap_size,
    "_emscripten_memcpy_big": _emscripten_memcpy_big,
    "_emscripten_resize_heap": _emscripten_resize_heap,
    "_exit": _exit,
    "abortOnCannotGrowMemory": abortOnCannotGrowMemory,
    "emscripten_realloc_buffer": emscripten_realloc_buffer,
    "flush_NO_FILESYSTEM": flush_NO_FILESYSTEM,
    "tempDoublePtr": tempDoublePtr,
    "DYNAMICTOP_PTR": DYNAMICTOP_PTR
  };
  // EMSCRIPTEN_START_ASM
  var asm =Module["asm"]// EMSCRIPTEN_END_ASM
  (asmGlobalArg, asmLibraryArg, buffer);

  var real__errnum = asm["_errnum"]; asm["_errnum"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__errnum.apply(null, arguments);
  };

  var real__free = asm["_free"]; asm["_free"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__free.apply(null, arguments);
  };

  var real__get_error_msg = asm["_get_error_msg"]; asm["_get_error_msg"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__get_error_msg.apply(null, arguments);
  };

  var real__malloc = asm["_malloc"]; asm["_malloc"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__malloc.apply(null, arguments);
  };

  var real__sbrk = asm["_sbrk"]; asm["_sbrk"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__sbrk.apply(null, arguments);
  };

  var real__web_T = asm["_web_T"]; asm["_web_T"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__web_T.apply(null, arguments);
  };

  var real__web_W = asm["_web_W"]; asm["_web_W"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__web_W.apply(null, arguments);
  };

  var real__web_compile = asm["_web_compile"]; asm["_web_compile"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__web_compile.apply(null, arguments);
  };

  var real__web_output = asm["_web_output"]; asm["_web_output"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__web_output.apply(null, arguments);
  };

  var real__web_run = asm["_web_run"]; asm["_web_run"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__web_run.apply(null, arguments);
  };

  var real__web_start = asm["_web_start"]; asm["_web_start"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real__web_start.apply(null, arguments);
  };

  var real_establishStackSpace = asm["establishStackSpace"]; asm["establishStackSpace"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real_establishStackSpace.apply(null, arguments);
  };

  var real_stackAlloc = asm["stackAlloc"]; asm["stackAlloc"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real_stackAlloc.apply(null, arguments);
  };

  var real_stackRestore = asm["stackRestore"]; asm["stackRestore"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real_stackRestore.apply(null, arguments);
  };

  var real_stackSave = asm["stackSave"]; asm["stackSave"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return real_stackSave.apply(null, arguments);
  };
  Module["asm"] = asm;
  var _emscripten_replace_memory = Module["_emscripten_replace_memory"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_emscripten_replace_memory"].apply(null, arguments) };
  var _errnum = Module["_errnum"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_errnum"].apply(null, arguments) };
  var _free = Module["_free"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_free"].apply(null, arguments) };
  var _get_error_msg = Module["_get_error_msg"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_get_error_msg"].apply(null, arguments) };
  var _malloc = Module["_malloc"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_malloc"].apply(null, arguments) };
  var _memcpy = Module["_memcpy"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_memcpy"].apply(null, arguments) };
  var _memset = Module["_memset"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_memset"].apply(null, arguments) };
  var _sbrk = Module["_sbrk"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_sbrk"].apply(null, arguments) };
  var _web_T = Module["_web_T"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_web_T"].apply(null, arguments) };
  var _web_W = Module["_web_W"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_web_W"].apply(null, arguments) };
  var _web_compile = Module["_web_compile"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_web_compile"].apply(null, arguments) };
  var _web_output = Module["_web_output"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_web_output"].apply(null, arguments) };
  var _web_run = Module["_web_run"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_web_run"].apply(null, arguments) };
  var _web_start = Module["_web_start"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["_web_start"].apply(null, arguments) };
  var establishStackSpace = Module["establishStackSpace"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["establishStackSpace"].apply(null, arguments) };
  var stackAlloc = Module["stackAlloc"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["stackAlloc"].apply(null, arguments) };
  var stackRestore = Module["stackRestore"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["stackRestore"].apply(null, arguments) };
  var stackSave = Module["stackSave"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["stackSave"].apply(null, arguments) };
  var dynCall_ii = Module["dynCall_ii"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["dynCall_ii"].apply(null, arguments) };
  var dynCall_iiii = Module["dynCall_iiii"] = function() {
    assert(runtimeInitialized, 'you need to wait for the runtime to be ready (e.g. wait for main() to be called)');
    assert(!runtimeExited, 'the runtime was exited (use NO_EXIT_RUNTIME to keep it alive after main() exits)');
    return Module["asm"]["dynCall_iiii"].apply(null, arguments) };



  // === Auto-generated postamble setup entry stuff ===

  Module['asm'] = asm;

  if (!Module["intArrayFromString"]) Module["intArrayFromString"] = function() { abort("'intArrayFromString' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["intArrayToString"]) Module["intArrayToString"] = function() { abort("'intArrayToString' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["ccall"]) Module["ccall"] = function() { abort("'ccall' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  Module["cwrap"] = cwrap;
  if (!Module["setValue"]) Module["setValue"] = function() { abort("'setValue' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["getValue"]) Module["getValue"] = function() { abort("'getValue' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["allocate"]) Module["allocate"] = function() { abort("'allocate' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["getMemory"]) Module["getMemory"] = function() { abort("'getMemory' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["AsciiToString"]) Module["AsciiToString"] = function() { abort("'AsciiToString' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["stringToAscii"]) Module["stringToAscii"] = function() { abort("'stringToAscii' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["UTF8ArrayToString"]) Module["UTF8ArrayToString"] = function() { abort("'UTF8ArrayToString' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["UTF8ToString"]) Module["UTF8ToString"] = function() { abort("'UTF8ToString' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["stringToUTF8Array"]) Module["stringToUTF8Array"] = function() { abort("'stringToUTF8Array' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["stringToUTF8"]) Module["stringToUTF8"] = function() { abort("'stringToUTF8' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["lengthBytesUTF8"]) Module["lengthBytesUTF8"] = function() { abort("'lengthBytesUTF8' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["UTF16ToString"]) Module["UTF16ToString"] = function() { abort("'UTF16ToString' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["stringToUTF16"]) Module["stringToUTF16"] = function() { abort("'stringToUTF16' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["lengthBytesUTF16"]) Module["lengthBytesUTF16"] = function() { abort("'lengthBytesUTF16' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["UTF32ToString"]) Module["UTF32ToString"] = function() { abort("'UTF32ToString' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["stringToUTF32"]) Module["stringToUTF32"] = function() { abort("'stringToUTF32' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["lengthBytesUTF32"]) Module["lengthBytesUTF32"] = function() { abort("'lengthBytesUTF32' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["allocateUTF8"]) Module["allocateUTF8"] = function() { abort("'allocateUTF8' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["stackTrace"]) Module["stackTrace"] = function() { abort("'stackTrace' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["addOnPreRun"]) Module["addOnPreRun"] = function() { abort("'addOnPreRun' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["addOnInit"]) Module["addOnInit"] = function() { abort("'addOnInit' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["addOnPreMain"]) Module["addOnPreMain"] = function() { abort("'addOnPreMain' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["addOnExit"]) Module["addOnExit"] = function() { abort("'addOnExit' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["addOnPostRun"]) Module["addOnPostRun"] = function() { abort("'addOnPostRun' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["writeStringToMemory"]) Module["writeStringToMemory"] = function() { abort("'writeStringToMemory' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["writeArrayToMemory"]) Module["writeArrayToMemory"] = function() { abort("'writeArrayToMemory' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["writeAsciiToMemory"]) Module["writeAsciiToMemory"] = function() { abort("'writeAsciiToMemory' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["addRunDependency"]) Module["addRunDependency"] = function() { abort("'addRunDependency' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["removeRunDependency"]) Module["removeRunDependency"] = function() { abort("'removeRunDependency' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["ENV"]) Module["ENV"] = function() { abort("'ENV' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["FS"]) Module["FS"] = function() { abort("'FS' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["FS_createFolder"]) Module["FS_createFolder"] = function() { abort("'FS_createFolder' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["FS_createPath"]) Module["FS_createPath"] = function() { abort("'FS_createPath' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["FS_createDataFile"]) Module["FS_createDataFile"] = function() { abort("'FS_createDataFile' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["FS_createPreloadedFile"]) Module["FS_createPreloadedFile"] = function() { abort("'FS_createPreloadedFile' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["FS_createLazyFile"]) Module["FS_createLazyFile"] = function() { abort("'FS_createLazyFile' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["FS_createLink"]) Module["FS_createLink"] = function() { abort("'FS_createLink' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["FS_createDevice"]) Module["FS_createDevice"] = function() { abort("'FS_createDevice' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["FS_unlink"]) Module["FS_unlink"] = function() { abort("'FS_unlink' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ). Alternatively, forcing filesystem support (-s FORCE_FILESYSTEM=1) can export this for you"); };
  if (!Module["GL"]) Module["GL"] = function() { abort("'GL' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["dynamicAlloc"]) Module["dynamicAlloc"] = function() { abort("'dynamicAlloc' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["warnOnce"]) Module["warnOnce"] = function() { abort("'warnOnce' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["loadDynamicLibrary"]) Module["loadDynamicLibrary"] = function() { abort("'loadDynamicLibrary' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["loadWebAssemblyModule"]) Module["loadWebAssemblyModule"] = function() { abort("'loadWebAssemblyModule' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["getLEB"]) Module["getLEB"] = function() { abort("'getLEB' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["getFunctionTables"]) Module["getFunctionTables"] = function() { abort("'getFunctionTables' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["alignFunctionTables"]) Module["alignFunctionTables"] = function() { abort("'alignFunctionTables' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["registerFunctions"]) Module["registerFunctions"] = function() { abort("'registerFunctions' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["addFunction"]) Module["addFunction"] = function() { abort("'addFunction' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["removeFunction"]) Module["removeFunction"] = function() { abort("'removeFunction' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["getFuncWrapper"]) Module["getFuncWrapper"] = function() { abort("'getFuncWrapper' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["prettyPrint"]) Module["prettyPrint"] = function() { abort("'prettyPrint' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["makeBigInt"]) Module["makeBigInt"] = function() { abort("'makeBigInt' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["dynCall"]) Module["dynCall"] = function() { abort("'dynCall' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["getCompilerSetting"]) Module["getCompilerSetting"] = function() { abort("'getCompilerSetting' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["stackSave"]) Module["stackSave"] = function() { abort("'stackSave' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["stackRestore"]) Module["stackRestore"] = function() { abort("'stackRestore' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["stackAlloc"]) Module["stackAlloc"] = function() { abort("'stackAlloc' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["establishStackSpace"]) Module["establishStackSpace"] = function() { abort("'establishStackSpace' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["print"]) Module["print"] = function() { abort("'print' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["printErr"]) Module["printErr"] = function() { abort("'printErr' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["getTempRet0"]) Module["getTempRet0"] = function() { abort("'getTempRet0' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["setTempRet0"]) Module["setTempRet0"] = function() { abort("'setTempRet0' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["Pointer_stringify"]) Module["Pointer_stringify"] = function() { abort("'Pointer_stringify' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["writeStackCookie"]) Module["writeStackCookie"] = function() { abort("'writeStackCookie' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["checkStackCookie"]) Module["checkStackCookie"] = function() { abort("'checkStackCookie' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };
  if (!Module["abortStackOverflow"]) Module["abortStackOverflow"] = function() { abort("'abortStackOverflow' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); };if (!Module["ALLOC_NORMAL"]) Object.defineProperty(Module, "ALLOC_NORMAL", { get: function() { abort("'ALLOC_NORMAL' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); } });
  if (!Module["ALLOC_STACK"]) Object.defineProperty(Module, "ALLOC_STACK", { get: function() { abort("'ALLOC_STACK' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); } });
  if (!Module["ALLOC_DYNAMIC"]) Object.defineProperty(Module, "ALLOC_DYNAMIC", { get: function() { abort("'ALLOC_DYNAMIC' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); } });
  if (!Module["ALLOC_NONE"]) Object.defineProperty(Module, "ALLOC_NONE", { get: function() { abort("'ALLOC_NONE' was not exported. add it to EXTRA_EXPORTED_RUNTIME_METHODS (see the FAQ)"); } });



  // Modularize mode returns a function, which can be called to
  // create instances. The instances provide a then() method,
  // must like a Promise, that receives a callback. The callback
  // is called when the module is ready to run, with the module
  // as a parameter. (Like a Promise, it also returns the module
  // so you can use the output of .then(..)).
  Module['then'] = function(func) {
    // We may already be ready to run code at this time. if
    // so, just queue a call to the callback.
    if (Module['calledRun']) {
      func(Module);
    } else {
      // we are not ready to call then() yet. we must call it
      // at the same time we would call onRuntimeInitialized.
      var old = Module['onRuntimeInitialized'];
      Module['onRuntimeInitialized'] = function() {
        if (old) old();
        func(Module);
      };
    }
    return Module;
  };

  /**
   * @constructor
   * @extends {Error}
   * @this {ExitStatus}
   */
  function ExitStatus(status) {
    this.name = "ExitStatus";
    this.message = "Program terminated with exit(" + status + ")";
    this.status = status;
  }ExitStatus.prototype = new Error();
  ExitStatus.prototype.constructor = ExitStatus;

  dependenciesFulfilled = function runCaller() {
    // If run has never been called, and we should call run (INVOKE_RUN is true, and Module.noInitialRun is not false)
    if (!Module['calledRun']) run();
    if (!Module['calledRun']) dependenciesFulfilled = runCaller; // try this again later, after new deps are fulfilled
  };





  /** @type {function(Array=)} */
  function run(args) {
    args = args || Module['arguments'];

    if (runDependencies > 0) {
      return;
    }

    writeStackCookie();

    preRun();

    if (runDependencies > 0) return; // a preRun added a dependency, run will be called later
    if (Module['calledRun']) return; // run may have just been called through dependencies being fulfilled just in this very frame

    function doRun() {
      if (Module['calledRun']) return; // run may have just been called while the async setStatus time below was happening
      Module['calledRun'] = true;

      if (ABORT) return;

      ensureInitRuntime();

      preMain();

      if (Module['onRuntimeInitialized']) Module['onRuntimeInitialized']();

      assert(!Module['_main'], 'compiled without a main, but one is present. if you added it from JS, use Module["onRuntimeInitialized"]');

      postRun();
    }

    if (Module['setStatus']) {
      Module['setStatus']('Running...');
      setTimeout(function() {
        setTimeout(function() {
          Module['setStatus']('');
        }, 1);
        doRun();
      }, 1);
    } else {
      doRun();
    }
    checkStackCookie();
  }
  Module['run'] = run;

  function checkUnflushedContent() {
    // Compiler settings do not allow exiting the runtime, so flushing
    // the streams is not possible. but in ASSERTIONS mode we check
    // if there was something to flush, and if so tell the user they
    // should request that the runtime be exitable.
    // Normally we would not even include flush() at all, but in ASSERTIONS
    // builds we do so just for this check, and here we see if there is any
    // content to flush, that is, we check if there would have been
    // something a non-ASSERTIONS build would have not seen.
    // How we flush the streams depends on whether we are in FILESYSTEM=0
    // mode (which has its own special function for this; otherwise, all
    // the code is inside libc)
    var print = out;
    var printErr = err;
    var has = false;
    out = err = function(x) {
      has = true;
    };
    try { // it doesn't matter if it fails
      var flush = flush_NO_FILESYSTEM;
      if (flush) flush(0);
    } catch(e) {}
    out = print;
    err = printErr;
    if (has) {
      warnOnce('stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the FAQ), or make sure to emit a newline when you printf etc.');
      warnOnce('(this may also be due to not including full filesystem support - try building with -s FORCE_FILESYSTEM=1)');
    }
  }

  function exit(status, implicit) {
    checkUnflushedContent();

    // if this is just main exit-ing implicitly, and the status is 0, then we
    // don't need to do anything here and can just leave. if the status is
    // non-zero, though, then we need to report it.
    // (we may have warned about this earlier, if a situation justifies doing so)
    if (implicit && Module['noExitRuntime'] && status === 0) {
      return;
    }

    if (Module['noExitRuntime']) {
      // if exit() was called, we may warn the user if the runtime isn't actually being shut down
      if (!implicit) {
        err('exit(' + status + ') called, but EXIT_RUNTIME is not set, so halting execution but not exiting the runtime or preventing further async execution (build with EXIT_RUNTIME=1, if you want a true shutdown)');
      }
    } else {

      ABORT = true;

      exitRuntime();

      if (Module['onExit']) Module['onExit'](status);
    }

    Module['quit'](status, new ExitStatus(status));
  }

  var abortDecorators = [];

  function abort(what) {
    if (Module['onAbort']) {
      Module['onAbort'](what);
    }

    if (what !== undefined) {
      out(what);
      err(what);
      what = JSON.stringify(what);
    } else {
      what = '';
    }

    ABORT = true;

    var extra = '';
    var output = 'abort(' + what + ') at ' + stackTrace() + extra;
    if (abortDecorators) {
      abortDecorators.forEach(function(decorator) {
        output = decorator(output, what);
      });
    }
    throw output;
  }
  Module['abort'] = abort;

  if (Module['preInit']) {
    if (typeof Module['preInit'] == 'function') Module['preInit'] = [Module['preInit']];
    while (Module['preInit'].length > 0) {
      Module['preInit'].pop()();
    }
  }


    Module["noExitRuntime"] = true;

  run();





  // {{MODULE_ADDITIONS}}





    return Module
  }
  );
  })();

  var editor = (function (){
    var elem = $("<div style='width:100%;height:calc( 100% - 10px );'></div>");
    var editor = ace.edit(elem[0], {
      theme: "ace/theme/chrome",
      selectionStyle: "text",
      showPrintMargin: false,
      fontFamily: 'Roboto Mono',
      fontSize:'16'
    });

    var sessions = {};
    var _currentSession;

    var _addSession = function(tab,caption,text){
      sessions[tab] = {};
      sessions[tab].caption = caption;
      sessions[tab].session = ace.createEditSession(text,"ace/mode/dummy");
    };


    return {
      html: elem[0],
      ace: editor,
      addSession: _addSession,
      currentSession: () => {return _currentSession;},
      sessionCaption: (tab) => {return sessions[tab].caption;},
      setSession: (tab) => {_currentSession=tab;editor.setSession(sessions[tab].session);},
      deleteSession: (tab) => {sessions.tab=undefined;}
    }

  })();

  var logger = (function (){
    var elem = $("<div class='logger-container container'>");
    var panel = $("<div class='logger-panel'>");

    elem.append(panel);

    return {
      log: (msg,_top,_bottom) => {
        panel.append($("<div style='margin-bottom:"+_bottom+"px;"+
          "margin-top:"+_top+"px;'>").html(msg));
        elem.scrollTop(panel.height());
      },
      hr: () => {
        panel.append($("<hr class='logger-hr'>"));
      },
      html: elem[0]
    }

  })();

  var iopanel = (function (){
    var top_area = $("<textarea class='iopanel-textarea' placeholder='input...' >");
    var top_panel = $("<div class='iopanel-top iopanel-div'>").append(top_area);

    var bottom_area = $("<textarea readonly class='iopanel-textarea'>output...</textarea>");
    var bottom_panel=$("<div class='iopanel-bottom iopanel-div'>").append(bottom_area);

    var elem = $("<div class='iopanel-container container'>")
      .append(top_panel).append(bottom_panel);

    return {
      html: elem[0],
      input: ()=>{return top_area.val();},
      output: (text)=>{ bottom_area.val(text);}
    }

  })();

  var statusbar = (function (){
    var inner=$("<div id='statusbar-left' class='statusbar-panel'>");
    var elem = $("<div class='statusbar-container'>")
      .append(inner);

    return {
      left: (msg) => {
        inner.html(msg);
      },
      html: elem[0]
    }

  })();

  var form_template = "<div class=\"w2ui-page page-0\">\n    <div class=\"w2ui-field\">\n        <label>####</label>\n        <div>\n           <input name=\"data\" type=\"text\" maxlength=\"100\" style=\"width: 250px\"/>\n        </div>\n    </div>\n</div>\n<div class=\"w2ui-buttons\">\n    <button class=\"w2ui-btn\" name=\"save\">Ok</button>\n</div>\n";

  var getString = (function (){

    function openPopup (_title,_label,_callback) {
      var form = form_template.replace('####',_label);
      if (w2ui['get_string_popup_form'])
        w2ui['get_string_popup_form'].destroy();
      $().w2form({
              name: 'get_string_popup_form',
              style: 'border: 0px; background-color: transparent;',
              formHTML: form,
              fields: [
                  { field: 'data', type: 'text', required: true }
              ],
              actions: {
                  "save": function () { 
                      if (this.validate('showErrors').length>0) return; 
                      w2popup.close();
                      w2ui['get_string_popup_form'].destroy();
                      _callback(this.get('data').el.value);
                  },
              }
      });

      $().w2popup('open', {
          name    : 'get_string_popup',
          title   : _title,
          body    : '<div id="get_string_div" style="width: 100%; height: 100%;"></div>',
          style   : 'padding: 15px 0px 0px 0px',
          width   : 490,
          height  : 180,
              speed: "0",
          showMax : false,
          onOpen: function (event) {
              event.onComplete = function () {
                  $('#w2ui-popup #get_string_div').w2render('get_string_popup_form');
              };
          }
      });

    }


    return openPopup;
  })();

  var factorial_demo = "/*\n\n  simple recursive factorial demo\n\n  input: n\n  output: n!\n\n*/\n\ninput int n;\noutput int x;\n\nint fact(int n) {\n  if (n == 1)\n    return n;\n  else\n    return n * fact(n - 1);\n}\n\nx = fact(n);\n";

  var prefix_sums_recursive_demo = "/*\n\nrecursive prefix sums\n\ninput:  array of integers (length power of 2)\noutput: array of prefix sums\n\nT = O(log n)\nW = O(n)\n\n*/\n\ninput int A[_];\noutput int B[A.size];\n\nint prefix_sums(int A[_], int B[_]) {\n  if (A.size == 1) {\n    B[0] = A[0];\n    return 1;\n  }\n\n  int n = A.size / 2;\n  int AA[n], BB[n];\n\n  pardo(i : n) AA[i] = A[2 * i] + A[2 * i + 1];\n\n  prefix_sums(AA, BB);\n\n  pardo(i : n) {\n    B[2 * i] = BB[i] - A[2 * i + 1];\n    B[2 * i + 1] = BB[i];\n  }\n\n  return 1;\n}\n\nprefix_sums(A, B);\n";

  var sum_logarithmic_demo = "/*\n\nbalanced binary tree computation\n  \ninput:  array of integers (length power of 2)\noutput: sum\n\nT = O(log n)\nW = O(n)\n\n*/\n\ninput int A[_];\nint n = A.size;\nint Y[n];\n\noutput int sum;\n\npardo(i : n) Y[i] = A[i];\n\nfor (int t = 0; 2 ^ t < n; t++)\n  pardo(i : n / 2 ^ (t + 1)) \n    Y[i] = Y[2 * i] + Y[2 * i + 1];\n\nsum = Y[0];\n";

  var prefix_sums_nonrecursive_demo = "/* \n\nnon-recursive prefix sums with additional O(n log n) memory \n\ninput:  array of integers (length power of 2)\noutput: array of prefix sums\n\nT = O(log n)\nW = O(n)\n\n*/\n\ninput int A[_];\nint n = A.size;\n\n// log should be built in\nint ln = 0;\nfor (; 2 ^ ln < n; ln++);\n\nint B[ln + 1, n]; // storage\n\noutput int S[n];\n\n// copy input\npardo(i : n) B[0, i] = A[i];\n\n// bottom-up traversal\nfor (int t = 0; 2 ^ t < n; t++)\n  pardo(i : n / 2 ^ (t + 1)) \n      B[t + 1, i] = B[t, 2 * i] + B[t, 2 * i + 1];\n\n// top-down traversal\nfor (int t = 0; 2 ^ t < n; t++) \n    pardo(i : n / 2 ^ (ln - t)) {\n        B[ln - t - 1, 2 * i] = B[ln - t, i] - B[ln - t - 1, 2 * i + 1];\n        B[ln - t - 1, 2 * i + 1] = B[ln - t, i];\n    } \n\n// write output\npardo(i : n) S[i] = B[0, i];\n";

  var prefix_sums_sequential_demo = "/* \n  \nsequential prefix sums \n\ninput:  array of integers \noutput: array of prefix sums\n\nT = O(n)\n\n*/\n\ninput int A[_];\nint n = A.size;\noutput int S[n];\n\nS[0] = A[0];\n\nfor (int i = 1; i < n; i++) \n    S[i] = S[i - 1] + A[i];\n";

  var sum_sequential_demo = "/*\n\nsimple sequential sum algorithm\n\ninput:  array of integers\noutput: sum\n\n*/\n\ninput int A[_];\noutput int sum = 0;\n\nfor (int i = 0; i < A.size; i++) \n    sum += A[i];\n";

  // import 'core-js';

  const no_program_msg = "no program loaded";

  var demos = {
    demo1: {name:'factorial.wt',code:factorial_demo},
    demo2: {name:'sum_sequential.wt',code:sum_sequential_demo},
    demo3: {name:'sum_logarithmic.wt',code:sum_logarithmic_demo},
    demo4: {name:'prefix_sums_sequential.wt',code:prefix_sums_sequential_demo},
    demo5: {name:'prefix_sums_recursive.wt',code:prefix_sums_recursive_demo},
    demo6: {name:'prefix_sums_nonrecursive.wt',code:prefix_sums_nonrecursive_demo}
  };

  var wt = {};

  editor.addSession("tab1","program.wt","output int x=42;");
  editor.setSession('tab1');

  var tabs=['tab1'];
  var maxtab=1;
  var program_ready=false; // is there a compiled program ready?
  var running = false; // program is running

  // load the backend functions
  Module().then(function(cc) {
    wt.compile       = cc.cwrap('web_compile', 'number', [ 'string','string' ]);
    wt.errnum        = cc.cwrap('errnum', 'number', []);
    wt.get_error_msg = cc.cwrap('get_error_msg', 'string', [ 'number' ]);
    wt.run           = cc.cwrap('web_run', 'number', []);
    wt.start         = cc.cwrap('web_start', 'number', ['string']);
    wt.output        = cc.cwrap('web_output', 'string', []);
    wt.W             = cc.cwrap('web_W','number',[]);
    wt.T             = cc.cwrap('web_T','number',[]);
  });

  function keyDownHandler(e) {
    // console.log(e.code);
    if ((e.which || e.keyCode) == 117) {
      // F6 toggles left panel
      e.preventDefault();
      w2ui['inner_layout_main_toolbar'].click('toggle_io_button');
    }
    /*
    else if ((e.which || e.keyCode) == 118) {
      // F7 toggles right panel
      e.preventDefault();
      w2ui['inner_layout_main_toolbar'].click('toggle_watcher_button');
    }
    */
    else if ((e.which || e.keyCode) == 114) {
      // F3 compiles
      if (!running) {
        e.preventDefault();
        compile();
      }
    }
    else if ((e.which || e.keyCode) == 115) {
      // F4 run
      if (program_ready) {
        e.preventDefault();
        runner.toggle();
      }
    }
  }

  function compile() {
    var name =  editor.sessionCaption(editor.currentSession());

    var res = wt.compile(name,editor.ace.getValue()+" ");
    logger.hr();
    logger.log("compiling <span class='logger-strong'>"+name+"</span>",0,5);
    for (let i = 0; i < wt.errnum(); i++) logger.log(wt.get_error_msg(i));
    if (res==0) {
      logger.log("<span >compilation ok</span>",5,5);
      statusbar.left("program <span class='statusbar-strong'>"+
        name+"</span> ready");
      w2ui['main_layout_left_toolbar'].set('run_button',{disabled:false});
      program_ready=true;
    } else {
      logger.log("<span class='logger-error'>there were errors</span>",5,5);
      w2ui['main_layout_left_toolbar'].set('run_button',{disabled:true});
      statusbar.left(no_program_msg);
      program_ready=false;
    }
  }

  var runner = (function(){
    var shouldStop = false;
    var name;

    var work = function() {
      var res = wt.run(5000);
      if (res && !shouldStop) setTimeout(work,0);
      else {
        running=false;
        shouldStop=false;
        statusbar.left("program <span class='statusbar-strong'>"+
             name+"</span> ready");
        logger.hr();
        w2ui['main_layout_left_toolbar'].set('run_button',{caption:'Run (F4)'});
        w2ui['inner_layout_main_toolbar'].set('compile_button',{disabled:false});
        if (res) {
          logger.log(name+" was interrupted.",0,5);
        }  else {
          iopanel.output(wt.output());
          logger.log("<span class='logger-strong'>"+name+"</span> finished.");
          logger.log("work = "+wt.W()+" <span style='padding-left:20px'>&nbsp;</span>  time = "
            +wt.T(),0,5);
        }
      }
    };

    return {
      toggle: function() {
        if (running) {
          shouldStop=true;
        } else {
          // not runnning - start
          name =  editor.sessionCaption(editor.currentSession());
          wt.start(iopanel.input());
          running = true;
          iopanel.output("");
          statusbar.left("program <span class='statusbar-strong'>"+
              name+"</span> running");
          w2ui['main_layout_left_toolbar'].set('run_button',{caption:'Stop (F4)'});
          w2ui['inner_layout_main_toolbar'].set('compile_button',{disabled:true});
          setTimeout(work,0);
        }
      }
    }
  })();



  function addTab(name) {
    editor.addSession("tab"+(++maxtab),name," ");
    w2ui['inner_layout_main_tabs'].add({id:"tab"+maxtab,text:name,closable:true});
    tabs.push('tab'+maxtab);
  }

  function addDemo(id) {
    editor.addSession("tab"+(++maxtab),demos[id].name,demos[id].code);
    w2ui['inner_layout_main_tabs'].add({id:"tab"+maxtab,text:demos[id].name,closable:true});
    tabs.push('tab'+maxtab);
  }

  function closeTab(event) {
    if (tabs.length==1) event.preventDefault();
    else {
      let killactive = editor.currentSession()==event.target;
      editor.deleteSession(event.target);
      for (let i=0;i<tabs.length;i++) if (tabs[i]==event.target) {
        if (killactive) {
          var nt;
          if (i>0) nt=tabs[i-1];
          else nt=tabs[i+1];
          event.onComplete=()=>w2ui['inner_layout_main_tabs'].click(nt);
        }
        tabs.splice(i,1);
        break;
      }
    }
  }

  function changeTab(event) {
    editor.setSession(event.target);
  }


  $().w2layout({
    name:'inner_layout',
    resizer:8,
     panels:[
      { 
        type: 'main', 
        toolbar: {
          items: [
            { type: 'menu', text:'Demos',
              items: [
                {id:'demo1',text:'factorial'},
                {id:'demo2',text:'sum (sequential)'},
                {id:'demo3',text:'sum (logarithmic)'},
                {id:'demo4',text:'prefix sums (sequential)'},
                {id:'demo5',text:'prefix sums (recursive)'},
                {id:'demo6',text:'prefix sums (non-recursive)'}
              ]
            },
            { type: 'button',  caption: 'New', onClick: ()=>getString("title","name",addTab)},
            { type: 'button',  id:'compile_button', caption: 'Compile (F3)', onClick: compile },
            { type: 'spacer' },
            { type: 'check', id: 'toggle_io_button', caption: 'I/O (F6)', checked:true,
                onClick: function(){ w2ui['main_layout'].toggle('left',true); }
            }
            /*,
            { type: 'check', id: 'toggle_watcher_button', caption: 'Watcher (F7)',checked:false,
                onClick: function(){ w2ui['main_layout'].toggle('right',true);}
            }
            */
          ],
          onClick: (event) => {
            if (event.subItem) 
              addDemo(event.subItem.id);
          }

        },
        tabs: {
          active: 'tab1',
          tabs: [
              { id: 'tab1', closable:true, caption: editor.sessionCaption('tab1') },
          ],
          onClick: changeTab,
          onClose: closeTab
        },
        resizable:true,
        content: editor.html
      },
      {
        type:'bottom', 
        resizable:true, 
        size:'120', 
        content: logger.html
      }
    ]
  });
                              
  $().w2layout({
    name: 'main_layout',
    resizer:8,
    panels: [
      { 
        type: 'left', 
        toolbar: {
          items: [
            { type: 'button',  id:'run_button', caption: 'Run (F4)', 
              onClick: runner.toggle, disabled:true },
          ]
        },
        resizable:true,
        size:'30%',
        content: iopanel.html
      },
      { type:'main', resizable:true },
      {
        type:'right', 
        resizable:true, 
        size:'30%', 
        hidden:true,
        content: 'nothing here yet'
      },
      {
        type:'bottom',
        resizable:false,
        size:'28',
        content:statusbar.html
      }
    ],
    onResize: function(event) {
        if (editor) event.onComplete = () => { editor.ace.resize(true); };
    }
  });

  w2ui['main_layout'].content('main',w2ui['inner_layout']);


  $(function () {
    document.addEventListener('keydown', keyDownHandler);
    $("#app-layout").w2render('main_layout');
    statusbar.left(no_program_msg);


  });

}());
