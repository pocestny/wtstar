import form_template from './get_string.form.txt'

export default (function (){

  function openPopup (_title,_label,_callback) {
    var form = form_template.replace('####',_label);
    if (w2ui['get_string_popup_form'])
      w2ui['get_string_popup_form'].destroy();
    $().w2form({
            name: 'get_string_popup_form',
            style: 'border: 0px; background-color: transparent;',
            formHTML: form,
            fields: [
                { field: 'data', type: 'text', required: true }
            ],
            actions: {
                "save": function () { 
                    if (this.validate('showErrors').length>0) return; 
                    w2popup.close();
                    w2ui['get_string_popup_form'].destroy();
                    _callback(this.get('data').el.value);
                },
            }
    });

    $().w2popup('open', {
        name    : 'get_string_popup',
        title   : _title,
        body    : '<div id="get_string_div" style="width: 100%; height: 100%;"></div>',
        style   : 'padding: 15px 0px 0px 0px',
        width   : 490,
        height  : 180,
            speed: "0",
        showMax : false,
        onOpen: function (event) {
            event.onComplete = function () {
                $('#w2ui-popup #get_string_div').w2render('get_string_popup_form');
            }
        }
    });

  }


  return openPopup;
})();


