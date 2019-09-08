export default (function (){
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


