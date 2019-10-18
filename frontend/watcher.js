export default (function (){
  var set_vars;
  var current_tid;

  var top_panel = $("<div class='watcher-top watcher-div'>");
  $().w2grid(
    {
      name:'threads_grid', header:'active threads',
      show : {
        header : true
      },
     columns: [
        { field: 'tid', caption: 'tid',sortable: true, resizable: true, size:'30%' },
        { field: 'parent', caption: 'parent',size:'30%',sortable: true, resizable: true },
        { field: 'var', caption: 'variable',size:'40%',sortable: true, resizable: true}
     ],
      onClick:function(event) {
        _set_vars(this.get(event.recid).tid);
      }
    }
  );
  var bottom_panel=$("<div class='watcher-bottom watcher-div'>");
  $().w2grid({
    name:'variables_grid', header:'variables',
      show : {
        header : true
      },
    columns: [
      {field: 'type', caption:'type', size:'80px',sortable: true, resizable: true},
      {field: 'name', caption:'name', size:'80px',sortable: true, resizable: true},
      {field: 'dims', caption:'size', size:'80px',sortable: true, resizable: true},
      {field: 'value', caption:'value', size:'100%',sortable: true, resizable: true}
    ]
  });

  var _set_vars = function(tid) {
        current_tid = tid;
        var data=set_vars(current_tid);
        w2ui['variables_grid'].header='variables in thread '+tid;
        w2ui['variables_grid'].records=data;
        w2ui['variables_grid'].refresh();
  };


  var elem = $("<div class='container'>")
    .append(top_panel).append(bottom_panel);

  var _set_threads = function(data) {
    w2ui['threads_grid'].records=data;
    w2ui['threads_grid'].refresh();

    if (data.length>0)
    _set_vars(data[0].tid);
    else {
        w2ui['variables_grid'].header='variables';
        w2ui['variables_grid'].records=[];
        w2ui['variables_grid'].refresh();
    }
    
  }


  return {
    html: elem[0],
    init: (callback)=>{
      w2ui['threads_grid'].render($(".watcher-top")[0]);
      w2ui['variables_grid'].render($(".watcher-bottom")[0]);
      set_vars=callback},
    set_threads: _set_threads
  }

})();


