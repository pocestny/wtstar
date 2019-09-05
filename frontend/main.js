// import 'core-js';

import './index.scss'

import backend from './backend.js'
import editor from './editor.js'
import logger from './logger.js'
import iopanel from './iopanel.js'
import getString from './get_string.js'


// demos
import factorial_demo from './demos/factorial.txt'
import prefix_sums_recursive_demo from './demos/prefix_sums_recursive.txt'   
import sum_logarithmic_demo from './demos/sum_logarithmic.txt'
import prefix_sums_nonrecursive_demo from './demos/prefix_sums_nonrecursive.txt'  
import prefix_sums_sequential_demo from './demos/prefix_sums_sequential.txt' 
import sum_sequential_demo from './demos/sum_sequential.txt'

var demos = {
  demo1: {name:'factorial.wt',code:factorial_demo},
  demo2: {name:'sum_sequential.wt',code:sum_sequential_demo},
  demo3: {name:'sum_logarithmic.wt',code:sum_logarithmic_demo},
  demo4: {name:'prefix_sums_sequential.wt',code:prefix_sums_sequential_demo},
  demo5: {name:'prefix_sums_recursive.wt',code:prefix_sums_recursive_demo},
  demo6: {name:'prefix_sums_nonrecursive.wt',code:prefix_sums_nonrecursive_demo}
}

var wt = {}

editor.addSession("tab1","program.wt","");
editor.setSession('tab1');
var tabs=['tab1'];
var maxtab=1;

// load the backend functions
backend().then(function(cc) {
  wt.compile       = cc.cwrap('web_compile', 'number', [ 'string' ]);
  wt.errnum        = cc.cwrap('errnum', 'number', []);
  wt.get_error_msg = cc.cwrap('get_error_msg', 'string', [ 'number' ]);
  wt.set_input     = cc.cwrap('web_set_input', 'number', [ 'string' ]);
  wt.run           = cc.cwrap('web_run', 'number', []);
  wt.output        = cc.cwrap('web_output', 'string', []);
});

function keyDownHandler(e) {
  // console.log(e.code);
  if ((e.which || e.keyCode) == 117) {
    // F6 toggles left panel
    e.preventDefault();
    w2ui['inner_layout_main_toolbar'].click('toggle_io_button');
  }
  else if ((e.which || e.keyCode) == 118) {
    // F7 toggles left panel
    e.preventDefault();
    w2ui['inner_layout_main_toolbar'].click('toggle_watcher_button');
  }
  else if ((e.which || e.keyCode) == 114) {
    // F3 toggles left panel
    e.preventDefault();
    compile();
  }
}

function compile() {
  var res = wt.compile(editor.ace.getValue());
  for (let i = 0; i < wt.errnum(); i++) logger.log(wt.get_error_msg(i));
  if (res==0) {
    w2ui['main_layout_left_toolbar'].set('current_program_name',
      {html:editor.sessionCaption(editor.currentSession())});
    w2ui['main_layout_left_toolbar'].set('run_button',{disabled:false});
  } else {
    w2ui['main_layout_left_toolbar'].set('current_program_name',
      {html:'---'});
    w2ui['main_layout_left_toolbar'].set('run_button',{disabled:true});
  }
}

function run() {
  wt.set_input(iopanel.input());
  wt.run();
  iopanel.output(wt.output());
}

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
    editor.deleteSession(event.target);
    for (let i=0;i<tabs.length;i++) if (tabs[i]==event.target) {
      var nt;
      if (i>0) nt=tabs[i-1];
      else nt=tabs[i+1];
      event.onComplete=()=>w2ui['inner_layout_main_tabs'].click(nt);
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
          { type: 'button',  caption: 'Add', onClick: ()=>getString("title","name",addTab)},
          { type: 'button',  caption: 'Compile (F3)', onClick: compile },
          { type: 'spacer' },
          { type: 'check', id: 'toggle_io_button', caption: 'I/O (F6)', checked:true,
              onClick: function(){ w2ui['main_layout'].toggle('left',true); }
          },
          { type: 'check', id: 'toggle_watcher_button', caption: 'Watcher (F7)',checked:false,
              onClick: function(){ w2ui['main_layout'].toggle('right',true);}
          }
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
  panels: [
    { 
      type: 'left', 
      toolbar: {
        items: [
          { type: 'button',  id:'run_button', caption: 'Run', onClick: run, disabled:true },
          { type: 'spacer'},
          {type:'html', id:"current_program_name", html:'---'}
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
});
