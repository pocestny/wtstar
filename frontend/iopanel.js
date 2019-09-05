export default (function (){
  var elem = $("<div id='iopanel-div'>")
    .append("<div id='io-top' style='width:100%;height:50%; position:absolute; top:0; left:0'><textarea id='input_text' style='width:100%;height:100%; resize:none'/>   </div>")
   .append("<div id='io-bottom' style='width:100%;height:50%; position:absolute; top:50%; left:0'><textarea readonly id='output_text' style='width:100%;height:100%; resize:none'/> </div>");
  

  return {
    html: elem[0],
    input: ()=>{return $('#input_text').val();},
    output: (text)=>{ $('#output_text').val(text);}
  }

})();


