export default (function (){
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


