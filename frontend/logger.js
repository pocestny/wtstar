export default (function (){
  var elem = $("<div id='logger-div'>").addClass("logger-panel");

  return {
    log: (msg) => {
      elem.append($("<div>").append(document.createTextNode(msg)));
    },
    html: elem[0]
  }

})();


