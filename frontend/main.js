/*! @file main.js
 *
 *  @brief entry point
 *
 */
// import 'core-js';

import './frontend.scss'

import backend from './backend.js'
// demos
import factorial_demo from './demos/factorial.txt'
import first_1_best_demo from './demos/first_1_crcw_best.txt'
import first_1_simple_demo from './demos/first_1_crcw_simple.txt'
import prefix_sums_nonrecursive_demo from './demos/prefix_sums_nonrecursive.txt'
import prefix_sums_recursive_demo from './demos/prefix_sums_recursive.txt'
import prefix_sums_sequential_demo from './demos/prefix_sums_sequential.txt'
import sum_logarithmic_demo from './demos/sum_logarithmic.txt'
import sum_sequential_demo from './demos/sum_sequential.txt'
import editor from './editor.js'
import getString from './get_string.js'
import iopanel from './iopanel.js'
import logger from './logger.js'
import statusbar from './statusbar.js'
import watcher from './watcher.js'

const no_program_msg = "no program loaded"

var demos = {
  demo1 : {name : 'factorial.wt', code : factorial_demo},
  demo2 : {name : 'sum_sequential.wt', code : sum_sequential_demo},
  demo3 : {name : 'sum_logarithmic.wt', code : sum_logarithmic_demo},
  demo4 :
      {name : 'prefix_sums_sequential.wt', code : prefix_sums_sequential_demo},
  demo5 :
      {name : 'prefix_sums_recursive.wt', code : prefix_sums_recursive_demo},
  demo6 : {
    name : 'prefix_sums_nonrecursive.wt',
    code : prefix_sums_nonrecursive_demo
  },
  demo7 : {name : 'first_one_simple.wt', code : first_1_simple_demo},
  demo8 : {name : 'first_one_optimal.wt', code : first_1_best_demo}
}

var wt = {}

var tabs = [ {id : 'tab1'} ];  // id, mID, marked
var maxtab = 1;

function check_mark() {
  var i;
  for (i = 0; i < tabs.length; i++)
    if (tabs[i].id == w2ui['inner_layout_main_tabs'].active) break;
  if (tabs[i].marked === undefined && tabs[i].mID !== undefined) {
    editor.ace.session.removeMarker(tabs[i].mID);
    tabs[i].mID = undefined;
  }
}

function unmark() {
  for (var i = 0; i < tabs.length; i++) tabs[i].marked = undefined;
  check_mark();
}

function editorChanged(delta) {
  var i;
  for (i = 0; i < tabs.length; i++)
    if (tabs[i].id == w2ui['inner_layout_main_tabs'].active) break;
  w2ui['inner_layout_main_tabs'].set(
      tabs[i].id, {caption : '* ' + editor.sessionCaption(tabs[i].id)});
}

editor.addSession("tab1", "program.wt", "output int x=42;", editorChanged);
editor.setSession('tab1');

// load the backend functions
backend().then(function(cc) {
  wt.get = cc.getValue;
  wt.compile = cc.cwrap('web_compile', 'number', [ 'string', 'string' ]);
  wt.errnum = cc.cwrap('errnum', 'number', []);
  wt.get_error_msg = cc.cwrap('get_error_msg', 'string', [ 'number' ]);
  wt.run = cc.cwrap('web_run', 'number', [ 'number', 'number' ]);
  wt.start = cc.cwrap('web_start', 'number', [ 'string' ]);
  wt.stop = cc.cwrap('web_stop', 'number', []);
  wt.output = cc.cwrap('web_output', 'string', []);
  wt.W = cc.cwrap('web_W', 'number', []);
  wt.T = cc.cwrap('web_T', 'number', []);
  wt.state = cc.cwrap('web_state', 'number', []);
  wt.name = cc.cwrap('web_name', 'string', []);
  wt.tids = cc.cwrap('web_tids', 'number', []);
  wt.n_thr = cc.cwrap('web_n_threads', 'number', []);
  wt.thread_parent = cc.cwrap('web_thread_parent', 'number', [ 'number' ]);
  wt.thread_base_name = cc.cwrap('web_thread_base_name', 'string', []);
  wt.thread_base_value =
      cc.cwrap('web_thread_base_value', 'number', [ 'number' ]);
  wt.prepare_vars = cc.cwrap('web_prepare_vars', 'number', [ 'number' ]);
  wt.var_shared = cc.cwrap('web_var_shared', 'number', [ 'number' ]);
  wt.var_type = cc.cwrap('web_var_type', 'string', [ 'number' ]);
  wt.var_name = cc.cwrap('web_var_name', 'string', [ 'number' ]);
  wt.var_dims = cc.cwrap('web_var_dims', 'string', [ 'number' ]);
  wt.var_value = cc.cwrap('web_var_value', 'string', [ 'number' ]);
  wt.current_line = cc.cwrap('web_current_line', 'number', []);
});

function format(wt) {
  var n = Number(wt);
  if (n < 1000) return wt;
  if (n < 1e6) return n / 1e3 + "K";
  if (n < 1e9) return n / 1e6 + "M";
  if (n < 1e12) return n / 1e9 + "G";
  return n / 1e12 + "T";
}

function handle(e, code, button) {
  if ((e.which || e.keyCode) == code) {
    e.preventDefault();
    if (!w2ui['inner_layout_main_toolbar'].get(button + '_button').disabled) {
      w2ui['inner_layout_main_toolbar'].click(button + '_button');
    }
  }
}

function keyDownHandler(e) {
  // console.log(e.code);
  handle(e, 117, 'toggle_io');
  handle(e, 118, 'toggle_watcher');
  handle(e, 114, 'compile');
  handle(e, 115, 'run');
  handle(e, 119, 'debug');
}

function compile() {
  unmark();
  var tab = editor.currentSession();
  var name = editor.sessionCaption(tab);
  w2ui['inner_layout_main_tabs'].set(tab,
                                     {caption : editor.sessionCaption(tab)});

  var res = wt.compile(name, editor.ace.getValue() + " ");
  logger.hr();
  logger.log("compiling <span class='logger-strong'>" + name + "</span>", 0, 5);
  for (let i = 0; i < wt.errnum(); i++) logger.log(wt.get_error_msg(i));
  if (res == 0) {
    logger.log("<span >compilation ok</span>", 5, 5);
    statusbar.left("program <span class='statusbar-strong'>" + name +
                   "</span> ready");
  } else {
    logger.log("<span class='logger-error'>there were errors</span>", 5, 5);
    statusbar.left(no_program_msg);
  }
  update_state();
  watcher.set_threads([]);
}

function set_watcher_threads() {
  var tids = wt.tids();
  var data = [];
  for (var i = 0; i < wt.n_thr(); i++) {
    var tid = wt.get(tids + 8 * i, 'i64');
    var par = wt.thread_parent(tid);
    var vv = ' ';
    if (par != 0)
      vv = wt.thread_base_name() + " = " + wt.thread_base_value(tid);
    data.push({'recid' : i + 1, 'tid' : tid, 'parent' : par, 'var' : vv});
  }
  watcher.set_threads(data);
}

function set_watcher_vars(tid) {
  var data = [];
  var n = wt.prepare_vars(tid);
  for (var i = 0; i < n; i++) {
    var span;
    if (wt.var_shared(i))
      span = '<span class="watcher-shared">'
      else span = '<span class="watcher-local">';
    var sspan = "</span>";

    data.push({
      'recid' : i + 1,
      'type' : span + wt.var_type(i) + sspan,
      'name' : span + wt.var_name(i) + sspan,
      'dims' : span + wt.var_dims(i) + sspan,
      'value' : span + wt.var_value(i) + sspan
    });
  }
  return data;
}

var runner = (function() {
  var shouldStop = false;
  var running = false;  // some code is executing
  var stop_on_bp;

  var work =
      function() {
    var res = wt.run(5000, stop_on_bp);
    if (res == 0 && !shouldStop) {
      statusbar.left(
          "program <span class='statusbar-strong'>" + wt.name() +
          "</span> running" +
          " <div style='display:inline-block;min-width:200px;padding-left:10px'>work = " +
          "<span style='font-family:Roboto;'>" + format(wt.W()) + "</span>" +
          " </div><span style='padding-left:10px'>&nbsp;</span>  time = " +
          "<span style='font-family:Roboto;'>" + format(wt.T()) + "</span>");
      setTimeout(work, 0);
    } else {
      running = false;
      shouldStop = false;
      update_state();
      set_watcher_threads();
      if (wt.state() != 1)
        statusbar.left("program <span class='statusbar-strong'>" + wt.name() +
                       "</span> ready");
      if (res == 0) {
        logger.log("program was interrupted", 0, 5);
      } else if (res < -1) {
        for (let i = 0; i < wt.errnum(); i++) logger.log(wt.get_error_msg(i));
        logger.log("<span class='logger-bad'>program crashed.</span>");
      } else if (res > 0) {
        logger.log("breakpoint " + res + " hit.");
        var l = wt.current_line() - 1;
        var i;
        for (i = 0; i < tabs.length; i++)
          if (tabs[i].id == w2ui['inner_layout_main_tabs'].active) break;
        tabs[i].mID = editor.ace.session.addMarker(
            new ace.Range(l, 0, l, 2000), "highlight-line", "line", false);
        tabs[i].marked = true;
      } else {
        iopanel.output(wt.output());
        logger.log(
            "finished. <span style='padding-left:10px'>&nbsp;</span>work = " +
                format(wt.W()) +
                " <span style='padding-left:10px'>&nbsp;</span>  time = " +
                format(wt.T()),
            0, 5);
      }
    }
  }

  var toggle =
      function(bp) {
    stop_on_bp = bp;
    if (running) {
      shouldStop = true;
    }
    // not runnning - start
    else if (!bp || wt.state() != 1) {
      unmark();
      iopanel.output(" ");
      wt.start(iopanel.input());
      if (wt.errnum() > 0) {
        logger.hr();
        logger.log("failed to run program <span class='statusbar-strong'>" +
                   wt.name() + "</span>");
        for (let i = 0; i < wt.errnum(); i++) logger.log(wt.get_error_msg(i));
      } else {
        running = true;
        iopanel.output("");
        statusbar.left("program <span class='statusbar-strong'>" + wt.name() +
                       "</span> running");
        logger.hr();
        logger.log("running program <span class='statusbar-strong'>" +
                   wt.name() + "</span>");
        update_state();
        setTimeout(work, 0);
      }
    } else {
      running = true;
      update_state();
      setTimeout(work, 0);
    }
  }

  return {
    active: function() { return running; },
        toggle_run: () => toggle(0), toggle_debug: () => toggle(1)
  }
})();

function update_state() {
  if (runner.active()) {
    w2ui['inner_layout_main_toolbar'].set('compile_button', {disabled : true});
    w2ui['inner_layout_main_toolbar'].set(
        'run_button', {caption : 'Stop (F4)', disabled : false});
    w2ui['inner_layout_main_toolbar'].set('debug_button', {disabled : true});
  } else {
    w2ui['inner_layout_main_toolbar'].set('compile_button', {disabled : false});
    w2ui['inner_layout_main_toolbar'].set('run_button', {caption : 'Run (F4)'});
    // WEB_NO_CODE
    if (wt.state() == -1) {
      w2ui['inner_layout_main_toolbar'].set('run_button', {disabled : true});
      w2ui['inner_layout_main_toolbar'].set('debug_button', {disabled : true});
      watcher.set_threads([]);
    } else {
      w2ui['inner_layout_main_toolbar'].set('run_button', {disabled : false});
      w2ui['inner_layout_main_toolbar'].set('debug_button', {disabled : false});
    }
  }
}

function addTab(name) {
  editor.addSession("tab" + (++maxtab), name, " ", editorChanged);
  w2ui['inner_layout_main_tabs'].add(
      {id : "tab" + maxtab, text : name, closable : true});
  tabs.push({id : 'tab' + maxtab});
}

function addDemo(id) {
  editor.addSession("tab" + (++maxtab), demos[id].name, demos[id].code,
                    editorChanged);
  w2ui['inner_layout_main_tabs'].add(
      {id : "tab" + maxtab, text : demos[id].name, closable : true});
  tabs.push({id : 'tab' + maxtab});
}

function closeTab(event) {
  if (tabs.length == 1)
    event.preventDefault();
  else {
    let killactive = editor.currentSession() == event.target;
    editor.deleteSession(event.target);
    for (let i = 0; i < tabs.length; i++)
      if (tabs[i].id == event.target) {
        if (killactive) {
          var nt;
          if (i > 0)
            nt = tabs[i - 1].id;
          else
            nt = tabs[i + 1].id;
          event.onComplete = () => w2ui['inner_layout_main_tabs'].click(nt);
        }
        tabs.splice(i, 1);
        break;
      }
  }
}

function changeTab(event) { 
  editor.setSession(event.target); 
  event.onComplete = check_mark;
}

$().w2layout({
  name : 'inner_layout',
  resizer : 8,
  panels : [
    {
      type : 'main',
      toolbar : {
        items : [
          {
            type : 'menu',
            text : 'Demos',
            items : [
              {id : 'demo1', text : 'factorial'},
              {id : 'demo2', text : 'sum (sequential)'},
              {id : 'demo3', text : 'sum (logarithmic)'},
              {id : 'demo4', text : 'prefix sums (sequential)'},
              {id : 'demo5', text : 'prefix sums (recursive)'},
              {id : 'demo6', text : 'prefix sums (non-recursive)'},
              {id : 'demo7', text : 'index of first 1 on cCRCW (simple)'},
              {id : 'demo8', text : 'index of first 1 on cCRCW (optimal)'}
            ]
          },
          {
            type : 'button',
            caption : 'New',
            onClick : () => getString("title", "name", addTab)
          },
          {
            type : 'button',
            id : 'compile_button',
            caption : 'Compile (F3)',
            onClick : compile
          },
          {
            type : 'button',
            id : 'run_button',
            caption : 'Run (F4)',
            disabled : true,
            onClick : runner.toggle_run
          },
          {
            type : 'button',
            id : 'debug_button',
            caption : 'Debug (F8)',
            disabled : true,
            onClick : runner.toggle_debug
          },
          {type : 'spacer'}, {
            type : 'check',
            id : 'toggle_io_button',
            caption : 'I/O (F6)',
            checked : true,
            onClick : function() { w2ui['main_layout'].toggle('left', true); }
          },
          {
            type : 'check',
            id : 'toggle_watcher_button',
            caption : 'Watcher (F7)',
            checked : false,
            onClick : function() { w2ui['main_layout'].toggle('right', true); }
          }
        ],
        onClick : (event) => {
          if (event.subItem) addDemo(event.subItem.id);
        }

      },
      tabs : {
        active : 'tab1',
        tabs : [
          {
            id : 'tab1',
            closable : true,
            caption : editor.sessionCaption('tab1')
          },
        ],
        onClick : changeTab,
        onClose : closeTab
      },
      resizable : true,
      content : editor.html
    },
    {type : 'bottom', resizable : true, size : '120', content : logger.html}
  ],
  onResize : function(event) {
    if (editor) event.onComplete = () => { editor.ace.resize(true); };
  }
});

$().w2layout({
  name : 'main_layout',
  resizer : 8,
  panels : [
    {type : 'left', resizable : true, size : '30%', content : iopanel.html},
    {type : 'main', resizable : true}, {
      type : 'right',
      resizable : true,
      size : '30%',
      hidden : true,
      content : watcher.html
    },
    {type : 'bottom', resizable : false, size : '28', content : statusbar.html}
  ]
});

w2ui['main_layout'].content('main', w2ui['inner_layout']);

$(function() {
  document.addEventListener('keydown', keyDownHandler);
  editor.ace.session.on('change', editorChanged);
  $("#app-layout").w2render('main_layout');
  statusbar.left(no_program_msg);
  watcher.init(set_watcher_vars);
  /*
  var m = editor.ace.session.addMarker(new ace.Range(0,0,1,20), "quak", "line",
  true);
  //editor.ace.session.setBreakpoint(0);
  editor.ace.session.addGutterDecoration(0,'quuk');
  */
});
