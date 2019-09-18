/*!
 * @file editor.js
 *
 * huh
 */
export default (function (){
  var elem = $("<div style='width:100%;height:calc( 100% - 10px );'></div>");
  var editor = ace.edit(elem[0], {
    theme: "ace/theme/chrome",
    selectionStyle: "text",
    showPrintMargin: false,
    fontFamily: 'Roboto Mono',
    fontSize:'16'
  });

  var sessions = {}
  var _currentSession;

  var _addSession = function(tab,caption,text){
    sessions[tab] = {}
    sessions[tab].caption = caption;
    sessions[tab].session = ace.createEditSession(text,"ace/mode/dummy");
  }


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

