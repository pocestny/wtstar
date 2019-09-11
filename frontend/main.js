// import 'core-js';

import './frontend.scss'

import backend from './backend.js'
import editor from './editor.js'
import logger from './logger.js'
import iopanel from './iopanel.js'
import statusbar from './statusbar.js'
import getString from './get_string.js'

// demos
import factorial_demo from './demos/factorial.txt'
import prefix_sums_recursive_demo from './demos/prefix_sums_recursive.txt'   
import sum_logarithmic_demo from './demos/sum_logarithmic.txt'
import prefix_sums_nonrecursive_demo from './demos/prefix_sums_nonrecursive.txt'  
import prefix_sums_sequential_demo from './demos/prefix_sums_sequential.txt' 
import sum_sequential_demo from './demos/sum_sequential.txt'

const no_program_msg = "no program loaded"

var demos = {
  demo1: {name:'factorial.wt',code:factorial_demo},
  demo2: {name:'sum_sequential.wt',code:sum_sequential_demo},
  demo3: {name:'sum_logarithmic.wt',code:sum_logarithmic_demo},
  demo4: {name:'prefix_sums_sequential.wt',code:prefix_sums_sequential_demo},
  demo5: {name:'prefix_sums_recursive.wt',code:prefix_sums_recursive_demo},
  demo6: {name:'prefix_sums_nonrecursive.wt',code:prefix_sums_nonrecursive_demo}
}

var wt = {}

editor.addSession("tab1","program.wt","output int x=42;");
editor.setSession('tab1');

var tabs=['tab1'];
var maxtab=1;
var program_ready=false; // is there a compiled program ready?
var running = false; // program is running

// load the backend functions
backend().then(function(cc) {
  wt.compile       = cc.cwrap('web_compile', 'number', [ 'string','string' ]);
  wt.errnum        = cc.cwrap('errnum', 'number', []);
  wt.get_error_msg = cc.cwrap('get_error_msg', 'string', [ 'number' ]);
  wt.run           = cc.cwrap('web_run', 'number', []);
  wt.start         = cc.cwrap('web_start', 'number', ['string']);
  wt.output        = cc.cwrap('web_output', 'string', []);
  wt.W             = cc.cwrap('web_W','number',[]);
  wt.T             = cc.cwrap('web_T','number',[]);
});

function format(wt) {
  var n = Number(wt);
  if (n<1000) return wt;
  if (n<1e6) return n/1e3+"K";
  if (n<1e9) return n/1e6+"M";
  if (n<1e12) return n/1e9+"G";
  return n/1e12+"T";
}

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
    w2ui['inner_layout_main_toolbar'].set('run_button',{disabled:false});
    program_ready=true;
  } else {
    logger.log("<span class='logger-error'>there were errors</span>",5,5);
    w2ui['inner_layout_main_toolbar'].set('run_button',{disabled:true});
    statusbar.left(no_program_msg);
    program_ready=false;
  }
}

var runner = (function(){
  var shouldStop = false;
  var name;

  var work = function() {
    var res = wt.run(5000);
    if (res == 1 && !shouldStop) {
       statusbar.left("program <span class='statusbar-strong'>"+
          name+"</span> running"
          + " <div style='display:inline-block;min-width:200px;padding-left:10px'>work = "+
          "<span style='font-family:Roboto;'>"+format(wt.W())+"</span>"
          +" </div><span style='padding-left:10px'>&nbsp;</span>  time = "
          +"<span style='font-family:Roboto;'>"+format(wt.T())+"</span>");
      setTimeout(work,0);
    } else {
      running=false;
      shouldStop=false;
      statusbar.left("program <span class='statusbar-strong'>"+
           name+"</span> ready");
      w2ui['inner_layout_main_toolbar'].set('run_button',{caption:'Run (F4)'});
      w2ui['inner_layout_main_toolbar'].set('compile_button',{disabled:false});
      if (res==1) {
        logger.log("program was interrupted",0,5);
      } else if (res<0) {  
        for (let i = 0; i < wt.errnum(); i++) logger.log(wt.get_error_msg(i));
        logger.log("<span class='logger-bad'>program crashed.</span>");
      } else {
        iopanel.output(wt.output());
        logger.log("finished. <span style='padding-left:10px'>&nbsp;</span>work = "
          +format(wt.W())
          +" <span style='padding-left:10px'>&nbsp;</span>  time = "
          +format(wt.T()),0,5);
      }
    }
  }

  return {
    toggle: function() {
      if (running) {
        shouldStop=true;
      } else {
        // not runnning - start
        name =  editor.sessionCaption(editor.currentSession());
        iopanel.output(" ");
        wt.start(iopanel.input());
        if (wt.errnum()>0) {
          logger.hr();
          logger.log("failed to run program <span class='statusbar-strong'>"+
            name+"</span>");
          for (let i = 0; i < wt.errnum(); i++) logger.log(wt.get_error_msg(i));
        } else {
          running = true;
          iopanel.output("");
          statusbar.left("program <span class='statusbar-strong'>"+
            name+"</span> running");
          logger.hr();
          logger.log("running program <span class='statusbar-strong'>"+
            name+"</span>");

          w2ui['inner_layout_main_toolbar'].set('run_button',{caption:'Stop (F4)'});
          w2ui['inner_layout_main_toolbar'].set('compile_button',{disabled:true});
          setTimeout(work,0);
        }
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
          { type: 'button',  id:'run_button', caption: 'Run (F4)', 
            onClick: runner.toggle, disabled:true },
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
  ],
  onResize: function(event) {
      if (editor) event.onComplete = () => { editor.ace.resize(true); };
  }
});
                            
$().w2layout({
  name: 'main_layout',
  resizer:8,
  panels: [
    { 
      type: 'left', 
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
  ]
});

w2ui['main_layout'].content('main',w2ui['inner_layout']);


$(function () {
  document.addEventListener('keydown', keyDownHandler);
  $("#app-layout").w2render('main_layout');
  statusbar.left(no_program_msg);


});
