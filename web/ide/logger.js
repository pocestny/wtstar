export default (function (){
  var elem = $("<div class='logger-container container'>")
  var panel = $("<div class='logger-panel'>")

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


